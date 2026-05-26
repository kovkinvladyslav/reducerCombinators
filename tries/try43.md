# Try 43: Staged Non-Link Validation

Current status: `failed_csynth_resources`

Branch: `try43`

Architecture: preserve the try41 staged `LINK` decide/write split and stage
selected-command non-link node validation before produced-pair emission.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try42 synthesized to the same `3707 LUT`, `1150 FF`, `II=1`, and `-1.16 ns` point as try41; direct produced-pair writes did not remove the critical path through non-link node validation and `local_status_code`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_STAGED_NON_LINK_VALIDATION`, selected non-link validation registers, a two-pass `CLASSIFY` validation flow, and a focused synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, full CSIM (`a43_final`), and FPGA-small CSIM (`b43_final`) passed. An earlier parallel full/small CSIM launch produced a Vitis `csim.exe` invalid-argument error, then passed when rerun sequentially. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with `3754 LUT`, `1288 FF`, `0 DSP`, `0 BRAM`, scheduler `II=1`, estimated Fmax `81.81 MHz`, and `-4.92 ns` slack; timing is much worse than try41/try42. |

## Goal

Attack the post-try41 critical path more directly by separating selected
`ERASE`/`ANNIHILATE` validation from produced-pair emission and completion.

## Hypothesis

Try42 shows that direct two-slot output writes are not enough. The remaining
path couples selected produced-pair/tag state, node-port selection, node address
range checking, and `local_status_code` storage. If the selected non-link path
uses one service tick to validate ports/read node data and a later service tick
to emit produced pairs and count the rule, the reported critical path may move
or shorten.

The expected tradeoff is one extra service tick for selected-command
`ERASE`/`ANNIHILATE` in the focused parallel profile.

## Difference From Prior Tries

Try42 specialized produced-pair writes but kept validation, emission, and
completion in one selected non-link execution service. Try43 changes the
schedule shape: validation happens first, emission happens later. This is a
staging experiment, not another append specialization.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_STAGED_NON_LINK_VALIDATION`
  - default `0`
- Reuse the existing `PARALLEL_SERVICE_CLASSIFY` service as a two-pass selected
  command service, avoiding a wider `ReducerServicePhase`.
- Add selected non-link staging registers:
  - validation pending flag
  - validated flag
  - two captured produced-pair outputs
- During selected-command classify:
  - first classify the rule kind;
  - for focused `ERASE`/`ANNIHILATE`, request a second classify tick for
    validation;
  - otherwise preserve the existing execute path.
- During validation:
  - pop the selected local active pair;
  - validate node address bounds;
  - read required node data;
  - capture the two produced pairs into staging registers;
  - set `local_status_code` only on validation errors.
- During execute:
  - if validated non-link output is pending, write the two produced pairs into
    `selected_command_produced_active_pairs`, count one executed step, and move
    to commit.
- Preserve the old path when the gate is off.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try43 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_STAGED_NON_LINK_VALIDATION=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a43
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b43
```

Focused staged-non-link CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_direct_link_decide_write_split_staged_nonlink_nostats.cfg --work_dir csyn_t43_parallel_staged_nonlink
```

## Results

- Host syntax: PASS
- Try43 focused syntax: PASS for `src/reducer_core_parallel.cpp`.
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the full testbench still expects them.
- Full CSIM: PASS (`a43_final`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`b43_final`, `Overall Result: PASS`)
- CSYNTH: completed, but timing and FF count regressed significantly versus
  try41/try42.

## HLS Evidence

- Work directory: `csyn_t43_parallel_staged_nonlink_final`
- Report paths:
  - `csyn_t43_parallel_staged_nonlink_final/hls/syn/report/csynth.rpt`
  - `csyn_t43_parallel_staged_nonlink_final/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t43_parallel_staged_nonlink_final/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_985_3_csynth.rpt`
  - `csyn_t43_parallel_staged_nonlink_final/hls/syn/report/csynth_design_size.rpt`
- LUT: `3754` top-level total, `3596` in the scheduler pipeline
- FF: `1288` top-level total, `1217` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_985_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`.
- Timing/slack: estimated clock period `12.223 ns`, estimated Fmax
  `81.81 MHz`. With the configured `2.70 ns` uncertainty, this is `-4.92 ns`
  against the effective `7.30 ns` budget.
- Critical path evidence: Vitis reports a path from
  `selected_command_produced_active_pairs.produced_pairs.right_port` through
  `get_interaction_rule_kind`, the selected rule-kind sparse mux, and
  `parallel_service_phase` selection/store. This is worse than try41's
  non-link node-validation path.
- Design-size evidence: final HW-transform design size is `571`
  instructions, up from try41/try42 `560`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try43 is a negative result. It is semantically correct and still achieves
`II=1`, but the staging implementation makes the selected-command controller
heavier. Instead of shortening the `ERASE`/`ANNIHILATE` validation path, it
moves the top path into rule classification and service-phase selection:

```text
selected produced-pair state
get_interaction_rule_kind
selected_command_rule_kind mux
parallel_service_phase select/store
```

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try42 direct non-link outputs | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try43 staged non-link validation | 3754 | 1288 | 0 | 0 | 1 | `-4.92` | `81.81 MHz` |

The lesson is specific: reusing `PARALLEL_SERVICE_CLASSIFY` for a second
validation tick is not free. It creates a longer classify/control path than the
node validation path it was meant to avoid.

## Next Recommendation

Return to try41 as the preferred focused base. The next try should not stage
`ERASE`/`ANNIHILATE` through the existing classify controller. A better
different approach is to reduce classification/control fan-in directly, for
example by precomputing a one-bit selected-command class during SELECT:

```text
is_link
is_simple_non_link
is_other
```

Then EXECUTE can route on a small class field instead of carrying the full
interaction-rule decode through service-phase selection.
