# Chat export: CUDA continuation context

Date: 2026-06-12

This file is a practical export of the current working context from the Codex chat. It is intended for continuing the CUDA/HVM2 experiment on a Linux machine.

## Diploma/project context

Topic: `Апаратна система паралельного виконання функціональних програм`.

The project implements a hardware-oriented reducer for interaction-combinator style networks. The reducer is written in C++ for AMD Vitis HLS. The important comparison is between:

- reducer C/RTL measurements from Vitis HLS;
- HVM2 as an existing implementation of interaction-net style computation;
- preferably HVM2 CUDA on Linux, because Windows currently has no `nvcc` in PATH.

## User preference for thesis wording

Use direct Ukrainian wording. Avoid:

- `runtime`, `рантайм`;
- `прогін`;
- artificial defensive starts like `не є лише...`;
- overexplaining what is obvious;
- calling old intermediate design variants `спроби` in the thesis text.

Use terms like:

- `журнал виконання`;
- `канали редукції`;
- `активні пари`;
- `такти RTL`;
- `логічні цикли планувальника`;
- `накладні витрати потокового запуску`.

## Current reducer measurements

Stable reducer data is in:

`docs/results/raw/email_snapshots/sum_sweep_reducer_sum_tree_rtl_20260528_092216.csv`

Do not use `docs/results/raw/sum_sweep_reducer_sum_tree_rtl_current.csv` as the thesis source unless rerun fully, because it was partially overwritten by interrupted runs.

Important reducer values:

| n | best lanes | active pairs | OPERATE | logical scheduler cycles | RTL cycles | time, us |
|---:|---:|---:|---:|---:|---:|---:|
| 16 | 4 | 8 | 8 | 2 | 196 | 1.96 |
| 32 | 8 | 16 | 16 | 2 | 252 | 2.52 |
| 64 | 16 | 32 | 32 | 2 | 364 | 3.64 |
| 128 | 32 | 64 | 64 | 2 | 716 | 7.16 |

Important nuance: `reducer_sum_tree_parallel_cosim` currently measures a direct parallel first-frontier ADD scenario. It builds independent active pairs `(0+1), (2+3), ...`, executes them through reduction lanes, then the top function reads partial results and sums them for checking. It is useful for measuring independent active-pair parallelism, but it should not be described as a full multi-level tree reduction performed entirely by the generic reducer.

## Parallel journal

Generated file:

`docs/results/raw/parallel_sum_tree_journal_n128_m32_20260611.csv`

For `n=128`, `m=32`, it shows two scheduler cycles:

- cycle 1: pair indexes 0..31;
- cycle 2: pair indexes 32..63.

Example rows:

| cycle | lane | active pair | operation | result |
|---:|---:|---|---|---:|
| 1 | 0 | `U24(1) <-> OPE(0)` | `0 + 1` | 1 |
| 1 | 1 | `U24(3) <-> OPE(1)` | `2 + 3` | 5 |
| 2 | 30 | `U24(125) <-> OPE(62)` | `124 + 125` | 249 |
| 2 | 31 | `U24(127) <-> OPE(63)` | `126 + 127` | 253 |

## HVM2 status

Local Windows HVM2 source path was:

`C:\diploma\HVM2-main`

HVM2 Rust `hvm run` was built and works. It is single-threaded in `src/main.rs`:

```rust
let mut tm = hvm::TMem::new(0, 1);
tm.evaluator(&net, &book);
```

The timer in the patched Rust version is around `tm.evaluator(&net, &book)`, so it measures evaluator time, not process startup time.

Rust baseline from 100 runs:

| n | result | ITRS | avg time, us | avg MIPS |
|---:|---:|---:|---:|---:|
| 16 | 120 | 38 | 5.791 | 6.777 |
| 32 | 496 | 78 | 10.450 | 10.369 |
| 64 | 2016 | 158 | 10.395 | 15.805 |
| 128 | 8128 | 318 | 18.469 | 18.230 |

CSV:

`docs/results/raw/hvm2_rust_sum_tree_rerun_20260611.csv`

## Old HVM2 threaded C results

Old file:

`docs/results/raw/sum_sweep_hvm_sum_tree_n16_128_threads2_n_current.csv`

Those results are not good as the main thesis comparison. The generated HVM2 C code creates pthreads at execution time and contains:

```c
// TODO: cache threads to avoid spawning overhead
```

For the small benchmark examples, thread creation and synchronization dominate. Some larger thread counts produced segmentation faults. If mentioned, describe them as evidence of software threading overhead, not as a clean HVM2 scaling result.

## CUDA plan

The Windows machine sees the GPU through `nvidia-smi`:

- GPU: NVIDIA GeForce GTX 1650
- Driver: 580.97
- CUDA version shown by driver: 13.0

But `nvcc` is not available in PATH, so HVM2 could not be built with CUDA support here.

On Linux, install CUDA Toolkit and make sure:

```bash
nvidia-smi
nvcc --version
```

both work.

Then build HVM2 and run:

```bash
python3 tools/hvm2_cuda_sum_tree_sweep.py \
  --hvm /path/to/HVM2/target/release/hvm \
  --mode cuda \
  --runs 30 \
  --sizes 16 32 64 128 \
  --out docs/results/raw/hvm2_cuda_sum_tree_linux.csv
```

If CUDA works, use `hvm2_cuda_sum_tree_linux.csv` for the final comparison. If it fails, keep the Rust HVM2 baseline and explain CUDA as unavailable/unstable in the available environment.

## Files to check first on Linux

1. `docs/linux_cuda_handoff_20260612.md`
2. `tools/hvm2_cuda_sum_tree_sweep.py`
3. `docs/hvm2_precise_timing.patch`
4. `docs/results/parallel_reducer_log_and_hvm2_check_20260611.md`
5. `docs/results/raw/parallel_sum_tree_statistics_from_snapshot_20260611.csv`
