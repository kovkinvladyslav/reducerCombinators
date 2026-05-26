# Try 32: Split Staged LINK Writeback

Current status: `failed_csynth_resources`

Branch: `try32`

Architecture: preserve try31 selected-command capture/classify split and staged
`LINK`, but split the final staged `LINK` result tick into substitution
writeback and produced-pair/completion ticks.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try31 improved timing versus try30 while keeping `II=1`, but still failed at `4602 LUT`, `2667 FF`, slack `-4.35`; Vitis reported the critical path from `substitution_memory_0_load` through produced-pair/status/completion stores. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK`, `PARALLEL_LINK_STAGE_APPEND_RESULT`, selected-command `LINK` emit-pair state, and a focused split-writeback synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, try32 focused syntax, full CSIM, and FPGA-small CSIM passed. The first FPGA-small work directory `t32s` compiled but Vitis failed to launch `csim.exe`; fresh short work directory `x32` passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `4594 LUT`, `2669 FF`, `0 DSP`, `0 BRAM`, but still failed timing with slack `-3.19` and estimated Fmax `95.28 MHz`. |

## Goal

Reduce the final staged-`LINK` write/result critical path without changing the
selected-command ownership model or interaction-combinator semantics.

## Hypothesis

Try31 still combines substitution-memory update, optional produced-pair append,
status selection, and completion accounting in one execute tick. Splitting that
tick should shorten the critical path after the substitution-memory read while
preserving the `II=1` service-loop structure.

## Difference From Prior Tries

Try30 staged `LINK` into begin/read/write-result. Try31 split active-pair
capture from rule classification. Try32 keeps both boundaries and changes only
the final staged-`LINK` result path:

```text
read previous substitution
write or clear substitution binding
append produced pair and update completion/status
```

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK`
  - default `0`
- Add a staged `LINK` service constant:
  - `PARALLEL_LINK_STAGE_APPEND_RESULT`
- Add one selected-command state bit:
  - `selected_command_link_emit_pair`
- In staged `LINK` writeback:
  - write the substitution binding or clear it;
  - record whether a produced pair must be emitted;
  - move to `PARALLEL_LINK_STAGE_APPEND_RESULT`.
- In append/result:
  - append the produced pair only when needed;
  - update status and `executed_steps`;
  - move to COMMIT.
- Preserve existing staged-`LINK` behavior when the gate is disabled.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_nostats.cfg`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try32 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t32_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir t32s
```

Focused split-writeback CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_nostats.cfg --work_dir csyn_t32_parallel_staged_link_split_writeback
```

## Results

- Host syntax: PASS
- Try32 focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the testbench still expects them.
- Full CSIM: PASS (`csim_t32_full`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS on rerun (`x32`, `Overall Result: PASS`). The first
  short work directory `t32s` compiled but Vitis failed to launch `csim.exe`
  with `invalid argument`; this matches the try31 launcher issue and was not a
  reducer mismatch.
- CSYNTH: completed, kept scheduler `II=1`, but failed timing/resource
  acceptance.

## HLS Evidence

- Work directory: `csyn_t32_parallel_staged_link_split_writeback`
- Report paths:
  - `csyn_t32_parallel_staged_link_split_writeback/hls/syn/report/csynth.rpt`
  - `csyn_t32_parallel_staged_link_split_writeback/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t32_parallel_staged_link_split_writeback/hls/syn/report/csynth_design_size.rpt`
- LUT: `4594`
- FF: `2669`
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_864_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-3.19`; estimated Fmax
  `95.28 MHz`; estimated clock period `10.495 ns`
- Critical path evidence: the try31 staged-`LINK` produced-pair/writeback path
  is no longer the reported limiter. Vitis now reports a broader control/status
  path from `global_queue_count_value` through redex-queue empty/refill
  selection and into the shared `local_status_code` store, with an incoming
  status value from the staged `LINK` unsupported-variable path at
  `src/reducer_core_parallel.cpp:1382`.
- Design-size evidence: final HW-transform design size is `823` instructions,
  only slightly above try31's `816`. `execute_selected_non_link_active_pair...`
  and `get_interaction_rule_kind` remain present, while the extra stage adds
  control state rather than a duplicated execute datapath.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try32 is another useful partial improvement, but it is still not an accepted
FPGA endpoint at the requested 10 ns clock with 2.70 ns uncertainty.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try30 staged `LINK` | 4770 | 2677 | 0 | 0 | 1 | `-7.29` | `68.57 MHz` |
| Try31 split classify | 4602 | 2667 | 0 | 0 | 1 | `-4.35` | `85.82 MHz` |
| Try32 split `LINK` writeback | 4594 | 2669 | 0 | 0 | 1 | `-3.19` | `95.28 MHz` |

The result validates the hardware hypothesis: moving substitution writeback
away from produced-pair/status/completion accounting improves timing again and
keeps `II=1`. The critical path has now moved out of the local staged-`LINK`
writeback body and into shared scheduler status/control selection, especially
the single `local_status_code` phi/store fed by multiple phases.

## Next Recommendation

The next try should avoid adding more rule stages and instead reduce shared
status mux pressure. A focused option is to defer error/status reporting through
a tiny pending-status register:

- each service phase writes `pending_status_code` and `pending_status_valid`;
- one narrow status service commits the pending status into `local_status_code`;
- success cases avoid feeding `local_status_code` from every service branch;
- keep try31/try32 boundaries enabled.

This should directly test whether the remaining timing failure is the shared
status phi/store rather than a specific interaction rule datapath.
