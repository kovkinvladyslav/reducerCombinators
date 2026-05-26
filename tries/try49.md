# Try 49: Direct Selected ERASE Datapath

Current status: `failed_csynth_resources`

Branch: `try49`

Architecture: keep the try48 true class-only selected-command interface, then
specialize only the selected `ERASE` datapath without the generic non-link
rule-kind switch.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try48 restored timing to `118.28 MHz`, but remained at `3791 LUT`, `1162 FF`, `II=1`, and about `-1.16 ns` slack. The critical path returned to selected non-link node-address/status production. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_DIRECT_SELECTED_ERASE`, a direct selected ERASE helper, and a focused synthesis config on top of the try48 class-only selected-command base. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a49`), and FPGA-small CSIM (`b49`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed at `3661 LUT`, `1165 FF`, `II=1`, `118.28 MHz`, and about `-1.16 ns` slack. LUT improves over try48, but timing is unchanged and still not timing-clean. |

## Goal

Reduce selected non-link datapath/control pressure by specializing the focused
selected `ERASE` execution path while preserving interaction-combinator
semantics.

## Hypothesis

The remaining focused scheduler critical path is no longer selected-rule
classification. It runs through selected non-link node-address selection,
range checking, and `local_status_code` production.

A direct selected `ERASE` helper can avoid the generic
`execute_selected_non_link_active_pair_without_queue_commit` switch and the
`InteractionRuleKind` argument for one common non-link rule. This should test
whether one-rule datapath specialization is a better FPGA direction than
adding broad non-link service phases.

## Difference From Prior Tries

Try42 enabled direct non-link produced outputs inside the existing rule-kind
switch and matched try41. Try48 removed selected-command rule-kind
classification but still sent `ERASE` and `ANNIHILATE` through the generic
selected non-link helper.

Try49 is different because selected `ERASE` is executed directly from the
class-only selected path, without converting back to an `InteractionRuleKind`
and without using the generic non-link switch for that rule. `ANNIHILATE`
remains on the existing helper so the experiment measures one rule at a time.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_DIRECT_SELECTED_ERASE`
  - default `0`
- Add `execute_selected_erase_direct(...)`:
  - pop the selected local active pair;
  - initialize the produced-pair buffer;
  - choose erasure and binary-node ports;
  - validate the node address;
  - on invalid address, set `REDUCER_STATUS_UNSUPPORTED_INTERACTION`;
  - on success, emit two produced pairs directly into the compact buffer.
- In the try48 class-only selected EXECUTE path:
  - route class `ERASE` to the direct helper when the gate is enabled;
  - route class `ANNIHILATE` to the existing selected non-link helper;
  - keep `LINK`, `VOID`, and unsupported behavior unchanged.
- Add a focused synthesis config enabling the try48 base plus:
  - `ENABLE_PARALLEL_DIRECT_SELECTED_ERASE=1`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try49 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1 -DENABLE_PARALLEL_DIRECT_SELECTED_ERASE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a49
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b49
```

Focused direct-erase CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try49_direct_selected_erase_nostats.cfg --work_dir csyn_t49_parallel_direct_selected_erase
```

## Results

- Host syntax: PASS with the default full-profile testbench.
- Focused syntax: PASS for both `src/reducer_core_parallel.cpp` and
  `test/reducer_test.cpp` with the try49 flag set.
- Host execution: covered by Vitis CSIM runs.
- Full CSIM: PASS (`a49`, `Overall Result: PASS`).
- FPGA-small CSIM: PASS (`b49`, `Overall Result: PASS`).
- CSYNTH: completed. LUT improves versus try48, but the result does not beat
  the try41 baseline and remains timing-negative after clock uncertainty.

## HLS Evidence

- Work directory: `csyn_t49_parallel_direct_selected_erase`
- Report paths:
  - `csyn_t49_parallel_direct_selected_erase/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t49_parallel_direct_selected_erase/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1149_3_csynth.rpt`
  - `csyn_t49_parallel_direct_selected_erase/hls/syn/report/csynth_design_size.rpt`
- LUT: `3661` top-level total, `3503` in the scheduler pipeline.
- FF: `1165` top-level total, `1094` in the scheduler pipeline.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1149_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`.
- Timing/slack: estimated clock `8.455 ns`, estimated Fmax `118.28 MHz`, and
  about `-1.16 ns` slack against the effective `7.30 ns` budget.
- Final HW-transform design size: `591` instructions.
- Design-size evidence:
  - `execute_selected_erase_direct`: `1838` compile/link instructions.
  - `execute_selected_non_link_active_pair_without_queue_commit` remains
    present for `ANNIHILATE`: `4606` compile/link instructions, but its
    array/struct cost drops to `84` because selected `ERASE` is routed around
    the helper.
- Critical path evidence: the critical path is still the selected ERASE
  address/status path:

```text
selected_command_produced_active_pairs.produced_pairs.left_port
is_erasure_port / node-address select
node-address range check
local_status_code phi/store
```

- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try49 is a useful but insufficient result.

Specializing selected `ERASE` directly from the class-only path reduces LUT
from try48's `3791` to `3661`, and the scheduler pipeline LUT drops from
`3633` to `3503`. This supports the one-rule-specialization direction for
resource reduction.

However, timing does not improve. The estimated clock remains `8.455 ns` with
about `-1.16 ns` slack, because the path still includes node-address
selection, range checking, and `local_status_code` writeback. The direct helper
removes some switch/append overhead, but not the actual timing limiter.

## Next Recommendation

Keep the direct selected `ERASE` datapath as evidence, but the next try should
separate invalid-node status from the common successful `ERASE` path. A
candidate is a focused profile that assumes classification already guarantees a
binary-node port, captures only the node address in EXECUTE, and defers the
rare invalid-address status through a narrower pending-status path instead of
feeding `local_status_code` in the same cycle as produced-pair formation.
