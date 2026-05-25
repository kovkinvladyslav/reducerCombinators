# Try 1: Staged FPGA-Small CALL

Current status: `planned`

Branch: `try1`

Architecture: staged single-lane `CALL`

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-25 | `planned` | Staged `CALL` architecture plan recorded before implementation. |

## Goal

Implement `CALL` in a hardware-appropriate form for the FPGA-small reducer while
preserving the existing functional `REF/CALL` semantics.

## Hypothesis

`CALL` is functionally correct but architecturally misplaced. Moving packed-book
instantiation from an inlined hot-loop rule into a bounded multi-cycle
instantiation stage should avoid the synthesis explosion caused by putting node
copying, variable initialization, redex emission, and address rewriting inside a
single pipelined reducer body.

## Difference From Prior Tries

This is the first recorded try in the persistent architecture log.

## Correct FPGA-Oriented `CALL` Implementation Plan

### Summary

`CALL` is functionally implemented today, but HLS evidence shows it is
architecturally misplaced: `apply_call_rule` performs full packed-book
instantiation inside the reducer's pipelined hot loop. The correct next phase is
to preserve current `REF/CALL` semantics while converting `CALL` into a bounded
multi-cycle instantiation stage.

The implementation will keep the existing full simulation behavior available,
add a staged FPGA-small `CALL` path, and document the measured evidence for
Chapter 3/4 comparison with HVM/HVM2-style runtime architecture.

### Key Changes

- Add a compile-time gate:
  - `ENABLE_STAGED_CALL`
  - default `0` for `FULL_SIMULATION_PROFILE`
  - default `1` for `FPGA_SMALL_PROFILE`
- Keep the existing `apply_call_rule` as the reference semantic implementation
  for full CSIM and regression comparison.
- Add a staged `CALL` instantiation state machine for the single-lane reducer:
  - detect `INTERACTION_RULE_CALL`
  - validate reference/header/capacity
  - capture `reference_port`, `call_argument_port`, `definition_header`
  - reserve fresh node/variable base ranges
  - initialize one fresh variable per service tick
  - copy/adjust one book node per service tick
  - push one adjusted book redex per service tick
  - push the adjusted root/application redex
- Change the reducer main loop condition so an active staged `CALL` transaction
  can finish even when `maximum_steps` has been reached:
  - `maximum_steps` still counts interaction reductions
  - internal `CALL` service ticks do not increment `performed_steps`
  - the completed `CALL` increments `performed_steps` exactly once
- Preserve existing status behavior:
  - unknown reference -> `REDUCER_STATUS_UNKNOWN_REFERENCE`
  - node capacity -> `REDUCER_STATUS_NODE_MEMORY_OVERFLOW`
  - variable capacity -> `REDUCER_STATUS_VARIABLE_MEMORY_OVERFLOW`
  - book node bounds -> `REDUCER_STATUS_BOOK_NODE_OVERFLOW`
  - book redex bounds -> `REDUCER_STATUS_BOOK_REDEX_OVERFLOW`
  - malformed adjusted book port -> `REDUCER_STATUS_UNSUPPORTED_INTERACTION`
  - queue capacity -> `REDUCER_STATUS_REDEX_QUEUE_OVERFLOW`
- Keep `CALL` disabled in the experimental parallel reducer for FPGA-small
  synthesis until the single-lane staged path is validated.

### Implementation Details

- Add a small `CallInstantiationState` struct with:
  - active flag
  - phase enum: idle, init variables, copy nodes, push redexes, push root, finish
  - reference/call argument ports
  - captured `BookDefinitionHeader`
  - fresh node and variable base indices
  - variable/node/redex loop indices
  - adjustment/status flags
- Add helper functions:
  - `begin_staged_call(...)`
  - `step_staged_call(...)`
  - `finish_staged_call_status(...)`
