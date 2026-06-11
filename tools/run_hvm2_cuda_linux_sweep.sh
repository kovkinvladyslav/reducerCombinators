#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
hvm_dir="${HVM2_DIR:-/tmp/HVM2}"
hvm_bin="${hvm_dir}/target/release/hvm"
runs="${HVM2_CUDA_RUNS:-30}"
sizes="${HVM2_CUDA_SIZES:-16 32 64 128}"
out="${HVM2_CUDA_OUT:-${repo_root}/docs/results/raw/hvm2_cuda_sum_tree_linux.csv}"

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

nvidia-smi >/dev/null
nvcc --version >/dev/null

if [[ ! -d "${hvm_dir}/.git" ]]; then
  git clone https://github.com/HigherOrderCO/HVM2.git "${hvm_dir}"
fi

if ! git -C "${hvm_dir}" apply --check "${repo_root}/docs/hvm2_precise_timing.patch" >/dev/null 2>&1; then
  printf 'HVM2 timing patch is already applied or does not match this checkout.\n' >&2
else
  git -C "${hvm_dir}" apply "${repo_root}/docs/hvm2_precise_timing.patch"
fi

cargo build --release --manifest-path "${hvm_dir}/Cargo.toml"

if ! "${hvm_bin}" run-cu "${repo_root}/docs/results/raw/hvm2_generated/hvm_sum_tree_16.hvm" | rg -q '^Result: 120$'; then
  printf 'ERROR: HVM2 run-cu sanity check failed for n=16.\n' >&2
  exit 1
fi

python3 "${repo_root}/tools/hvm2_cuda_sum_tree_sweep.py" \
  --hvm "${hvm_bin}" \
  --mode cuda \
  --runs "${runs}" \
  --sizes ${sizes} \
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
