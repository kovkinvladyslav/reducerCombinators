# Try 26: Single Selected-Lane Command Register Probe

Current status: `csynth_passed`

Branch: `try26`

Architecture: keep the try21 phased structural parallel scheduler and
register-backed lane queues, but replace the two-lane SELECT-to-EXECUTE state
with one selected-lane command register in a focused synthesis profile.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try25 showed that a `VOID`-only profile can reach parent scheduler `II=1`, but timing is unacceptable: `2730 LUT`, `1469 FF`, `0 DSP`, `0 BRAM`, slack `-9.15`, Fmax `60.78 MHz`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SELECTED_COMMAND`, implemented the gated single-command SELECT/EXECUTE/COMMIT path, added the focused synthesis config, and passed default plus focused syntax checks. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small CSIM both completed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | Focused selected-command synthesis completed after partitioning the selected produced-pair buffer: `3170 LUT`, `1503 FF`, `0 DSP`, `0 BRAM`, loop `II=2`, slack `+0.12`, Fmax `139.31 MHz`. |

## Goal

Test whether the try21/try24 `II=3` and try25 timing collapse are caused by
carrying too much two-lane selection and writeback state through one parent
pipeline.

## Hypothesis

The current phased scheduler stores candidate active pairs, rule kinds,
commit bits, produced-pair buffers, and produced-pair cursors for both lanes.
Even when only a tiny rule subset is enabled, HLS still sees wide service-state
muxing across SELECT, EXECUTE, and COMMIT.

If SELECT writes one bounded command register instead of two-lane arrays, then
EXECUTE and COMMIT should have less state to mux. The probe intentionally
reduces parallel issue width to one selected lane per command; that is
acceptable because this try measures control-state ownership, not final
parallel throughput.

## Difference From Prior Tries

Try22 factored service bodies into C++ helper functions, but HLS still scheduled
the same parent-loop state and produced a worse result.

Try24 specialized two-lane conflict checking but kept two-lane candidate and
writeback state, producing essentially the same result as try21.

Try25 removed producer rules, which improved II but destroyed timing. Try26
does not remove more rules. It changes the boundary between SELECT, EXECUTE,
and COMMIT by carrying one explicit selected-lane command.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SELECTED_COMMAND`
  - default `0`
- Add a small command state in `reducer_core_parallel.cpp` when the gate is
  enabled:
  - command valid bit
  - selected lane index
  - selected active pair
  - selected rule kind
  - selected produced-pair buffer
  - selected produced-pair cursor
- In the phased scheduler under the gate:
  - REFILL remains unchanged.
  - SELECT scans lane-local queues and stores only the first eligible command.
  - EXECUTE pops and executes only the selected lane.
  - COMMIT flushes produced pairs from the selected command only.
- Preserve default behavior when the gate is disabled.
- Add a focused config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_selected_command_nostats.cfg`
- Keep the try21 structural subset:
  - `VOID`
  - `ERASE`
  - `ANNIHILATE`
- Keep disabled:
  - `LINK`
  - `COMMUTE`
  - `OPERATE`
  - `SWITCH`
  - `CALL`
  - reducer and parallel statistics.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Focused selected-command syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t26_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t26_small
```

Focused selected-command CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_selected_command_nostats.cfg --work_dir csyn_t26_parallel_structural_phased_nolink_regqueue_selected_command
```

## Results

Fill this after implementation.

- Host syntax: PASS
- Focused syntax: PASS
- Host execution:
- Full CSIM: PASS (`csim_t26_full_partitioned`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`csim_t26_small_partitioned`, `Overall Result: PASS`)
- CSYNTH: PASS, diagnostic architecture only

## HLS Evidence

- Work directory: `csyn_t26_parallel_structural_phased_nolink_regqueue_selected_command_partitioned`
- Report paths:
  - `csyn_t26_parallel_structural_phased_nolink_regqueue_selected_command_partitioned/hls/syn/report/csynth.rpt`
  - `csyn_t26_parallel_structural_phased_nolink_regqueue_selected_command_partitioned/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t26_parallel_structural_phased_nolink_regqueue_selected_command_partitioned/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_734_3_csynth.rpt`
  - `csyn_t26_parallel_structural_phased_nolink_regqueue_selected_command_partitioned/hls/syn/report/csynth_design_size.rpt`
- LUT: `3170`
- FF: `1503`
- DSP: `0`
- BRAM: `0`
- II: main scheduler loop `II=2`, iteration latency `3`
- Timing/slack: top slack `+0.12`, estimated Fmax `139.31 MHz`, estimated clock `7.178 ns` for `10 ns` target with `2.70 ns` uncertainty
- Last Vitis log line if frozen: not applicable; synthesis completed

Additional evidence:

- The first selected-command synthesis without partitioning
  `selected_command_produced_active_pairs.produced_pairs` reached the same
  `II=2` but reported `Memory Dependency`, `3095 LUT`, `1388 FF`, and top slack
  `+0.12`.
- Partitioning that selected produced-pair buffer removed the local memory from
  the report and changed the remaining violation to `Resource Limitation`.

## Conclusion

This is a useful positive diagnostic, but not the final parallel runtime.

Compared with try21/try24, the selected-command probe improves the parent
scheduler from `II=3` to `II=2`, keeps `0 BRAM`, and gives positive timing
slack. It also reduces resources compared with try21/24:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try21 register local queues | 3914 | 2071 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try24 pairwise select | 3913 | 2070 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try26 single selected command | 3170 | 1503 | 0 | 0 | 2 | `+0.12` | `139.31 MHz` |

The tradeoff is important: this focused path gives up dual-lane issue and uses
one selected command. Therefore it proves that registered command/writeback
ownership is the right direction, but it is not the final architecture for
parallel throughput.

## Next Recommendation

The next try should extend the selected-command idea to a two-entry command
buffer:

- SELECT fills up to two registered commands after conflict checking.
- EXECUTE consumes one command per service tick, or one per lane only if HLS can
  keep the command state independent.
- COMMIT owns produced-pair flushing and all local/global queue counter writes.

The goal is to recover measurable dual-lane execution while preserving the
registered ownership that improved try26 timing and II.
