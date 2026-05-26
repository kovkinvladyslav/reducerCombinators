# Try 42: Direct Non-Link Produced Outputs

Current status: `failed_csynth_resources`

Branch: `try42`

Architecture: preserve the try41 staged `LINK` decide/write split and add a
focused direct produced-pair construction path for selected-command `ERASE` and
`ANNIHILATE`.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try41 completed focused CSYNTH at `3707 LUT`, `1150 FF`, `II=1`, and `-1.16 ns` slack; the reported critical path moved from staged `LINK` substitution memory into selected-command non-link produced-pair and node-check logic. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_DIRECT_NON_LINK_OUTPUTS`, direct selected-command `ERASE`/`ANNIHILATE` produced-pair helpers, and a focused synthesis config extending try41. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a42`), and FPGA-small CSIM (`b42`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with the same top-level hardware point as try41: `3707 LUT`, `1150 FF`, `0 DSP`, `0 BRAM`, scheduler `II=1`, estimated Fmax `118.28 MHz`, and `-1.16 ns` slack. |

## Goal

Shorten the new try41 critical path in selected-command non-link execution while
preserving the staged `LINK` improvement that removed the substitution-memory
read/compare/write chain from the top timing path.

## Hypothesis

In the focused FPGA-small parallel profile, `COMMUTE`, `OPERATE`, `SWITCH`, and
`CALL` are disabled, and the compact produced buffer has exactly two slots.
For that measured subset, valid `ERASE` and `ANNIHILATE` interactions always
produce exactly two active pairs. If selected-command non-link execution writes
those two produced pairs directly instead of calling the generic
`append_produced_active_pair` helper twice, HLS may remove the produced-count
capacity checks and overflow muxing from the critical path.

The expected hardware tradeoff is deliberately narrow: the direct path is only
enabled under an explicit profile gate, and it does not change full-profile
semantics.

## Difference From Prior Tries

Try38 specialized staged `LINK` produced-pair append. Try39 and try40 targeted
staged `LINK` read/write/control variants. Try41 split staged `LINK`
decision from substitution writeback and moved the top critical path away from
`LINK`.

Try42 does not alter staged `LINK`. It targets the new bottleneck reported by
try41: selected-command `ERASE`/`ANNIHILATE` output construction and node checks
feeding `local_status_code`.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_DIRECT_NON_LINK_OUTPUTS`
  - default `0`
- Add focused direct helper functions in `reducer_core_parallel.cpp`:
  - direct selected-command `ERASE`
  - direct selected-command `ANNIHILATE`
- Enable those helpers only when:
  - `ENABLE_PARALLEL_DIRECT_NON_LINK_OUTPUTS == 1`
  - `ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER == 1`
- Preserve generic `apply_lane_erase_rule` and `apply_lane_annihilate_rule`
  when the gate is off.
- Keep validation conservative:
  - full-profile CSIM must still pass;
  - FPGA-small CSIM must still pass;
  - focused FPGA-small CSYNTH measures whether this direct non-link path helps.
- Add a focused synthesis config that extends try41 with
  `ENABLE_PARALLEL_DIRECT_NON_LINK_OUTPUTS=1`.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try42 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_DIRECT_NON_LINK_OUTPUTS=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a42
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b42
```

Focused direct-non-link CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_direct_link_decide_write_split_direct_nonlink_nostats.cfg --work_dir csyn_t42_parallel_direct_nonlink
```

## Results

- Host syntax: PASS
- Try42 focused syntax: PASS for both `src/reducer_core_parallel.cpp` and
  `test/reducer_test.cpp`.
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the full testbench still expects them.
- Full CSIM: PASS (`a42`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`b42`, `Overall Result: PASS`)
- CSYNTH: completed, but did not improve resources or timing versus try41.

## HLS Evidence

- Work directory: `csyn_t42_parallel_direct_nonlink`
- Report paths:
  - `csyn_t42_parallel_direct_nonlink/hls/syn/report/csynth.rpt`
  - `csyn_t42_parallel_direct_nonlink/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t42_parallel_direct_nonlink/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_977_3_csynth.rpt`
  - `csyn_t42_parallel_direct_nonlink/hls/syn/report/csynth_design_size.rpt`
- LUT: `3707` top-level total, `3549` in the scheduler pipeline
- FF: `1150` top-level total, `1079` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_977_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`.
- Timing/slack: estimated clock period `8.455 ns`, estimated Fmax
  `118.28 MHz`. With the configured `2.70 ns` uncertainty, this remains
  `-1.16 ns` against the effective `7.30 ns` budget.
- Critical path evidence: Vitis still reports a path from
  `selected_command_produced_active_pairs.produced_pairs.left_port` through
  erasure port/tag selection, node address selection, node address range check,
  and the `local_status_code` phi/store.
- Design-size evidence: final HW-transform design size is `560` instructions,
  matching try41.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try42 is a negative result. The direct non-link output helpers are correct under
CSIM, but they synthesize to the same focused hardware point as try41. This
suggests Vitis had already removed most of the generic two-append overhead in
the compact produced-buffer profile. The real remaining path is not the
produced-pair counter overflow logic; it is the coupling between:

```text
selected produced-pair/tag state
ERASE node-port selection
node address range check
local_status_code selection/store
```

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try42 direct non-link outputs | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |

For the diploma, this is still useful: it shows that after buffer compaction,
rule-specific append specialization has diminishing returns for
`ERASE`/`ANNIHILATE`. The scheduler bottleneck is now status/control staging
around node-port validity, not produced-pair slot writes.

## Next Recommendation

Keep try41 as the preferred focused base. The next try should avoid another
direct append/write specialization and instead target the status/control path
reported by Vitis. A meaningfully different approach would stage selected
non-link node validation before produced-pair construction:

```text
CLASSIFY selected non-link
VALIDATE node/tag/address and read node data
EMIT produced pairs or error status
```

That would add a service tick for `ERASE`/`ANNIHILATE`, but it directly attacks
the reported `node_address -> local_status_code` critical path.