- In `reducer_core.cpp`, route `INTERACTION_RULE_CALL` as:
  - old `apply_call_rule` when `ENABLE_STAGED_CALL == 0`
  - staged transaction when `ENABLE_STAGED_CALL == 1`
- Add a synthesis config:
  - `hls_config_synth_fpga_small_staged_call.cfg`
  - flags:
    `-DFPGA_SMALL_PROFILE=1 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -DENABLE_STAGED_CALL=1`
- Add a CSIM config or test mode for small staged call with testbench flags also
  defining `ENABLE_STAGED_CALL=1`.

### Documentation

- Add a dedicated architecture note, for example
  `docs/call_architecture_notes.md`, covering:
  - current evidence: full/small with inline `CALL` stalls after HLS performance
    transforms
  - completed no-call synthesis result: about `4202 LUT`, `2283 FF`, `2 DSP`,
    main loop `II=4`
  - interpretation: core reducer is FPGA-feasible; inline `CALL` is the
    synthesis bottleneck
  - why `CALL` differs from small interaction rules: it is bounded graph
    instantiation, not a constant-size rewrite
  - comparison angle for HVM/HVM2: runtime-level `REF/CALL` expansion maps
    naturally to a separate hardware service
  - planned diagrams:
    - hot reducer loop without `CALL`
    - staged `CALL` instantiation FSM
    - book memory to live graph memory dataflow
- Update `docs/runtime_profiles.md`:
  - mark inline `CALL` as functionally implemented
  - mark FPGA-small staged `CALL` as the next synthesis-oriented implementation
  - record the no-call synthesis numbers as evidence, not as final full-runtime
    results.

### Test Plan

- Run existing full-profile host/CSIM tests unchanged.
- Run FPGA-small host/CSIM tests with staged `CALL` enabled.
- Add targeted staged `CALL` tests:
  - identity/root call
  - constructor-node instantiation
  - internal active-pair redex emission
  - nested reference behavior already covered by current book tests
  - safe reference duplication path
  - unknown reference status
  - node overflow status
  - variable overflow status
  - redex overflow status
  - malformed local book port gives unsupported interaction
  - `performed_steps` increments once for a completed `CALL`
  - `maximum_steps=1` with a `CALL` that emits follow-up redexes reports
    max-steps reached after completing that one call
- Run synthesis in this order:
  - existing no-call small config as control
  - new staged-call small config
  - compare LUT, FF, DSP, timing, II, and design-size report.

### Assumptions

- The first correct implementation target is the single-lane reducer.
- Parallel `CALL` support remains planned until single-lane staged `CALL` is
  synthesizable and measured.
- The old inline `CALL` remains as the semantic reference path.
- The staged implementation prioritizes synthesizable architecture and
  measurable behavior over immediate II=1.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try1
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir interactReduce_csim_small_try1
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_staged_call.cfg --work_dir interactReduce_csynth_small_staged_call_try1
```

## Expected Comparison Baseline

Existing control build:

```text
interactReduce_csynth_small_nostats_nocall
```

Measured result:

```text
LUT: 4202
FF: 2283
DSP: 2
Main loop II: 4
```

## Success Criteria

- Full profile CSIM still passes.
- FPGA-small staged-call CSIM passes.
- Staged-call synthesis completes.
- Resource growth over the no-call baseline is explainable.

## Failure Criteria

- CSIM mismatch.
- Synthesis freezes after Performance/HW transforms.
- Resources return to the old explosion pattern.
- Timing/control path becomes clearly worse than the no-call baseline.

## Results

Fill this after implementation.

- Host syntax:
- Host execution:
- Full CSIM:
- FPGA-small CSIM:
- CSYNTH:

## HLS Evidence

Fill this when reports or logs exist.

- Work directory:
- Report paths:
- LUT:
- FF:
- DSP:
- BRAM:
- II:
- Timing/slack:
- Last Vitis log line if frozen:

## Conclusion

Pending implementation.

## Next Recommendation

Pending implementation result.

