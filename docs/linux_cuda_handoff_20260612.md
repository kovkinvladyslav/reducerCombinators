# Linux/CUDA handoff for HVM2 comparison

Date: 2026-06-12
Project: hardware-oriented interaction-combinator reducer
Repo path on Windows: `C:\diploma\interactReduce`

## Goal

Continue the CUDA comparison on a Linux machine with NVIDIA CUDA Toolkit installed. The Windows machine has an NVIDIA GTX 1650 visible through `nvidia-smi`, but `nvcc` is not available in PATH, so HVM2 cannot be rebuilt with the CUDA feature here.

The useful experiment is:

1. Build HVM2 with CUDA support on Linux.
2. Generate equivalent HVM2 `sum_tree` examples for `n = 16, 32, 64, 128`.
3. Run `hvm run-cu` for those examples.
4. Compare CUDA HVM2 timings with reducer C/RTL results from the stable snapshot.

## Files added for the handoff

- `tools/hvm2_cuda_sum_tree_sweep.py` - generates HVM2 sum-tree programs and measures `hvm run`, `hvm run-cu`, or both.
- `docs/hvm2_precise_timing.patch` - optional HVM2 patch that prints TIME and MIPS with more precision.
- `docs/results/parallel_reducer_log_and_hvm2_check_20260611.md` - current local analysis of reducer statistics and HVM2 Rust rerun.
- `docs/results/raw/parallel_sum_tree_journal_n128_m32_20260611.csv` - journal of the deterministic parallel reducer scenario for `n=128`, `m=32`.
- `docs/results/raw/parallel_sum_tree_statistics_from_snapshot_20260611.csv` - reduced summary extracted from the stable reducer C/RTL snapshot.
- `docs/results/raw/hvm2_rust_sum_tree_rerun_20260611.csv` - stable Rust `hvm run` baseline.

## Current reducer reference data

Use the stable reducer snapshot, not `*_current.csv`, because `current` was partially overwritten by interrupted reruns.

Stable file:

`docs/results/raw/email_snapshots/sum_sweep_reducer_sum_tree_rtl_20260528_092216.csv`

Best reducer C/RTL values from that snapshot:

| n | best lanes | active pairs | OPERATE | logical scheduler cycles | RTL cycles | time, us |
|---:|---:|---:|---:|---:|---:|---:|
| 16 | 4 | 8 | 8 | 2 | 196 | 1.96 |
| 32 | 8 | 16 | 16 | 2 | 252 | 2.52 |
| 64 | 16 | 32 | 32 | 2 | 364 | 3.64 |
| 128 | 32 | 64 | 64 | 2 | 716 | 7.16 |

For `n=128`, full reducer lane line:

| lanes | active pairs | OPERATE | logical scheduler cycles | RTL cycles | time, us |
|---:|---:|---:|---:|---:|---:|
| 2 | 64 | 64 | 32 | 746 | 7.46 |
| 4 | 64 | 64 | 16 | 730 | 7.30 |
| 8 | 64 | 64 | 8 | 722 | 7.22 |
| 16 | 64 | 64 | 4 | 718 | 7.18 |
| 32 | 64 | 64 | 2 | 716 | 7.16 |
| 64 | 64 | 64 | 1 | 778 | 7.78 |
| 128 | 64 | 64 | 1 | 778 | 7.78 |

## Current HVM2 status on Windows

HVM2 Rust `hvm run` was rebuilt locally and works. It is single-threaded in `src/main.rs`:

```rust
let mut tm = hvm::TMem::new(0, 1);
tm.evaluator(&net, &book);
```

Measured Rust baseline, 100 runs:

| n | result | ITRS | avg time, us | avg MIPS |
|---:|---:|---:|---:|---:|
| 16 | 120 | 38 | 5.791 | 6.777 |
| 32 | 496 | 78 | 10.450 | 10.369 |
| 64 | 2016 | 158 | 10.395 | 15.805 |
| 128 | 8128 | 318 | 18.469 | 18.230 |

Old HVM2 C threaded data should not be used as primary comparison. The generated HVM2 C code creates pthreads during execution and contains:

```c
// TODO: cache threads to avoid spawning overhead
```

For small examples, thread creation and synchronization dominate the measured time. Some high-thread runs also ended with segmentation fault.

## Linux run status on 2026-06-12

This handoff was attempted on Ubuntu 24.04.4 LTS under kernel
`6.17.0-23-generic`.

