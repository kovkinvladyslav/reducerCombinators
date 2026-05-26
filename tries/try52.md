# Try 52: Outlined Direct Selected ERASE Helper

Current status: `failed_csynth_resources`

Branch: `try52`

Architecture: keep the try49 direct selected `ERASE` semantics, but force the
direct selected `ERASE` helper to remain a separate HLS function/module instead
of being inlined into the monolithic scheduler loop.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try51 proved that moving selected `ERASE` validation between service arms still leaves one shared scheduler `local_status_code` phi. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_DIRECT_ERASE_NOINLINE`, applied `#pragma HLS INLINE off` to `execute_selected_erase_direct`, and added focused config `hls_config_try52_direct_erase_noinline_nostats.cfg`. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full-profile CSIM, and FPGA-small CSIM passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed and improved estimated Fmax to `131.66 MHz`, but the scheduler loop worsened from `II=1` to `II=3`. |

## Goal

Test whether a simple HLS module boundary around selected `ERASE` is enough to
stop Vitis from merging node-address validation and status production into the
main scheduler pipeline critical path.

## Hypothesis

Try51 showed that logical service staging inside the same pipelined loop is not
real architectural separation. Before attempting a larger multi-function
scheduler rewrite, try52 tests the smallest module-boundary step:

```text
selected EXECUTE tick -> noinline direct ERASE helper -> produced-pair buffer
```

If `#pragma HLS INLINE off` prevents the helper body from being flattened into
the scheduler loop, the reported top critical path may move away from:

```text
selected produced-pair left port
is_erasure_port / node-address select
node-address range check
local_status_code phi/store
```

The expected cost is a small call/control overhead. Semantics should remain the
same as try49 because the helper body is not changed.

## Difference From Prior Tries

Try50 and try51 moved status/validation between scheduler service arms. Try52
does not move validation and does not add another pending-status path.

This try is different because it tests HLS module outlining as the separation
mechanism. It keeps the direct selected `ERASE` implementation from try49 and
only changes whether HLS may inline that helper into the scheduler pipeline.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_DIRECT_ERASE_NOINLINE`
  - default `0`
- In `execute_selected_erase_direct(...)`, add:
  - `#pragma HLS INLINE off` when the new gate is enabled.
- Add focused config:
  - `hls_config_try52_direct_erase_noinline_nostats.cfg`
- Use the try49 direct selected `ERASE` base:
  - true class-only selected command enabled;
  - direct selected `ERASE` enabled;
  - try50 pending status disabled;
  - try51 prevalidation disabled.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try52 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1 -DENABLE_PARALLEL_DIRECT_SELECTED_ERASE=1 -DENABLE_PARALLEL_DIRECT_ERASE_NOINLINE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a52
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b52
```

Focused CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try52_direct_erase_noinline_nostats.cfg --work_dir csyn_t52_parallel_direct_erase_noinline
```

## Results

- Host syntax: passed for default testbench, focused top, and focused testbench syntax.
- Host execution: not run separately; covered by Vitis CSIM testbench.
- Full CSIM: passed with `hls_config.cfg` in work directory `a52`.
- FPGA-small CSIM: passed with `hls_config_fpga_small.cfg` in work directory `b52`.
- CSYNTH: completed in `csyn_t52_parallel_direct_erase_noinline`, but did not meet the scheduler-II requirement.

## HLS Evidence

- Work directory: `csyn_t52_parallel_direct_erase_noinline`
- Report paths:
  - `csyn_t52_parallel_direct_erase_noinline/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t52_parallel_direct_erase_noinline/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1195_3_csynth.rpt`
  - `csyn_t52_parallel_direct_erase_noinline/hls/syn/report/execute_selected_erase_direct_csynth.rpt`
  - `csyn_t52_parallel_direct_erase_noinline/hls/syn/report/csynth_design_size.rpt`
- LUT: `3821` top-level total, `3634` in the scheduler pipeline, `270` in the outlined selected-ERASE helper.
- FF: `1555` top-level total, `1484` in the scheduler pipeline, `136` in the outlined selected-ERASE helper.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1195_3` target `II=1`, achieved `II=3`; helper pipeline target `II=1`, achieved `II=1`.
- Timing/slack: top estimated clock `7.595 ns`, uncertainty `2.70 ns`, effective budget `7.30 ns`, estimated Fmax `131.66 MHz`; approximately `-0.295 ns` against the effective budget.
- Design size: `20,251` compile/link instructions and `763` HW-transform instructions.
- Last Vitis log line if frozen: not applicable; synthesis completed.

The helper boundary is visible in the reports: `execute_selected_erase_direct`
has its own report and reaches latency `1`, interval `1`. However, HLS reports
new scheduler-loop II violations:

```text
Unable to enforce a carried dependence constraint ... between load operation
on array node_memory and call operation ... to execute_selected_erase_direct

Unable to schedule call operation ... to execute_selected_erase_direct due to
limited resources (II = 2)

Pipelining result : Target II = 1, Final II = 3, Depth = 4, loop
VITIS_LOOP_1195_3

Loop Constraint Status: All loop constraints were NOT satisfied.
Estimated Fmax: 131.66 MHz
```

The critical path moved away from the previous inline node-address/status
chain and into the helper call/return path feeding scheduler control:

```text
call_ret / call_ret1 to execute_selected_erase_direct
local_status_code phis
local_status_code_0_write_ln2385
```

## Conclusion

Try52 is a mixed but rejected result.

The implementation preserves correctness under full and FPGA-small CSIM, and
it proves that a simple `INLINE off` pragma can create a real synthesized
module boundary around selected `ERASE`. That boundary improves the estimated
clock from the try49-try51 plateau of `118.28 MHz` to `131.66 MHz`.

However, the same boundary introduces a function-call scheduling dependency
inside the pipelined scheduler loop. The top scheduler loop worsens from
`II=1` to `II=3`, and resources also increase versus try49:

| Build | LUT | FF | Main loop II | Fmax |
| --- | ---: | ---: | ---: | ---: |
| Try49 direct selected `ERASE` | 3661 | 1165 | 1 | `118.28 MHz` |
| Try51 prevalidated selected `ERASE` | 3698 | 1300 | 1 | `118.28 MHz` |
| Try52 outlined selected `ERASE` helper | 3821 | 1555 | 3 | `131.66 MHz` |

For diploma use, this is useful evidence that module separation improves timing
only when it is also matched to the scheduler protocol. A noinline helper call
inside an `II=1` loop is not a free boundary; HLS must still schedule the call,
its return value, and memory dependence in the same loop-carried context.

## Next Recommendation

Do not keep noinline calls directly inside the `II=1` scheduler loop.

The next different approach should introduce a real protocol boundary rather
than only a C++ function boundary. Two suitable directions:

- move selected `ERASE` into a multi-cycle service phase with explicit request
  and response registers, so the helper is not called in the same loop
  iteration that owns the scheduler's local-queue and node-memory dependencies;
- or split the scheduler into separate HLS functions for classify/execute/commit
  with a small command record between them, then measure whether Vitis preserves
  module boundaries without creating a loop-carried call dependency.
