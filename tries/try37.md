# Try 37: Compact Produced-Pair Buffer For Focused Parallel Scheduler

Current status: `failed_csynth_resources`

Branch: `try37`

Architecture: return to the try35 single-site refill base and reduce the
focused parallel scheduler's produced-pair buffer capacity when high-fanout
rules are disabled.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try36 completed synthesis but worsened timing/resources (`4091 LUT`, `2117 FF`, `II=1`, slack `-2.86`) and exposed `selected_command_produced_active_pairs.produced_pairs` plus status/service-phase routing as the new limiter. |
| 2026-05-26 | `implemented` | Added `ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER`, narrowed produced-pair capacity/counter only when the gate is enabled, and added a focused compact-produced synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full CSIM (`a37`), and FPGA-small CSIM (`b37`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `3998 LUT`, `1148 FF`, `0 DSP`, `0 BRAM`, and slack `-2.00`; this is the best focused parallel result so far but still misses timing. |

## Goal

Reduce selected-command produced-pair state pressure in the focused parallel
scheduler without changing the default full or FPGA-small semantic profiles.

## Hypothesis

The focused parallel synthesis profiles disable `COMMUTE`, `OPERATE`,
`SWITCH`, and `CALL`, leaving only small constant-size rules. In that subset,
`LINK` can emit at most one produced pair, while `ERASE` and `ANNIHILATE` emit
at most two. The generic `ProducedActivePairs` buffer still has capacity `17`
because the full reducer needs room for `CALL` book redexes. For the focused
parallel scheduler, synthesizing all 17 entries creates unnecessary mux and
register pressure. A compile-time focused capacity of `2` should reduce LUTs
and possibly timing pressure while preserving the measured rule subset.

## Difference From Prior Tries

Try36 added a generic completion service and made the service-phase/status mux
worse. Try37 does not keep that completion service. It returns to try35 and
targets the structure that try36 exposed in the critical path:

```text
selected_command_produced_active_pairs.produced_pairs
```

The change is not another scheduler phase split. It is a focused state-width
reduction for the rule subset used by the parallel synthesis probe.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER`
  - default `0`
- When the gate is enabled:
  - set `MAXIMUM_PRODUCED_ACTIVE_PAIRS_PER_INTERACTION` to `2`;
  - set `PRODUCED_PAIR_COUNTER_BIT_COUNT` to `2`.
- Use the gate only in a focused parallel synthesis config that also disables:
  - `COMMUTE`
  - `OPERATE`
  - `SWITCH`
  - `CALL`
- Do not enable `ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION`.
- Preserve default full-profile and FPGA-small behavior when the gate is off.
- Keep try35 gates enabled:
  - `ENABLE_PARALLEL_SERVICE_PHASE`
  - `ENABLE_REGISTER_LANE_QUEUE`
  - `ENABLE_PARALLEL_SELECTED_COMMAND`
  - `ENABLE_PARALLEL_STAGED_LINK`
  - `ENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY`
  - `ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK`
  - `ENABLE_PARALLEL_SPLIT_REFILL`
  - `ENABLE_PARALLEL_SINGLE_REFILL_POP`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try37 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a37
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b37
```

Focused compact-buffer CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_nostats.cfg --work_dir csyn_t37_parallel_compact_produced
```

## Results

- Host syntax: PASS
- Try37 focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the full testbench still expects them.
- Full CSIM: PASS (`a37`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`b37`, `Overall Result: PASS`)
- CSYNTH: completed, kept scheduler `II=1`, substantially reduced FFs, but
  still failed timing acceptance.

## HLS Evidence

- Work directory: `csyn_t37_parallel_compact_produced`
- Report paths:
  - `csyn_t37_parallel_compact_produced/hls/syn/report/csynth.rpt`
  - `csyn_t37_parallel_compact_produced/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t37_parallel_compact_produced/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_899_3_csynth.rpt`
  - `csyn_t37_parallel_compact_produced/hls/syn/report/csynth_design_size.rpt`
- LUT: `3998` top-level total, `3840` in the scheduler pipeline
- FF: `1148` top-level total, `1077` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_899_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-2.00`; estimated Fmax
  `107.48 MHz`; estimated clock period `9.304 ns`
- Critical path evidence: Vitis reported an unmet path to the
  `selected_command_lane_index` store with `8.95329 ns` combination delay. The
  critical path itself is through staged `LINK` substitution memory read,
  append-result handling, and `local_status_code` phi/store:
  `substitution_memory_0_load -> sparsemux -> append_produced_active_pair ->
  icmp_ln1620 -> local_status_code`.
- Design-size evidence: final HW-transform design size is `592` instructions,
  down from try35's `808` and try36's `864`. The global pop remains a single
  `active_pair_queue_pop_global` contribution at `452` instructions, preserving
  try35's single-pop property.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try37 is not timing-clean, so its current status is
`failed_csynth_resources`. However, it is the best focused parallel scheduler
result so far and validates the state-width hypothesis.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try35 single-site refill pop | 4003 | 2114 | 0 | 0 | 1 | `-2.05` | `107.01 MHz` |
| Try36 selected completion service | 4091 | 2117 | 0 | 0 | 1 | `-2.86` | `98.40 MHz` |
| Try37 compact produced buffer | 3998 | 1148 | 0 | 0 | 1 | `-2.00` | `107.48 MHz` |

The important architectural evidence is the FF reduction: narrowing the
produced-pair buffer from the full `CALL`-sized capacity of `17` to the focused
parallel rule-subset capacity of `2` nearly halves the scheduler FF estimate.
This confirms that full semantic buffer sizing was leaking into the focused
parallel hardware experiment.

## Next Recommendation

Keep try37 as the new focused parallel base. The next try should target the
remaining staged `LINK` append/status path:

- preserve `ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER`;
- preserve try35 single-site refill pop;
- do not re-enable try36 completion service;
- split staged `LINK` append-result status from substitution-memory read/write,
  or specialize the focused `LINK` append path so it avoids the generic
  `append_produced_active_pair` overflow/status mux where possible.
