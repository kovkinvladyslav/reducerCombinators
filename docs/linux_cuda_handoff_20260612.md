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

This handoff was completed on Ubuntu 24.04.4 LTS. The machine was first on
kernel `6.17.0-23-generic`, where NVIDIA userspace was installed but no matching
NVIDIA kernel module existed. CUDA was made usable by installing the HWE NVIDIA
580 open module/toolkit packages, rebooting into `6.17.0-35-generic`, and
building HVM2 with `gcc-12/g++-12`.

Post-reboot device/toolchain checks:

```text
GPU: NVIDIA GeForce GTX 1650
Driver: 580.159.03
Driver-reported CUDA version: 13.0
nvcc: CUDA compilation tools 12.0, V12.0.140
```

HVM2 CUDA required the local patch in `docs/hvm2_precise_timing.patch`. Besides
precise timing output, the patch:

- avoids the Windows `-rdynamic` link argument;
- uses a GTX-1650-compatible CUDA geometry and memory profile;
- moves CUDA `ROOT` inside the reduced variable memory;
- zero-initializes the CUDA graph state;
- adds CUDA error checks for launch/memory failures.

The validated CUDA sweep was run through the strict helper:

```bash
bash tools/run_hvm2_cuda_linux_sweep.sh
```

The helper rebuilt `/tmp/HVM2` with CUDA support, ran a `n=16` sanity check,
then ran `30` CUDA runs for each size. All rows passed validation. The host
time/MIPS columns are the end-to-end `hvm run-cu` timing printed by HVM2, while
the GPU columns are the fixed CUDA-kernel timing fields from `GPU_TIME` and
`GPU_MIPS`:

| n | runs ok | result | ITRS | avg host time, us | avg host MIPS | avg GPU time, us | min GPU time, us | max GPU time, us | avg GPU MIPS |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 16 | 30 | 120 | 38 | 12018.167 | 0.003162 | 15.487 | 11.104 | 44.480 | 2.716999 |
| 32 | 30 | 496 | 78 | 12086.400 | 0.006454 | 15.150 | 12.064 | 38.912 | 5.584008 |
| 64 | 30 | 2016 | 158 | 12298.600 | 0.012847 | 14.944 | 12.096 | 43.168 | 11.699298 |
| 128 | 30 | 8128 | 318 | 13494.333 | 0.023566 | 15.408 | 11.168 | 36.608 | 22.550795 |

CUDA source data:

- `docs/results/raw/hvm2_cuda_sum_tree_linux.csv`
- per-run logs in `docs/results/raw/hvm2_cuda_logs/`

The fallback Rust sweep also completed:

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

Use `docs/results/raw/hvm2_cuda_sum_tree_linux.csv` for the Linux CUDA
comparison, and `docs/results/raw/hvm2_rust_sum_tree_linux.csv` as the Linux CPU
fallback/baseline.


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

The strict helper can clone/build HVM2 separately under `/tmp/HVM2`, apply the
local CUDA patch, and run the sweep:

```bash
cd /path/to/reducerCombinators
bash tools/run_hvm2_cuda_linux_sweep.sh
```

To run the raw sweep manually after building a patched HVM2 binary:

```bash
python3 tools/hvm2_cuda_sum_tree_sweep.py \
  --hvm /path/to/HVM2/target/release/hvm \
  --mode cuda \
  --runs 30 \
  --sizes 16 32 64 128 \
  --log-dir docs/results/raw/hvm2_cuda_logs \
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