Dependency setup could not install CUDA packages because `sudo` requires an
interactive password in this session:

```text
sudo: a terminal is required to read the password
sudo: a password is required
```

The local NVIDIA stack is also not ready for CUDA measurement:

```text
nvidia-smi: couldn't communicate with the NVIDIA driver
nvcc: command not found
```

HVM2 was cloned to `/tmp/HVM2`, `docs/hvm2_precise_timing.patch` was applied,
and `cargo build --release` completed. The build emitted:

```text
WARNING: CUDA compiler not found. HVM will not be able to run on GPU.
```

The requested CUDA sweep was still executed to capture the failure mode:

```bash
python3 tools/hvm2_cuda_sum_tree_sweep.py \
  --hvm /tmp/HVM2/target/release/hvm \
  --mode cuda \
  --runs 30 \
  --sizes 16 32 64 128 \
  --out docs/results/raw/hvm2_cuda_sum_tree_linux.csv
```

All CUDA rows failed with `CUDA runtime not available!`, which confirms that no
valid HVM2 CUDA timing can be reported from this machine yet.

The fallback Rust sweep did complete:

```bash
python3 tools/hvm2_cuda_sum_tree_sweep.py \
  --hvm /tmp/HVM2/target/release/hvm \
  --mode rust \
  --runs 100 \
  --sizes 16 32 64 128 \
  --out docs/results/raw/hvm2_rust_sum_tree_linux.csv
```

| n | runs ok | result | ITRS | avg time, us | min time, us | max time, us | avg MIPS |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 100 | 120 | 38 | 4.909 | 3.771 | 13.340 | 8.169 |
| 32 | 100 | 496 | 78 | 5.971 | 4.819 | 14.248 | 13.361 |
| 64 | 100 | 2016 | 158 | 9.484 | 7.194 | 73.544 | 18.131 |
| 128 | 100 | 8128 | 318 | 18.640 | 15.645 | 59.086 | 18.280 |

Use `docs/results/raw/hvm2_rust_sum_tree_linux.csv` as the Linux CPU fallback
baseline. Do not use `docs/results/raw/hvm2_cuda_sum_tree_linux.csv` as a timing
comparison; it documents environment failure only.


## Linux dependency setup

A setup helper is included:

```bash
bash tools/setup_linux_cuda_hvm2_deps.sh
```

It installs the common Ubuntu/Debian packages needed for this experiment:

```bash
sudo apt install -y build-essential clang git curl pkg-config python3 python3-venv python3-pip nvidia-cuda-toolkit
```

If `nvcc --version` still does not work after this step, install CUDA Toolkit from NVIDIA's official repository and add `/usr/local/cuda/bin` to `PATH`.

## Linux steps

Clone/build HVM2 separately:

```bash
git clone https://github.com/HigherOrderCO/HVM2.git
cd HVM2
# Optional: apply precise output patch from this repo
# git apply /path/to/reducerCombinators/docs/hvm2_precise_timing.patch
cargo build --release
./target/release/hvm run-cu /path/to/example.hvm
```

Run the sweep from this repo:

```bash
cd /path/to/reducerCombinators
python3 tools/hvm2_cuda_sum_tree_sweep.py \
  --hvm /path/to/HVM2/target/release/hvm \
  --mode cuda \
  --runs 30 \
  --sizes 16 32 64 128 \
  --out docs/results/raw/hvm2_cuda_sum_tree_linux.csv
```

If CUDA run-cu is unstable, also run Rust baseline on the Linux machine:

```bash
python3 tools/hvm2_cuda_sum_tree_sweep.py \
  --hvm /path/to/HVM2/target/release/hvm \
  --mode rust \
  --runs 100 \
  --sizes 16 32 64 128 \
  --out docs/results/raw/hvm2_rust_sum_tree_linux.csv
```

## Interpretation to use in the thesis

The reducer measurements are C/RTL measurements of the generated hardware model. HVM2 CUDA, if it works, is a GPU software/backend comparison for the same general computation style, not an FPGA measurement.

The strongest comparison will be:

- reducer C/RTL: number of RTL cycles and calculated time;
- HVM2 CUDA: ITRS, TIME, MIPS from `hvm run-cu`;
- HVM2 Rust: fallback CPU baseline if CUDA fails.

For the reducer, mention that the reported logical scheduler cycles and RTL cycles are different. Logical cycles describe how many batches of independent active pairs are handled by the parallel scheduler. RTL cycles include HLS control, initialization, result readback, and generated hardware scheduling.
