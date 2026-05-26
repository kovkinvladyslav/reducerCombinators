# Try 41: Staged LINK Decide-Write Split

Current status: `failed_csynth_resources`

Branch: `try41`

Architecture: return to the try38 focused parallel base and split selected
staged `LINK` writeback into a separate decision tick and substitution-write
tick.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try40 matched try38 at `3749 LUT`, `1148 FF`, `II=1`, slack `-1.97`; Vitis still reported `substitution_memory_0_load -> selected_command_link_previous_port -> unbound-marker comparison -> local_status_code phi/store`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT`, a `DECIDE_RESULT` staged `LINK` phase, widened the gated staged-link phase register, and added a focused synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a41`), and FPGA-small CSIM (`b41`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `3707 LUT`, `1150 FF`, `0 DSP`, `0 BRAM`, estimated Fmax `118.28 MHz`, and slack `-1.16`; this is the best timing so far but still misses the effective timing budget. |

## Goal

Break the remaining selected-command staged `LINK` read/compare/writeback
control path by making the unbound-marker decision a service tick that does not
also write substitution memory or complete the interaction.

## Hypothesis

Try39 showed that computing the unbound predicate during the read tick and
consuming it in the same loop-carried region was not enough. Try40 showed the
completion guard was not the limiter. The remaining candidate is the combined
writeback service tick:

```text
load previous substitution
compare against unbound marker
write substitution memory
select service/status/control
```

If staged `LINK` uses a separate decision stage after the read stage, HLS may
schedule the previous-port comparison into a narrower state that only sets
`selected_command_link_emit_pair`. The following write stage then only mutates
substitution memory based on a one-bit state. This increases staged `LINK`
latency by one service tick, but may reduce the loop critical path.

## Difference From Prior Tries

Try39 cached the unbound predicate in the read stage, which kept predicate
production and consumption in the reported path. Try41 does not compute the
predicate during read. It adds a new explicit staged `LINK` state between read
and write:

```text
READ_PREVIOUS -> DECIDE_RESULT -> WRITE_RESULT -> APPEND_RESULT
```

Try40 changed only completion bookkeeping and synthesized back to try38. Try41
changes the actual staged `LINK` writeback schedule.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT`
  - default `0`
- Add a new staged `LINK` phase constant:
  - `PARALLEL_LINK_STAGE_DECIDE_RESULT`
- Widen `selected_command_link_stage` only when this gate is enabled.
- In the read stage:
  - load and store `selected_command_link_previous_port`;
  - transition to `DECIDE_RESULT` when the gate is enabled;
  - otherwise preserve the existing transition to `WRITE_RESULT`.
- In `DECIDE_RESULT`:
  - compare `selected_command_link_previous_port` against the unbound marker;
  - set `selected_command_link_emit_pair`;
  - transition to `WRITE_RESULT`;
  - do not write substitution memory.
- In `WRITE_RESULT` under the gate:
  - write either `selected_command_link_other_port` or the unbound marker based
    only on `selected_command_link_emit_pair`;
  - transition to `APPEND_RESULT`.
- Preserve existing behavior when the gate is off.
- Do not enable try39 read-decode or try40 implicit-success in this profile.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try41 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a41
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b41
```

Focused decide-write-split CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_direct_link_decide_write_split_nostats.cfg --work_dir csyn_t41_parallel_link_decide_write_split
```

## Results

- Host syntax: PASS
- Try41 focused syntax: PASS for both `src/reducer_core_parallel.cpp` and
  `test/reducer_test.cpp`.
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the full testbench still expects them.
- Full CSIM: PASS (`a41`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`b41`, `Overall Result: PASS`)
- CSYNTH: completed, improved timing and LUTs versus try38/try40, but still
  failed timing acceptance.

## HLS Evidence

- Work directory: `csyn_t41_parallel_link_decide_write_split`
- Report paths:
  - `csyn_t41_parallel_link_decide_write_split/hls/syn/report/csynth.rpt`
  - `csyn_t41_parallel_link_decide_write_split/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t41_parallel_link_decide_write_split/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_906_3_csynth.rpt`
  - `csyn_t41_parallel_link_decide_write_split/hls/syn/report/csynth_design_size.rpt`
- LUT: `3707` top-level total, `3549` in the scheduler pipeline
- FF: `1150` top-level total, `1079` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_906_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: estimated clock period `8.455 ns`, estimated Fmax
  `118.28 MHz`. With the configured `2.70 ns` uncertainty, this remains
  `-1.16 ns` against the effective `7.30 ns` budget.
- Critical path evidence: the prior substitution-memory staged `LINK` path is
  gone from the top critical path. Vitis now reports a path through
  `selected_command_produced_active_pairs.produced_pairs.left_port`, erasure
  port/node checks in `apply_lane_erase_rule`, and `local_status_code`
  phi/store.
- Design-size evidence: final HW-transform design size is `560` instructions,
  down from try38/try40 `574` and try39 `576`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try41 is not timing-clean yet, but it is a meaningful positive architectural
result. Splitting staged `LINK` decision from substitution writeback removes
the `substitution_memory_0_load -> previous_port -> unbound-marker comparison`
chain from the reported top critical path and improves both LUTs and Fmax.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try38 direct staged `LINK` append | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |
| Try40 direct staged `LINK` implicit success | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |

The tradeoff is one extra service tick for variable `LINK`. That is acceptable
for this phase because the experiment prioritizes synthesizable schedule shape
over raw per-rule latency. The improvement is strong evidence for the diploma:
the HLS bottleneck was not the final completion guard or a cached predicate, but
the combined read/compare/write service boundary.

## Next Recommendation

Keep try41 as the new focused parallel base. The next try should target the new
critical path in non-link selected-command execution:

- preserve the staged `LINK` decide/write split;
- focus on `ERASE`/`ANNIHILATE` produced-pair generation or node-port checks;
- consider a compact/direct non-link produced-pair path for the focused rule
  subset, similar in spirit to try38's direct `LINK` append;
- avoid further staged `LINK` tweaks unless a future report points back there.
