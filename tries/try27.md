# Try 27: Two-Entry Selected Command Buffer Probe

Current status: `failed_csynth_resources`

Branch: `try27`

Architecture: keep the try26 registered selected-command boundary, but allow
SELECT to capture up to two independent lane commands before EXECUTE/COMMIT.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try26 improved the focused parallel scheduler to `II=2`, `3170 LUT`, `1503 FF`, `0 DSP`, `0 BRAM`, and slack `+0.12`, but it selected only one lane command and therefore could not measure dual-lane issue. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_TWO_COMMAND_BUFFER`, implemented a gated two-command SELECT/EXECUTE/COMMIT path, added the focused synthesis config, and passed default plus focused syntax checks. |
| 2026-05-26 | `failed_csynth_resources` | Focused synthesis completed, but the two-command path grew to `5700 LUT`, `1924 FF`, `0 DSP`, `0 BRAM`, loop `II=2`, slack `-2.76`, and Fmax `99.36 MHz`; CSIM execution was blocked by Windows Application Control after build. |

## Goal

Recover measurable two-lane scheduler behavior without returning to the wide
two-lane candidate/writeback state that kept try21 and try24 at `II=3`.

## Hypothesis

Try26 showed that a registered command boundary is useful: SELECT stores a small
command, EXECUTE consumes it, and COMMIT owns produced-pair flushing. The cost
is that only one lane can issue.

If SELECT stores two fixed command entries, then independent non-conflicting
lane work can be carried across the scheduler boundary while preserving the
ownership split that helped try26. This may increase area and may return to
`II=3`, but it directly tests whether dual command state can synthesize better
than the older generic phased arrays.

## Difference From Prior Tries

Try24 specialized the conflict check but still used the generic per-lane
candidate and produced-pair arrays.

Try26 replaced that state with one selected command and improved II/timing, but
intentionally sacrificed dual issue.

Try27 is different because it keeps the selected-command architecture while
adding a second explicit command entry. It is not another conflict-only change
and not another rule-deletion experiment.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_TWO_COMMAND_BUFFER`
  - default `0`
- Keep all default builds unchanged when the gate is disabled.
- In the focused phased scheduler:
  - REFILL remains unchanged.
  - SELECT initializes two command slots.
  - Lane 0 may fill command 0.
  - Lane 1 may fill command 1 only if it fits the step budget and does not
    conflict with command 0.
  - If lane 0 is empty and lane 1 has work, lane 1 may fill command 0.
  - EXECUTE executes valid command slots.
  - COMMIT flushes produced pairs from command 0 first, then command 1, so queue
    counter mutation still has one owner.
- Use two fixed command variables rather than dynamic arrays:
  - command valid bit
  - lane index
  - active pair
  - rule kind
  - produced-pair buffer
  - produced-pair cursor
- Partition each command produced-pair buffer so HLS does not infer a local RAM
  like the first try26 synthesis did.
- Add a focused config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_two_command_nostats.cfg`
- Keep the try26 structural subset:
  - enabled: `VOID`, `ERASE`, `ANNIHILATE`
  - disabled: `LINK`, `COMMUTE`, `OPERATE`, `SWITCH`, `CALL`
  - disabled statistics for the synthesis measurement.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Focused two-command syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_TWO_COMMAND_BUFFER=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t27_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t27_small
```

Focused two-command CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_two_command_nostats.cfg --work_dir csyn_t27_parallel_structural_phased_nolink_regqueue_two_command
```

## Results

Fill this after implementation.

- Host syntax: PASS
- Focused syntax: PASS
- Host execution:
- Full CSIM: BLOCKED by Windows Application Control after successful build; generated `csim.exe` could not be executed.
- FPGA-small CSIM: BLOCKED by Windows Application Control after successful build; generated `csim.exe` could not be executed.
- CSYNTH: completed, but failed as an optimization because timing/resource cost is too high versus try26.

## HLS Evidence

- Work directory: `csyn_t27_parallel_structural_phased_nolink_regqueue_two_command`
- Report paths:
  - `csyn_t27_parallel_structural_phased_nolink_regqueue_two_command/hls/syn/report/csynth.rpt`
  - `csyn_t27_parallel_structural_phased_nolink_regqueue_two_command/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t27_parallel_structural_phased_nolink_regqueue_two_command/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_755_3_csynth.rpt`
  - `csyn_t27_parallel_structural_phased_nolink_regqueue_two_command/hls/syn/report/csynth_design_size.rpt`
- LUT: `5700`
- FF: `1924`
- DSP: `0`
- BRAM: `0`
- II: main scheduler loop `II=2`, iteration latency `3`
- Timing/slack: top slack `-2.76`, estimated Fmax `99.36 MHz`, estimated clock `10.065 ns` for `10 ns` target with `2.70 ns` uncertainty
- Last Vitis log line if frozen: not applicable; synthesis completed.

Additional evidence:

- The loop violation remained `Resource Limitation`; no local produced-pair RAM
  appeared after partitioning both command produced-pair buffers.
- The design-size report shows duplicated conflict, execute, and commit logic:
  `get_interaction_rule_kind`, `execute_lane_active_pair_without_queue_commit`,
  and `commit_one_phased_produced_pair` each appear as two calls.
- Compared with try26, the two-command path adds `2530 LUT` and `421 FF` while
  keeping the same `II=2` and changing slack from `+0.12` to `-2.76`.

## Conclusion

Try27 is useful negative evidence, not a viable replacement for try26.

The result proves that a two-entry command buffer is synthesizable and keeps
the selected-command ownership model: SELECT stores commands, EXECUTE consumes
commands, and COMMIT owns produced-pair flushing. However, duplicating the
command execution and commit datapaths in the same service phase costs too much
area and timing on the Artix-7 target.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try21 register local queues | 3914 | 2071 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try26 single selected command | 3170 | 1503 | 0 | 0 | 2 | `+0.12` | `139.31 MHz` |
| Try27 two-command buffer | 5700 | 1924 | 0 | 0 | 2 | `-2.76` | `99.36 MHz` |

For diploma wording, this is evidence that registered command ownership is
good, but duplicating the structural rule datapath for two same-phase commands
is not the right scaling step. Parallel issue needs a narrower command service,
not duplicated execute/commit bodies in one parent scheduler loop.

## Next Recommendation

The next try should preserve try26's single-command timing boundary but separate
dual selection from execution:

- SELECT may still identify up to two independent commands.
- Store only one command into the active execute register per service tick.
- Keep a second waiting command register or a tiny issue FIFO.
- EXECUTE and COMMIT should consume one command at a time so HLS does not
  duplicate the rule and commit datapaths.

This would test whether dual-lane selection can improve queue utilization
without paying try27's two-datapath timing cost.
