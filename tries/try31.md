# Try 31: Split Selected-Command Classification

Current status: `failed_csynth_resources`

Branch: `try31`

Architecture: preserve try30 staged `LINK`, but split selected-command active
pair capture from interaction-rule classification.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try30 reached scheduler `II=1` with staged `LINK`, but failed timing at `4770 LUT`, `2677 FF`, slack `-7.29`, with the critical path moving into selected-command capture/classification/control. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY`, widened `ReducerServicePhase` to 3 bits, added `PARALLEL_SERVICE_CLASSIFY`, and added a focused split-classify synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, try26 syntax guard, try31 focused syntax, full CSIM, and FPGA-small CSIM passed. The first long-name FPGA-small CSIM build compiled but Vitis failed to launch `csim.exe`; rerun in short work directory `t31s` passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `4602 LUT`, `2667 FF`, `0 DSP`, `0 BRAM`, but failed timing with slack `-4.35` and estimated Fmax `85.82 MHz`. |

## Goal

Recover timing from try30 without losing the useful staged-`LINK` scheduling
result. The focused structural parallel profile should still support:

```text
VOID
LINK
ERASE
ANNIHILATE
```

## Hypothesis

Try30 made the substitution-memory part of `LINK` schedulable, but the parent
scheduler still captures a lane candidate, classifies the rule, and stores
selected-command control in one service path. Registering the selected active
pair first and classifying it in the next service tick should shorten the
SELECT path and make the critical path more local.

## Difference From Prior Tries

This try does not add a second command slot, a waiting command, or another
execute datapath. It keeps try30's single selected command and staged `LINK`
service. The only new boundary is between:

```text
lane queue peek/capture
interaction-rule classification
```

That makes it different from try27/try28 command-buffer experiments and from
try30's staged-`LINK` experiment.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY`
  - default `0`
- Add a new parallel service phase:
  - `PARALLEL_SERVICE_CLASSIFY`
- Use the new phase only when:
  - `ENABLE_PARALLEL_SERVICE_PHASE`
  - `ENABLE_PARALLEL_SELECTED_COMMAND`
  - `ENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY`
- In selected-command SELECT:
  - capture only the lane index and active pair;
  - do not call `get_interaction_rule_kind`;
  - move to `PARALLEL_SERVICE_CLASSIFY`.
- In CLASSIFY:
  - classify the already-captured active pair;
  - move to EXECUTE;
  - keep the staged `LINK` path unchanged.
- Preserve the existing one-phase behavior when the gate is disabled.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_nostats.cfg`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try26 selected-command syntax guard:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try31 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t31_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t31_small
```

Focused split-classify CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_nostats.cfg --work_dir csyn_t31_parallel_staged_link_split_classify
```

## Results

- Host syntax: PASS
- Try26 syntax guard: PASS
- Try31 focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the testbench still expects them.
- Full CSIM: PASS (`csim_t31_full`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS on rerun (`t31s`, `Overall Result: PASS`). The first
  work directory `csim_t31_small` compiled but Vitis failed to launch
  `csim.exe` with `invalid argument`; this was treated as a tool launcher issue
  because the short-path rerun passed.
- CSYNTH: completed, kept scheduler `II=1`, but failed timing/resource
  acceptance.

## HLS Evidence

- Work directory: `csyn_t31_parallel_staged_link_split_classify`
- Report paths:
  - `csyn_t31_parallel_staged_link_split_classify/hls/syn/report/csynth.rpt`
  - `csyn_t31_parallel_staged_link_split_classify/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t31_parallel_staged_link_split_classify/hls/syn/report/csynth_design_size.rpt`
- LUT: `4602`
- FF: `2667`
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_861_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-4.35`; estimated Fmax
  `85.82 MHz`; estimated clock period `11.652 ns`
- Critical path evidence: the try30 selected-command classification path was
  improved, but the limiter moved back into the staged `LINK` write/result
  path. Vitis reports a path from `substitution_memory_0_load` at
  `src/reducer_core_parallel.cpp:1392` through
  `selected_command_produced_active_pairs.produced_pairs.left_port`,
  `local_status_code`, and `executed_steps` stores.
- Design-size evidence: `get_interaction_rule_kind` remains visible as a
  separate 41-instruction unrolled/classification contribution, and
  `execute_selected_non_link_active_pair_without_queue_commit` remains the
  selected non-`LINK` helper. The final HW-transform design size is `816`
  instructions.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try31 is a useful partial improvement, but not an acceptable FPGA endpoint.
Splitting selected active-pair capture from classification reduces the timing
failure compared with try30:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try30 staged `LINK` | 4770 | 2677 | 0 | 0 | 1 | `-7.29` | `68.57 MHz` |
| Try31 split classify | 4602 | 2667 | 0 | 0 | 1 | `-4.35` | `85.82 MHz` |

This supports the hypothesis that selected-command classification was part of
try30's timing problem. However, after that path is shortened, the critical
path returns to staged `LINK` result/writeback and completion/status muxing.
The architecture remains too timing-hostile for the target clock.

## Next Recommendation

Do not add more SELECT phases immediately. The next try should keep the useful
split-classify and staged-`LINK` boundaries, but split the final `LINK`
write/result tick:

- one service tick writes or clears `substitution_memory`;
- a later service tick appends the produced pair and updates
  `local_status_code`/`executed_steps`;
- commit service remains unchanged.

This directly targets the reported try31 path from substitution-memory load to
produced-pair/status/completion stores.
