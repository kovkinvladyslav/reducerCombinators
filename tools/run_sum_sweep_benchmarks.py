#!/usr/bin/env python3
"""Run sum-size sweep benchmarks for the reducer and HVM2.

The script is intentionally conservative: it stops increasing the sum size after
the sequential reducer benchmark fails. HVM2 is then run only for reducer-proven
sum sizes and only for thread counts that have a corresponding successful
reducer lane result.

In CSV files, N means the number of terms in the expression:
0 + 1 + ... + (N - 1).
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import re
import shutil
import stat
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "docs" / "results" / "raw"
BUILD = ROOT / "build" / "sum_sweep"
VITIS_RUN = Path(r"C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat")
VPP = Path(r"C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat")
DEFAULT_SUMS = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192]
DEFAULT_LANES = [2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]
DEFAULT_HVM_THREADS = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024]
U24_MODULUS = 1 << 24
DEFAULT_HLS_RETRY_COUNT = 2
EXPECTED_CSIM_KEYS = {
    "root_value",
    "result_value",
    "active_pair_count",
    "performed_steps",
    "status_code",
    "call_count",
    "operate_count",
    "parallel_cycle_count",
    "multi_commit_cycle_count",
    "performed_void_count",
    "maximum_global_queue_count",
    "max_queue_count",
    "max_node_count",
    "max_variable_count",
}
VITIS_PROCESS_IMAGES = [
    "vitis-run.exe",
    "vitis_hls.exe",
    "xelab.exe",
    "xsim.exe",
    "xvlog.exe",
    "xvhdl.exe",
]


def try_command(command: list[str], cwd: Path = ROOT, timeout: int = 120) -> subprocess.CompletedProcess[str]:
    print("+", " ".join(map(str, command)))
    command_text = [str(part) for part in command]
    popen_kwargs: dict[str, object] = {
        "cwd": str(cwd),
        "text": True,
        "stdout": subprocess.PIPE,
        "stderr": subprocess.PIPE,
    }
    if os.name != "nt":
        popen_kwargs["start_new_session"] = True
    process = subprocess.Popen(command_text, **popen_kwargs)
    try:
        stdout, stderr = process.communicate(timeout=timeout)
        return subprocess.CompletedProcess(command_text, process.returncode, stdout=stdout, stderr=stderr)
    except subprocess.TimeoutExpired:
        if os.name == "nt":
            subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
        else:
            process.kill()
        stdout, stderr = process.communicate()
        stdout = stdout or ""
        stderr = stderr or ""
        stderr += f"\nTIMEOUT after {timeout} seconds; process tree was terminated"
        return subprocess.CompletedProcess(command_text, 124, stdout=stdout, stderr=stderr)


def wsl_path(path: Path) -> str:
    resolved = path.resolve()
    drive = resolved.drive.rstrip(":").lower()
    rest = resolved.relative_to(resolved.anchor).as_posix()
    return f"/mnt/{drive}/{rest}"


def ceil_log2(value: int) -> int:
    return max(1, (value - 1).bit_length())


def hvm_tpc_l2_for_threads(threads: int) -> int:
    if threads <= 1:
        return 0
    if threads & (threads - 1):
        raise ValueError("HVM thread count must be 1 or a power of two")
    return threads.bit_length() - 1


def capacity_for_sum(sum_n: int) -> int:
    capacity = 1
    while capacity < sum_n + 2:
        capacity *= 2
    return max(128, capacity)


def expected_sum_u24(term_count: int) -> int:
    return (term_count * (term_count - 1) // 2) % U24_MODULUS


def sequential_operation_count(term_count: int) -> int:
    return max(0, term_count - 1)


def expected_sequential_steps(term_count: int) -> int:
    return sequential_operation_count(term_count) * 3 + 2


def expected_tree_steps(term_count: int) -> int:
    return term_count


def macro_flags_for_sum(term_count: int) -> list[str]:
    operation_count = sequential_operation_count(term_count)
    capacity = capacity_for_sum(term_count)
    max_steps = max(4096, operation_count * 4 + 128)
    produced_bits = ceil_log2(capacity + 2)
    return [
        f"-DSUM_REC_OPERATION_COUNT={operation_count}",
        f"-DSUM_REC_EXPECTED_RESULT={expected_sum_u24(term_count)}",
        f"-DSUM_REC_EXPECTED_STEPS={expected_sequential_steps(term_count)}",
        f"-DSUM_REC_MAXIMUM_STEPS={max_steps}",
        f"-DREDUCER_MAXIMUM_NODE_COUNT={capacity}",
        f"-DREDUCER_MAXIMUM_REDEX_COUNT={capacity}",
        f"-DREDUCER_MAXIMUM_VARIABLE_COUNT={capacity + 1}",
        f"-DREDUCER_MAXIMUM_REDUCTION_STEPS={max_steps}",
        f"-DREDUCER_MAXIMUM_BOOK_NODE_COUNT={capacity}",
        f"-DREDUCER_MAXIMUM_BOOK_REDEX_COUNT={capacity}",
        f"-DREDUCER_MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION={capacity}",
        f"-DREDUCER_MAXIMUM_BOOK_REDEX_COUNT_PER_DEFINITION={capacity}",
        f"-DREDUCER_MAXIMUM_BOOK_VARIABLE_COUNT_PER_DEFINITION={capacity}",
        f"-DREDUCER_PRODUCED_PAIR_COUNTER_BIT_COUNT={produced_bits}",
    ]


def parse_key_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.strip().split("=", 1)
            key = key.strip()
            if key in EXPECTED_CSIM_KEYS:
                values[key] = value.strip()
    return values


def parse_cosim_report(report_path: Path) -> dict[str, str]:
    if not report_path.exists():
        return {}
    text = report_path.read_text(encoding="utf-8", errors="ignore")
    match = re.search(
        r"\|\s*Verilog\s*\|\s*(?P<status>\w+)\s*\|\s*(?P<min>\d+)\s*\|\s*(?P<avg>\d+)\s*\|\s*(?P<max>\d+).*?\|\s*(?P<total>\d+)\s*\|",
        text,
        re.S,
    )
    if not match:
        return {"rtl_status": "unparsed"}
    return {
        "rtl_status": match.group("status"),
        "latency_min_cycles": match.group("min"),
        "latency_avg_cycles": match.group("avg"),
        "latency_max_cycles": match.group("max"),
        "total_execution_cycles": match.group("total"),
    }


def parse_csynth_xml(xml_path: Path) -> dict[str, str]:
    if not xml_path.exists():
        return {}
    root = ET.parse(xml_path).getroot()
    def text_of(tag: str) -> str:
        node = root.find(f".//{tag}")
        return "" if node is None or node.text is None else node.text
    resources = {"BRAM_18K": "", "DSP": "", "FF": "", "LUT": ""}
    # The text report is easier for resource summary across Vitis versions.
    rpt_path = xml_path.with_suffix(".rpt")
    if rpt_path.exists():
        rpt = rpt_path.read_text(encoding="utf-8", errors="ignore")
        m = re.search(r"\|\+ .*?\|\s*Timing\|.*?\|\s*(\d+) \(\d+%\)\|\s*(\d+) \(\d+%\)\|\s*(\d+) \(\d+%\)\|\s*(\d+) \(\d+%\)\|", rpt)
        if m:
            resources = {"BRAM_18K": m.group(1), "DSP": m.group(2), "FF": m.group(3), "LUT": m.group(4)}
    result = {
        "target_clock_period_ns": text_of("TargetClockPeriod"),
        "estimated_clock_period_ns": text_of("EstimatedClockPeriod"),
        **resources,
    }
    try:
        target = float(result["target_clock_period_ns"])
        estimated = float(result["estimated_clock_period_ns"])
        result["safe_clock_period_ns"] = f"{max(target, estimated):.3f}"
    except (TypeError, ValueError):
        result["safe_clock_period_ns"] = ""
    return result


def add_rtl_time_metrics(row: dict[str, object]) -> None:
    if row.get("total_execution_cycles"):
        row["rtl_cycles"] = row["total_execution_cycles"]
    if row.get("latency_avg_cycles"):
        row["rtl_latency_avg_cycles"] = row["latency_avg_cycles"]
    if row.get("safe_clock_period_ns") and row.get("total_execution_cycles"):
        row["estimated_time_us"] = float(row["safe_clock_period_ns"]) * float(row["total_execution_cycles"]) / 1000.0
        row["executions_per_second"] = 1_000_000.0 / float(row["estimated_time_us"])


def hls_case_fingerprint(config_path: Path) -> str:
    digest = hashlib.sha256()
    config_text = config_path.read_text(encoding="utf-8")
    digest.update(config_text.encode("utf-8"))
    hashed_paths: set[Path] = set()
    for line in config_text.splitlines():
        stripped = line.strip()
        if stripped.startswith("syn.file=") or stripped.startswith("tb.file="):
            relative_path = stripped.split("=", 1)[1].strip()
            source_path = (ROOT / relative_path).resolve()
            hashed_paths.add(source_path)
            digest.update(str(source_path).encode("utf-8"))
            if source_path.exists():
                digest.update(source_path.read_bytes())
            else:
                digest.update(b"<missing>")
    # Header-only changes can alter the generated RTL without changing the HLS
    # config. Include project headers so stale reports are not reused silently.
    for pattern in ("src/*.hpp", "test/*.hpp"):
        for source_path in sorted(ROOT.glob(pattern)):
            resolved = source_path.resolve()
            if resolved in hashed_paths:
                continue
            digest.update(str(resolved).encode("utf-8"))
            digest.update(resolved.read_bytes())
    return digest.hexdigest()


def manifest_path_for_work(work_name: str) -> Path:
    return ROOT / work_name / "sum_sweep_manifest.json"


def read_manifest(work_name: str) -> dict[str, object]:
    manifest_path = manifest_path_for_work(work_name)
    if not manifest_path.exists():
        return {}
    try:
        return json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {}


def write_manifest(work_name: str, manifest: dict[str, object]) -> None:
    manifest_path = manifest_path_for_work(work_name)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")


def manifest_matches(manifest: dict[str, object], config_hash: str, top_name: str) -> bool:
    return (
        manifest.get("case_fingerprint_sha256") == config_hash and
        manifest.get("top_name") == top_name and
        manifest.get("script") == "run_sum_sweep_benchmarks.py"
    )


def terminate_vitis_processes() -> None:
    if os.name != "nt":
        return
    for image_name in VITIS_PROCESS_IMAGES:
        subprocess.run(
            ["taskkill", "/IM", image_name, "/T", "/F"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )


def is_retryable_hls_failure(row: dict[str, object]) -> bool:
    message = str(row.get("failure_message", ""))
    stage = str(row.get("failure_stage", ""))
    return (
        "Spawn failed" in message or
        stage in {"synth_timeout", "cosim_timeout", "csim_timeout"}
    )


def remove_stale_hls_work_dir(work_dir: Path, matching_manifest: bool) -> None:
    if matching_manifest or not work_dir.exists():
        return
    resolved = work_dir.resolve()
    if resolved.parent != ROOT.resolve():
        raise RuntimeError(f"Refusing to remove unexpected HLS work directory: {resolved}")
    if not (resolved.name.startswith("sum_") or resolved.name.startswith("parallel_sum_")):
        raise RuntimeError(f"Refusing to remove unexpected HLS work directory: {resolved}")
    def make_writable_and_retry(function, path, exc_info):
        if not issubclass(exc_info[0], PermissionError):
            raise exc_info[1]
        os.chmod(path, stat.S_IWRITE)
        function(path)

    last_error: Exception | None = None
    for attempt in range(3):
        try:
            if sys.version_info >= (3, 12):
                shutil.rmtree(resolved, onexc=lambda function, path, exc: (os.chmod(path, stat.S_IWRITE), function(path)))
            else:
                shutil.rmtree(resolved, onerror=make_writable_and_retry)
            return
        except PermissionError as error:
            last_error = error
            terminate_vitis_processes()
            time.sleep(2 + attempt)
    if last_error:
        raise last_error


def reset_hls_work_dir_for_retry(work_dir: Path) -> None:
    terminate_vitis_processes()
    if not work_dir.exists():
        return
    resolved = work_dir.resolve()
    if resolved.parent != ROOT.resolve():
        raise RuntimeError(f"Refusing to reset unexpected HLS work directory: {resolved}")
    if not (resolved.name.startswith("sum_") or resolved.name.startswith("parallel_sum_")):
        raise RuntimeError(f"Refusing to reset unexpected HLS work directory: {resolved}")
    remove_stale_hls_work_dir(resolved, matching_manifest=False)


def write_hls_config(sum_n: int, cflags: list[str], work_dir: Path) -> Path:
    config_path = work_dir / f"hls_sum_{sum_n}.cfg"
    flag_text = "-DFPGA_SMALL_PROFILE=1 " + " ".join(cflags)
    config_path.write_text(
        "\n".join(
            [
                "part=xc7a35tcpg236-1",
                "",
                "[hls]",
                "flow_target=vivado",
                "package.output.format=ip_catalog",
                "package.output.syn=false",
                "syn.top=reducer_sum_rec_scaled_cosim",
                "csim.clean=1",
                f"syn.cflags={flag_text}",
                f"tb.cflags={flag_text}",
                "syn.file=./src/reducer_sum_rec_scaled_cosim.cpp",
                "syn.file=./src/reducer_core.cpp",
                "tb.file=./test/reducer_sum_rec_scaled_cosim_test.cpp",
                "",
            ]
        ),
        encoding="utf-8",
    )
    return config_path


def run_hls_case(
    config_path: Path,
    work_name: str,
    top_name: str,
    mode: str,
    timeout: int,
    row: dict[str, object],
) -> dict[str, object]:
    for attempt_index in range(DEFAULT_HLS_RETRY_COUNT + 1):
        attempt_row = dict(row)
        attempt_row["hls_attempt"] = attempt_index + 1
        result = run_hls_case_once(config_path, work_name, top_name, mode, timeout, attempt_row)
        if result.get("pass") == "yes" or not is_retryable_hls_failure(result) or attempt_index == DEFAULT_HLS_RETRY_COUNT:
            if attempt_index > 0:
                result["hls_retries_used"] = attempt_index
            return result
        reset_hls_work_dir_for_retry(ROOT / work_name)
    return result


def run_hls_case_once(
    config_path: Path,
    work_name: str,
    top_name: str,
    mode: str,
    timeout: int,
    row: dict[str, object],
) -> dict[str, object]:
    config_hash = hls_case_fingerprint(config_path)
    work_dir = ROOT / work_name
    synth_xml_path = work_dir / "hls" / "syn" / "report" / "csynth.xml"
    cosim_report_path = work_dir / "hls" / "sim" / "report" / f"{top_name}_cosim.rpt"
    manifest = read_manifest(work_name)
    matching_manifest = manifest_matches(manifest, config_hash, top_name)
    remove_stale_hls_work_dir(work_dir, matching_manifest)
    if not matching_manifest:
        manifest = {}
    row["work_name"] = work_name

    if mode == "csim":
        csim = try_command(["cmd", "/c", VITIS_RUN, "--mode", "hls", "--config", config_path, "--work_dir", work_name, "--csim"], timeout=timeout)
        row["csim_returncode"] = csim.returncode
        row.update({f"csim_{key}": value for key, value in parse_key_values(csim.stdout).items()})
        row["stdout_tail"] = "\n".join(csim.stdout.splitlines()[-20:])
        if csim.returncode != 0:
            row["pass"] = "no"
            row["failure_stage"] = "csim_timeout" if csim.returncode == 124 else "csim"
            row["failure_message"] = (csim.stderr + "\n" + csim.stdout)[-2000:]
        else:
            row["pass"] = "yes"
        return row

    if matching_manifest and manifest.get("synth_pass") is True and synth_xml_path.exists():
        row["synth_returncode"] = "reused"
    else:
        synth = try_command(["cmd", "/c", VPP, "-c", "--mode", "hls", "--config", config_path, "--work_dir", work_name], timeout=timeout)
        row["synth_returncode"] = synth.returncode
        row["stdout_tail"] = "\n".join(synth.stdout.splitlines()[-20:])
        if synth.returncode != 0:
            row["pass"] = "no"
            row["failure_stage"] = "synth_timeout" if synth.returncode == 124 else "synth"
            row["failure_message"] = (synth.stderr + "\n" + synth.stdout)[-2000:]
            return row
        manifest = {
            "script": "run_sum_sweep_benchmarks.py",
            "case_fingerprint_sha256": config_hash,
            "top_name": top_name,
            "work_name": work_name,
            "synth_pass": True,
        }
        write_manifest(work_name, manifest)

    row.update(parse_csynth_xml(synth_xml_path))
    if mode == "synth":
        row["pass"] = "yes"
        return row

    manifest = read_manifest(work_name)
    matching_manifest = manifest_matches(manifest, config_hash, top_name)
    if matching_manifest and manifest.get("cosim_pass") is True and cosim_report_path.exists():
        row["cosim_returncode"] = "reused"
    else:
        cosim = try_command(["cmd", "/c", VITIS_RUN, "--mode", "hls", "--config", config_path, "--work_dir", work_name, "--cosim"], timeout=timeout)
        row["cosim_returncode"] = cosim.returncode
        row["stdout_tail"] = "\n".join(cosim.stdout.splitlines()[-20:])
        if cosim.returncode != 0:
            row["pass"] = "no"
            row["failure_stage"] = "cosim_timeout" if cosim.returncode == 124 else "cosim"
            row["failure_message"] = (cosim.stderr + "\n" + cosim.stdout)[-2000:]
            return row
        manifest.update({
            "script": "run_sum_sweep_benchmarks.py",
            "case_fingerprint_sha256": config_hash,
            "top_name": top_name,
            "work_name": work_name,
            "synth_pass": True,
        })
        write_manifest(work_name, manifest)

    row.update(parse_cosim_report(cosim_report_path))
    add_rtl_time_metrics(row)
    row["pass"] = "yes" if row.get("rtl_status", "Pass").lower() in {"pass", "passed"} else "no"
    if row["pass"] == "yes":
        manifest = read_manifest(work_name)
        manifest.update({
            "script": "run_sum_sweep_benchmarks.py",
            "case_fingerprint_sha256": config_hash,
            "top_name": top_name,
            "work_name": work_name,
            "synth_pass": True,
            "cosim_pass": True,
        })
        write_manifest(work_name, manifest)
    if row["pass"] != "yes" and not row.get("failure_stage"):
        row["failure_stage"] = "cosim_report"
        row["failure_message"] = f"Missing or non-passing report: {cosim_report_path}"
    return row


def run_sequential_reducer(sum_n: int, hls_mode: str, timeout: int) -> dict[str, object]:
    cflags = macro_flags_for_sum(sum_n)
    work_root = BUILD / "hls"
    work_root.mkdir(parents=True, exist_ok=True)
    config_path = write_hls_config(sum_n, cflags, work_root)
    work_name = f"sum_{sum_n}_{hls_mode}_{hls_case_fingerprint(config_path)[:12]}"
    row: dict[str, object] = {
        "benchmark": "sum_recursive",
        "n": sum_n,
        "lanes": 1,
        "result": expected_sum_u24(sum_n),
        "reduction_steps": expected_sequential_steps(sum_n),
        "operate_count": sequential_operation_count(sum_n),
        "expected_result_u24": expected_sum_u24(sum_n),
        "expected_reduction_steps": expected_sequential_steps(sum_n),
        "expected_operate_count": sequential_operation_count(sum_n),
        "hls_mode": hls_mode,
    }
    return run_hls_case(config_path, work_name, "reducer_sum_rec_scaled_cosim", hls_mode, timeout, row)


def write_parallel_hls_config(sum_n: int, lanes: int, cflags: list[str], work_dir: Path) -> Path:
    config_path = work_dir / f"hls_parallel_sum_{sum_n}_lanes_{lanes}.cfg"
    flag_text = "-DFPGA_SMALL_PROFILE=1 " + " ".join(cflags)
    config_path.write_text(
        "\n".join(
            [
                "part=xc7a35tcpg236-1",
                "",
                "[hls]",
                "flow_target=vivado",
                "package.output.format=ip_catalog",
                "package.output.syn=false",
                "syn.top=reducer_sum_tree_parallel_cosim",
                "csim.clean=1",
                f"syn.cflags={flag_text}",
                f"tb.cflags={flag_text}",
                "syn.file=./src/reducer_sum_tree_parallel_cosim.cpp",
                "syn.file=./src/reducer_core_parallel.cpp",
                "tb.file=./test/reducer_sum_tree_parallel_cosim_test.cpp",
                "",
            ]
        ),
        encoding="utf-8",
    )
    return config_path


def run_parallel_reducer_rtl(sum_n: int, lanes: int, hls_mode: str, timeout: int) -> dict[str, object]:
    capacity = capacity_for_sum(sum_n)
    max_steps = max(4096, sum_n * 4 + 128)
    produced_bits = ceil_log2(capacity + 2)
    cflags = [
        f"-DREDUCER_CONFIGURED_LANE_COUNT={lanes}",
        f"-DSUM_TREE_TERM_COUNT={sum_n}",
        f"-DSUM_TREE_EXPECTED_RESULT={expected_sum_u24(sum_n)}",
        f"-DSUM_TREE_EXPECTED_STEPS={expected_tree_steps(sum_n)}",
        f"-DSUM_TREE_EXPECTED_OPERATE_COUNT={sum_n // 2}",
        f"-DSUM_TREE_MAXIMUM_STEPS={max_steps}",
        f"-DREDUCER_MAXIMUM_NODE_COUNT={capacity}",
        f"-DREDUCER_MAXIMUM_REDEX_COUNT={capacity}",
        f"-DREDUCER_MAXIMUM_VARIABLE_COUNT={capacity + 1}",
        f"-DREDUCER_MAXIMUM_REDUCTION_STEPS={max_steps}",
        f"-DREDUCER_MAXIMUM_BOOK_NODE_COUNT={capacity}",
        f"-DREDUCER_MAXIMUM_BOOK_REDEX_COUNT={capacity}",
        f"-DREDUCER_MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION={capacity}",
        f"-DREDUCER_MAXIMUM_BOOK_REDEX_COUNT_PER_DEFINITION={capacity}",
        f"-DREDUCER_MAXIMUM_BOOK_VARIABLE_COUNT_PER_DEFINITION={capacity}",
        f"-DREDUCER_PRODUCED_PAIR_COUNTER_BIT_COUNT={produced_bits}",
    ]
    work_root = BUILD / "hls"
    work_root.mkdir(parents=True, exist_ok=True)
    config_path = write_parallel_hls_config(sum_n, lanes, cflags, work_root)
    work_name = f"parallel_sum_{sum_n}_lanes_{lanes}_{hls_mode}_{hls_case_fingerprint(config_path)[:12]}"
    row: dict[str, object] = {
        "benchmark": "sum_tree",
        "n": sum_n,
        "lanes": lanes,
        "result": expected_sum_u24(sum_n),
        "reduction_steps": expected_tree_steps(sum_n),
        "operate_count": sum_n // 2,
        "expected_result_u24": expected_sum_u24(sum_n),
        "expected_reduction_steps": expected_tree_steps(sum_n),
        "expected_operate_count": sum_n // 2,
        "active_pair_count_input": sum_n // 2,
        "hls_mode": hls_mode,
    }
    return run_hls_case(config_path, work_name, "reducer_sum_tree_parallel_cosim", hls_mode, timeout, row)


def write_hvm_sum_recursive(sum_n: int) -> Path:
    path = BUILD / "hvm" / f"hvm_sum_recursive_{sum_n}.hvm"
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["@main = x"]
    previous = "0"
    for value in range(1, sum_n):
        next_name = "x" if value == sum_n - 1 else f"v{value}"
        lines.append(f"  & {previous} ~ $([+] $({value} {next_name}))")
        previous = next_name
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def write_hvm_sum_tree(sum_n: int) -> Path:
    path = BUILD / "hvm" / f"hvm_sum_tree_{sum_n}.hvm"
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["@main = x"]
    current_terms = [str(value) for value in range(sum_n)]
    level = 0
    while len(current_terms) > 1:
        next_terms: list[str] = []
        for pair_index in range(0, len(current_terms), 2):
            is_last_output = len(current_terms) == 2 and pair_index == 0
            output = "x" if is_last_output else f"t{level}_{pair_index // 2}"
            lines.append(f"  & {current_terms[pair_index]} ~ $([+] $({current_terms[pair_index + 1]} {output}))")
            next_terms.append(output)
        current_terms = next_terms
        level += 1
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def patch_hvm_c_timing(c_path: Path) -> None:
    text = c_path.read_text(encoding="utf-8")
    text = text.replace('printf("- TIME: %.2fs\\n", duration);', 'printf("- TIME: %.9fs\\n", duration);')
    text = text.replace(
        'printf("- MIPS: %.2f\\n", (double)itrs / duration / 1000000.0);',
        'printf("- MIPS: %.6f\\n", (double)itrs / duration / 1000000.0);',
    )
    c_path.write_text(text, encoding="utf-8")


def parse_hvm_output(text: str) -> dict[str, object]:
    result_match = re.search(r"Result:\s*([^\n\r]+)", text)
    itrs_match = re.search(r"- ITRS:\s*(\d+)", text)
    time_match = re.search(r"- TIME:\s*([0-9.]+)s", text)
    mips_match = re.search(r"- MIPS:\s*([0-9.]+)", text)
    if not (result_match and itrs_match and time_match and mips_match):
        raise RuntimeError(f"Could not parse HVM output:\n{text}")
    return {
        "result": result_match.group(1).strip(),
        "itrs": int(itrs_match.group(1)),
        "time_seconds": float(time_match.group(1)),
        "mips": float(mips_match.group(1)),
    }


def run_hvm(sum_n: int, threads: int, runs: int, timeout: int, benchmark: str) -> dict[str, object]:
    hvm_file = write_hvm_sum_tree(sum_n) if benchmark == "sum_tree" else write_hvm_sum_recursive(sum_n)
    hvm_build = BUILD / "hvm"
    c_file = hvm_build / f"hvm_{benchmark}_{sum_n}.c"
    exe = hvm_build / f"hvm_{benchmark}_{sum_n}_t{threads}"
    row: dict[str, object] = {
        "benchmark": benchmark,
        "n": sum_n,
        "threads": threads,
        "runs": runs,
        "result": expected_sum_u24(sum_n),
        "expected_result_u24": expected_sum_u24(sum_n),
        "hvm_pass": "no",
    }
    gen_command = f'export PATH="$HOME/.cargo/bin:$PATH"; cd {wsl_path(ROOT)}; hvm gen-c {wsl_path(hvm_file)} > {wsl_path(c_file)}'
    gen = try_command(["wsl", "-d", "hvm-ubuntu", "-e", "sh", "-lc", gen_command], timeout=timeout)
    if gen.returncode != 0:
        row["failure_stage"] = "hvm_gen_c_timeout" if gen.returncode == 124 else "hvm_gen_c"
        row["failure_message"] = (gen.stderr + "\n" + gen.stdout)[-2000:]
        return row
    patch_hvm_c_timing(c_file)
    tpc_l2 = hvm_tpc_l2_for_threads(threads)
    compile_command = f"cd {wsl_path(ROOT)}; gcc -O2 -pthread -DTPC_L2={tpc_l2} {wsl_path(c_file)} -lm -o {wsl_path(exe)}"
    compiled = try_command(["wsl", "-d", "hvm-ubuntu", "-e", "sh", "-lc", compile_command], timeout=timeout)
    if compiled.returncode != 0:
        row["failure_stage"] = "hvm_compile_timeout" if compiled.returncode == 124 else "hvm_compile"
        row["failure_message"] = (compiled.stderr + "\n" + compiled.stdout)[-2000:]
        return row
    run_command_text = f"cd {wsl_path(ROOT)}; for i in $(seq 1 {runs}); do {wsl_path(exe)}; done"
    completed = try_command(["wsl", "-d", "hvm-ubuntu", "-e", "sh", "-lc", run_command_text], timeout=max(timeout, runs * 2))
    if completed.returncode != 0:
        row["failure_stage"] = "hvm_run_timeout" if completed.returncode == 124 else "hvm_run"
        row["failure_message"] = (completed.stderr + "\n" + completed.stdout)[-2000:]
        return row
    chunks = re.split(r"(?=Result:)", completed.stdout)
    parsed = [parse_hvm_output(chunk) for chunk in chunks if chunk.strip()]
    if len(parsed) != runs:
        row["failure_stage"] = "hvm_parse"
        row["failure_message"] = completed.stdout[-2000:]
        return row
    times = [float(item["time_seconds"]) for item in parsed]
    mips = [float(item["mips"]) for item in parsed]
    row.update(
        {
            "hvm_pass": "yes",
            "result_values": ";".join(sorted({str(item["result"]) for item in parsed})),
            "interaction_values": ";".join(str(value) for value in sorted({int(item["itrs"]) for item in parsed})),
            "interactions": ";".join(str(value) for value in sorted({int(item["itrs"]) for item in parsed})),
            "avg_time_seconds": sum(times) / len(times),
            "avg_time_us": (sum(times) / len(times)) * 1_000_000.0,
            "executions_per_second": 1.0 / (sum(times) / len(times)),
            "min_time_seconds": min(times),
            "max_time_seconds": max(times),
            "avg_mips": sum(mips) / len(mips),
            "min_mips": min(mips),
            "max_mips": max(mips),
            "stdev_mips": math.sqrt(sum((value - (sum(mips) / len(mips))) ** 2 for value in mips) / len(mips)),
        }
    )
    return row


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fieldnames: list[str] = []
    for row in rows:
        for key in row:
            if key not in fieldnames:
                fieldnames.append(key)
    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def read_csv(path: Path) -> list[dict[str, object]]:
    if not path.exists() or path.stat().st_size == 0:
        return []
    with path.open("r", encoding="utf-8", newline="") as file:
        return list(csv.DictReader(file))


def find_hvm_row(
    rows: list[dict[str, object]],
    benchmark: str,
    sum_n: int,
    threads: int,
    runs: int,
) -> dict[str, object] | None:
    for row in rows:
        if (
            row.get("benchmark") == benchmark and
            str(row.get("n")) == str(sum_n) and
            str(row.get("threads")) == str(threads) and
            str(row.get("runs")) == str(runs) and
            row.get("hvm_pass") == "yes"
        ):
            return row
    return None


def find_reducer_row(
    rows: list[dict[str, object]],
    benchmark: str,
    sum_n: int,
    lanes: int,
    hls_mode: str,
) -> dict[str, object] | None:
    for row in rows:
        if (
            row.get("benchmark") == benchmark and
            str(row.get("n")) == str(sum_n) and
            str(row.get("lanes")) == str(lanes) and
            row.get("hls_mode") == hls_mode and
            row.get("pass") in {"yes", "no"}
        ):
            return row
    return None


def dedupe_rows(rows: list[dict[str, object]], key_fields: tuple[str, ...]) -> list[dict[str, object]]:
    seen: set[tuple[str, ...]] = set()
    deduped: list[dict[str, object]] = []
    for row in rows:
        key = tuple(str(row.get(field, "")) for field in key_fields)
        if key in seen:
            continue
        seen.add(key)
        deduped.append(row)
    return deduped


def normalize_reducer_rows(rows: list[dict[str, object]], benchmark: str) -> list[dict[str, object]]:
    normalized: list[dict[str, object]] = []
    for row in rows:
        fixed = dict(row)
        if not fixed.get("benchmark"):
            fixed["benchmark"] = benchmark
        if not fixed.get("hls_mode"):
            fixed["hls_mode"] = "cosim"
        normalized.append(fixed)
    return normalized


def parse_int_list(text: str) -> list[int]:
    return [int(part.strip()) for part in text.split(",") if part.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description="Run sum sweep benchmarks for reducer and HVM2.")
    parser.add_argument("--sums", default=",".join(str(x) for x in DEFAULT_SUMS), help="Comma-separated sum limits.")
    parser.add_argument("--lanes", default=",".join(str(x) for x in DEFAULT_LANES), help="Comma-separated reducer lane counts.")
    parser.add_argument("--hvm-threads", default=",".join(str(x) for x in DEFAULT_HVM_THREADS), help="Comma-separated HVM2 thread counts.")
    parser.add_argument("--hvm-runs", type=int, default=100)
    parser.add_argument("--hls-mode", choices=["csim", "synth", "cosim"], default="cosim")
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--stop-on-fail", action="store_true", default=True)
    args = parser.parse_args()

    sums = parse_int_list(args.sums)
    lanes = parse_int_list(args.lanes)
    hvm_threads = parse_int_list(args.hvm_threads)
    BUILD.mkdir(parents=True, exist_ok=True)
    RAW.mkdir(parents=True, exist_ok=True)

    reducer_sum_recursive_csv = RAW / "sum_sweep_reducer_sum_recursive_rtl_current.csv"
    reducer_sum_tree_csv = RAW / "sum_sweep_reducer_sum_tree_rtl_current.csv"
    hvm_sum_recursive_csv = RAW / "sum_sweep_hvm_sum_recursive_current.csv"
    hvm_sum_tree_csv = RAW / "sum_sweep_hvm_sum_tree_threads_current.csv"
    summary_json = RAW / "sum_sweep_summary_current.json"
    sequential_rows: list[dict[str, object]] = dedupe_rows(
        normalize_reducer_rows(read_csv(reducer_sum_recursive_csv), "sum_recursive"),
        ("n", "lanes", "hls_mode"),
    )
    tree_rows: list[dict[str, object]] = dedupe_rows(
        normalize_reducer_rows(read_csv(reducer_sum_tree_csv), "sum_tree"),
        ("n", "lanes", "hls_mode"),
    )
    hvm_recursive_rows: list[dict[str, object]] = dedupe_rows(read_csv(hvm_sum_recursive_csv), ("benchmark", "n", "threads", "runs"))
    hvm_tree_rows: list[dict[str, object]] = dedupe_rows(read_csv(hvm_sum_tree_csv), ("benchmark", "n", "threads", "runs"))
    successful_recursive_sums: list[int] = []
    successful_sum_to_lanes: dict[int, list[int]] = {}

    def flush_outputs() -> None:
        write_csv(reducer_sum_recursive_csv, sequential_rows)
        write_csv(reducer_sum_tree_csv, tree_rows)
        write_csv(hvm_sum_recursive_csv, hvm_recursive_rows)
        write_csv(hvm_sum_tree_csv, hvm_tree_rows)
        summary = {
            "sums_requested": sums,
            "lanes_requested": lanes,
            "hvm_threads_requested": hvm_threads,
            "hls_mode": args.hls_mode,
            "hvm_runs": args.hvm_runs,
            "successful_recursive_sums": successful_recursive_sums,
            "successful_sum_to_lanes": successful_sum_to_lanes,
            "outputs": {
                "reducer_sum_recursive_csv": "docs/results/raw/sum_sweep_reducer_sum_recursive_rtl_current.csv",
                "reducer_sum_tree_csv": "docs/results/raw/sum_sweep_reducer_sum_tree_rtl_current.csv",
                "hvm_sum_recursive_csv": "docs/results/raw/sum_sweep_hvm_sum_recursive_current.csv",
                "hvm_sum_tree_csv": "docs/results/raw/sum_sweep_hvm_sum_tree_threads_current.csv",
            },
        }
        summary_json.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    for sum_n in sums:
        seq_row = find_reducer_row(sequential_rows, "sum_recursive", sum_n, 1, args.hls_mode)
        if seq_row is None:
            seq_row = run_sequential_reducer(sum_n, args.hls_mode, args.timeout)
            sequential_rows.append(seq_row)
        flush_outputs()
        if seq_row.get("pass") != "yes":
            if args.stop_on_fail:
                break
            continue
        successful_recursive_sums.append(sum_n)
        flush_outputs()

        successful_lanes: list[int] = []
        for lane_count in lanes:
            if lane_count > sum_n:
                break
            lane_row = find_reducer_row(tree_rows, "sum_tree", sum_n, lane_count, args.hls_mode)
            if lane_row is None:
                lane_row = run_parallel_reducer_rtl(sum_n, lane_count, args.hls_mode, args.timeout)
                tree_rows.append(lane_row)
            if lane_row.get("pass") == "yes":
                successful_lanes.append(lane_count)
                successful_sum_to_lanes[sum_n] = successful_lanes
                flush_outputs()
            else:
                flush_outputs()
                break
        successful_sum_to_lanes[sum_n] = successful_lanes
        flush_outputs()

    for sum_n in successful_recursive_sums:
        existing_hvm_row = find_hvm_row(hvm_recursive_rows, "sum_recursive", sum_n, 1, args.hvm_runs)
        if existing_hvm_row is None:
            hvm_recursive_rows.append(run_hvm(sum_n, 1, args.hvm_runs, args.timeout, "sum_recursive"))
        flush_outputs()

    for sum_n, successful_lanes in successful_sum_to_lanes.items():
        allowed_threads = [thread for thread in hvm_threads if thread == 1 or thread in successful_lanes]
        for thread_count in allowed_threads:
            existing_hvm_row = find_hvm_row(hvm_tree_rows, "sum_tree", sum_n, thread_count, args.hvm_runs)
            if existing_hvm_row is None:
                hvm_tree_rows.append(run_hvm(sum_n, thread_count, args.hvm_runs, args.timeout, "sum_tree"))
            flush_outputs()
    flush_outputs()
    summary = json.loads(summary_json.read_text(encoding="utf-8"))
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
