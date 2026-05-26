# Try 36: Selected-Command Completion Service

Current status: `failed_csynth_resources`

Branch: `try36`

Architecture: preserve try35 single-site refill pop, but split selected-command
success completion so `executed_steps` is updated in a narrow completion
service instead of inside selected-command execution.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try35 completed synthesis with the best focused parallel result so far (`4003 LUT`, `2114 FF`, `II=1`, slack `-2.05`), and moved the critical path away from global queue pop/count into `executed_steps` completion accounting. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION`, `PARALLEL_SERVICE_COMPLETE_SELECTED`, completion metadata registers, and a focused completion-service synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full CSIM (`a36`), and FPGA-small CSIM (`b36`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `4091 LUT`, `2117 FF`, `0 DSP`, `0 BRAM`, and slack `-2.86`, which is worse than the try35 base. |

## Goal

Reduce the selected-command scheduler critical path now that try35 removed the
global queue pop/count path from the limiter.

## Hypothesis

The try35 report shows the critical path from the loop condition through
`local_status_code` control and into the `executed_steps` select/store. This
suggests selected-command execution is still doing too much completion work in
one service tick. Moving successful completion into a separate service should
shorten the execute/control path, similar to the earlier single-lane
deferred-completion result, while preserving interaction semantics.

## Difference From Prior Tries

Try33 deferred error/status and failed to meaningfully improve timing. Try36
does not use that broad pending-status service. Try35 fixed the refill pop
datapath. Try36 keeps that improvement and targets a different limiter:

```text
selected command executes
selected command records completion metadata
completion service increments executed_steps
commit service flushes produced pairs
```

The new boundary is around success completion, not error reporting or queue
refill.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION`
  - default `0`
- Add a service phase:
  - `PARALLEL_SERVICE_COMPLETE_SELECTED`
- Use this gate only in the selected-command focused profile.
- Add completion state:
  - `selected_command_completion_valid`
  - `selected_command_completion_lane_index`
  - `selected_command_completion_rule_kind`
- In selected-command execution:
  - when a non-`LINK` rule commits successfully, record completion state instead
    of incrementing `executed_steps`;
  - when staged `LINK` finishes successfully, record completion state instead of
    incrementing `executed_steps`;
  - keep error/status behavior direct and unchanged.
- In the completion service:
  - count the executed rule;
  - increment `executed_steps`;
  - increment the lane committed-step counter;
  - clear completion state;
  - move to COMMIT.
- Preserve current behavior when the gate is disabled.
- Keep try35 gates enabled in the focused synthesis config:
  - split selected classify
  - split LINK writeback
  - split refill
  - single-site refill pop
- Do not enable try33 pending status.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try36 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a36
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b36
```

Focused completion-service CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_completion_nostats.cfg --work_dir csyn_t36_parallel_completion
```

## Results

- Host syntax: PASS
- Try36 focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the full testbench still expects them.
- Full CSIM: PASS (`a36`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`b36`, `Overall Result: PASS`)
- CSYNTH: completed, kept scheduler `II=1`, but failed timing/resource
  acceptance compared with the try35 base.

## HLS Evidence

- Work directory: `csyn_t36_parallel_completion`
- Report paths:
  - `csyn_t36_parallel_completion/hls/syn/report/csynth.rpt`
  - `csyn_t36_parallel_completion/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t36_parallel_completion/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_899_3_csynth.rpt`
  - `csyn_t36_parallel_completion/hls/syn/report/csynth_design_size.rpt`
- LUT: `4091` top-level total, `3923` in the scheduler pipeline
- FF: `2117` top-level total, `2046` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_899_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-2.86`; estimated Fmax
  `98.40 MHz`; estimated clock period `10.162 ns`
- Critical path evidence: Vitis reported a path from
  `selected_command_produced_active_pairs.produced_pairs.left_port` through
  non-link rule/port checks and the `local_status_code` phi/store. It also
  reported a separate unmet path from the loop condition to the
  `parallel_service_phase` store at `select_ln1715` with `11.4196 ns`
  combination delay. This means the completion-service split removed
  `executed_steps` from the reported limiter, but it pushed pressure into the
  selected-command phase transition/status mux.
- Design-size evidence: final HW-transform design size is `864` instructions,
  up from try35's `808`. The design-size report still shows one
  `active_pair_queue_pop_global` contribution at `452` instructions, so the
  try35 single-pop refill property was preserved.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try36 is a useful negative result. The hypothesis was only partially right:
moving successful completion out of selected-command execution removed
`executed_steps` from the visible critical path, but the added completion phase
and `selected_command_completion_valid` routing widened the service-phase and
status-control muxes. Compared with try35, the design got larger and slower:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try35 single-site refill pop | 4003 | 2114 | 0 | 0 | 1 | `-2.05` | `107.01 MHz` |
| Try36 selected completion service | 4091 | 2117 | 0 | 0 | 1 | `-2.86` | `98.40 MHz` |

The architectural lesson is that success-completion deferral is not free in the
parallel selected-command controller. Because selected-command execution already
feeds `local_status_code`, produced-pair state, staged `LINK` state, and
service-phase selection, adding a completion-valid branch increases the same
control fan-in that the try was meant to reduce.

## Next Recommendation

Return to try35 as the better base. The next try should target the current
try36 limiter directly:

- reduce selected-command produced-pair state width, especially the complete
  `selected_command_produced_active_pairs.produced_pairs` array that now appears
  in the critical path;
- or split non-link `ERASE`/`ANNIHILATE` produced-pair append from status
  classification, rather than adding a generic completion service;
- keep the try35 single-site refill pop;
- do not keep `ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION` enabled in the next
  focused synthesis profile.
