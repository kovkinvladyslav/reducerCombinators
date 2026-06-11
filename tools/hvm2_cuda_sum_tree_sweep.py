#!/usr/bin/env python3
"""Generate HVM2 sum-tree programs and measure hvm run / run-cu.

This script is intended for the Linux CUDA handoff. It does not require the
reducer project to be built. It only needs an HVM2 executable.
"""

from __future__ import annotations

import argparse
import csv
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path


def build_sum_tree_hvm(n: int) -> str:
    if n < 2 or n & (n - 1):
        raise ValueError("n must be a power of two and >= 2")

    lines: list[str] = ["@main = x"]
    current_names: list[str] = []

    for pair_index in range(n // 2):
        left = pair_index * 2
        right = left + 1
        out = f"t0_{pair_index}"
        lines.append(f"  & {left} ~ $([+] $({right} {out}))")
        current_names.append(out)

    level = 1
    while len(current_names) > 1:
        next_names: list[str] = []
        for pair_index in range(0, len(current_names), 2):
            left = current_names[pair_index]
            right = current_names[pair_index + 1]
            is_last = len(current_names) == 2 and pair_index == 0
            out = "x" if is_last else f"t{level}_{pair_index // 2}"
            lines.append(f"  & {left} ~ $([+] $({right} {out}))")
            if not is_last:
                next_names.append(out)
        current_names = next_names
        level += 1

    return "\n".join(lines) + "\n"


def parse_hvm_output(text: str) -> dict[str, str]:
    result_match = re.search(r"Result:\s*([^\r\n]+)", text)
    itrs_match = re.search(r"-\s*ITRS:\s*([0-9]+)", text)
    time_match = re.search(r"-\s*TIME:\s*([0-9.]+)s", text)
    mips_match = re.search(r"-\s*MIPS:\s*([0-9.]+)", text)
    return {
        "result": result_match.group(1).strip() if result_match else "",
        "itrs": itrs_match.group(1) if itrs_match else "",
        "time_seconds": time_match.group(1) if time_match else "",
        "mips": mips_match.group(1) if mips_match else "",
    }


def run_one(hvm: Path, command: str, program: Path, timeout: float) -> tuple[bool, str, float, str]:
    start = time.perf_counter()
    try:
        completed = subprocess.run(
            [str(hvm), command, str(program)],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
        elapsed = time.perf_counter() - start
        return completed.returncode == 0, completed.stdout, elapsed, ""
    except subprocess.TimeoutExpired as exc:
        elapsed = time.perf_counter() - start
        output = ""
        if exc.stdout:
            output += exc.stdout if isinstance(exc.stdout, str) else exc.stdout.decode(errors="replace")
        return False, output, elapsed, "timeout"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--hvm", required=True, type=Path, help="Path to HVM2 executable")
    parser.add_argument("--mode", choices=["rust", "cuda", "both"], default="cuda")
    parser.add_argument("--runs", type=int, default=30)
    parser.add_argument("--sizes", nargs="+", type=int, default=[16, 32, 64, 128])
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--work-dir", type=Path, default=Path("docs/results/raw/hvm2_generated"))
    parser.add_argument("--out", type=Path, default=Path("docs/results/raw/hvm2_cuda_sum_tree_linux.csv"))
    args = parser.parse_args()

    args.work_dir.mkdir(parents=True, exist_ok=True)
    args.out.parent.mkdir(parents=True, exist_ok=True)

    modes = []
    if args.mode in ("rust", "both"):
        modes.append(("hvm_run", "run"))
    if args.mode in ("cuda", "both"):
        modes.append(("hvm_run_cuda", "run-cu"))

    rows: list[dict[str, object]] = []

    for n in args.sizes:
        program = args.work_dir / f"hvm_sum_tree_{n}.hvm"
        program.write_text(build_sum_tree_hvm(n), encoding="utf-8")
        expected = n * (n - 1) // 2

        for mode_name, command in modes:
            parsed_runs: list[dict[str, str]] = []
            wall_times: list[float] = []
            failures: list[str] = []

            for run_index in range(args.runs):
                ok, output, wall_time, failure = run_one(args.hvm, command, program, args.timeout)
                if not ok:
                    failures.append(failure or output.strip().splitlines()[-1] if output.strip() else "nonzero_exit")
                    continue
                parsed = parse_hvm_output(output)
                if parsed["result"] != str(expected):
                    failures.append(f"unexpected_result:{parsed['result']}")
                    continue
                parsed_runs.append(parsed)
                wall_times.append(wall_time)

            time_values = [float(r["time_seconds"]) for r in parsed_runs if r["time_seconds"]]
            mips_values = [float(r["mips"]) for r in parsed_runs if r["mips"]]
            itrs_values = sorted({r["itrs"] for r in parsed_runs if r["itrs"]})
            result_values = sorted({r["result"] for r in parsed_runs if r["result"]})

            row = {
                "benchmark": "sum_tree",
                "mode": mode_name,
                "n": n,
                "runs_requested": args.runs,
                "runs_ok": len(parsed_runs),
                "result_values": ";".join(result_values),
                "expected_result": expected,
                "itrs_values": ";".join(itrs_values),
                "avg_time_seconds": statistics.mean(time_values) if time_values else "",
                "avg_time_us": statistics.mean(time_values) * 1_000_000 if time_values else "",
                "min_time_us": min(time_values) * 1_000_000 if time_values else "",
                "max_time_us": max(time_values) * 1_000_000 if time_values else "",
                "stdev_time_us": statistics.stdev(time_values) * 1_000_000 if len(time_values) > 1 else "",
                "avg_mips": statistics.mean(mips_values) if mips_values else "",
                "min_mips": min(mips_values) if mips_values else "",
                "max_mips": max(mips_values) if mips_values else "",
                "avg_wall_time_us": statistics.mean(wall_times) * 1_000_000 if wall_times else "",
                "failures": len(failures),
                "first_failure": failures[0] if failures else "",
                "program_file": str(program),
            }
            rows.append(row)
            print(f"{mode_name} n={n}: ok={row['runs_ok']}/{args.runs}, avg_time_us={row['avg_time_us']}")

    fieldnames = [
        "benchmark", "mode", "n", "runs_requested", "runs_ok", "result_values",
        "expected_result", "itrs_values", "avg_time_seconds", "avg_time_us",
        "min_time_us", "max_time_us", "stdev_time_us", "avg_mips", "min_mips",
        "max_mips", "avg_wall_time_us", "failures", "first_failure", "program_file",
    ]
    with args.out.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    print(args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
