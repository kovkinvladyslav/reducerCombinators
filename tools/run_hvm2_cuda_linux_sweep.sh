#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
hvm_dir="${HVM2_DIR:-/tmp/HVM2}"
hvm_bin="${hvm_dir}/target/release/hvm"
runs="${HVM2_CUDA_RUNS:-30}"
sizes="${HVM2_CUDA_SIZES:-16 32 64 128}"
out="${HVM2_CUDA_OUT:-${repo_root}/docs/results/raw/hvm2_cuda_sum_tree_linux.csv}"
log_dir="${HVM2_CUDA_LOG_DIR:-${repo_root}/docs/results/raw/hvm2_cuda_logs}"
sanity_timeout="${HVM2_CUDA_SANITY_TIMEOUT:-180}"
sweep_timeout="${HVM2_CUDA_TIMEOUT:-180}"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'ERROR: required command not found: %s\n' "$1" >&2
    return 1
  fi
}

require_command git
require_command cargo
require_command python3
require_command nvidia-smi
require_command nvcc
require_command gcc-12
require_command g++-12

nvidia-smi >/dev/null
nvcc --version >/dev/null
mkdir -p "${log_dir}"

if [[ ! -d "${hvm_dir}/.git" ]]; then
  git clone https://github.com/HigherOrderCO/HVM2.git "${hvm_dir}"
fi

if ! git -C "${hvm_dir}" apply --check "${repo_root}/docs/hvm2_precise_timing.patch" >/dev/null 2>&1; then
  printf 'HVM2 timing patch is already applied or does not match this checkout.\n' >&2
else
  git -C "${hvm_dir}" apply "${repo_root}/docs/hvm2_precise_timing.patch"
fi

echo "Building HVM2 with CUDA support..."
CC=gcc-12 \
CXX=g++-12 \
HOST_CC=gcc-12 \
HOST_CXX=g++-12 \
cargo build --release --manifest-path "${hvm_dir}/Cargo.toml" 2>&1 | tee "${log_dir}/hvm2_cuda_build.log"

echo "Running CUDA sanity check: n=16, timeout=${sanity_timeout}s..."
sanity_log="${log_dir}/hvm_run_cuda_sanity_n16.log"
if ! timeout "${sanity_timeout}" "${hvm_bin}" run-cu "${repo_root}/docs/results/raw/hvm2_generated/hvm_sum_tree_16.hvm" >"${sanity_log}" 2>&1; then
  printf 'ERROR: HVM2 run-cu sanity command failed or timed out. Log: %s\n' "${sanity_log}" >&2
  tail -80 "${sanity_log}" >&2 || true
  exit 1
fi
if ! rg -q '^Result: 120$' "${sanity_log}"; then
  printf 'ERROR: HVM2 run-cu sanity check failed for n=16.\n' >&2
  tail -80 "${sanity_log}" >&2 || true
  exit 1
fi
tail -20 "${sanity_log}"

echo "Running CUDA sweep: runs=${runs}, sizes=${sizes}, timeout=${sweep_timeout}s, logs=${log_dir}"
python3 "${repo_root}/tools/hvm2_cuda_sum_tree_sweep.py" \
  --hvm "${hvm_bin}" \
  --mode cuda \
  --runs "${runs}" \
  --sizes ${sizes} \
  --timeout "${sweep_timeout}" \
  --log-dir "${log_dir}" \
  --out "${out}"

python3 - "${out}" <<'PY'
import csv
import sys

path = sys.argv[1]
with open(path, newline="", encoding="utf-8") as file:
    rows = list(csv.DictReader(file))

bad = [
    row for row in rows
    if row["mode"] != "hvm_run_cuda"
    or int(row["runs_ok"]) != int(row["runs_requested"])
    or row["result_values"] != row["expected_result"]
]
if bad:
    print("ERROR: CUDA sweep did not complete cleanly.", file=sys.stderr)
    for row in bad:
        print(row, file=sys.stderr)
    raise SystemExit(1)

print(f"Validated CUDA sweep: {path}")
PY
