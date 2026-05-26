# Try 28: Waiting Command Slot Probe

Current status: `failed_csynth_resources`

Branch: `try28`

Architecture: preserve try26's single active execute/commit command, but add a
second waiting command slot captured by SELECT and promoted only after the
active command is fully committed.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try27 proved that two same-phase command datapaths are too expensive: `5700 LUT`, `1924 FF`, `0 DSP`, `0 BRAM`, `II=2`, slack `-2.76`, Fmax `99.36 MHz`. Try26 remains the timing/resource baseline at `3170 LUT`, `1503 FF`, `II=2`, slack `+0.12`, but it cannot carry a second independent lane command. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_WAITING_COMMAND`, implemented one active command plus one waiting metadata slot, added the focused synthesis config, and passed default plus focused syntax checks. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small CSIM both completed with `Overall Result: PASS`. |
| 2026-05-26 | `failed_csynth_resources` | Focused synthesis completed, but timing is worse than try26 and try27: `3790 LUT`, `1787 FF`, `0 DSP`, `0 BRAM`, loop `II=2`, slack `-5.73`, Fmax `76.74 MHz`. |

## Goal

Test whether the parallel scheduler can retain a second independent selected
lane without duplicating the rule execution and produced-pair commit datapaths.

## Hypothesis

Try26 showed that one registered command is a good timing boundary. Try27 showed
that duplicating the active command datapath is too expensive. Therefore the
next useful architecture is a tiny command issue buffer:

- one active command is executed and committed;
- one waiting command stores a second independent lane;
- COMMIT promotes the waiting command after the active command has no pending
  produced pairs.

If this works, it should keep area and timing closer to try26 than try27 while
allowing SELECT to observe two lanes.

## Difference From Prior Tries

Try26 has one command and discards/stalls other lane work.

Try27 has two active command slots and executes both in the same EXECUTE phase,
which duplicates execution and commit logic.

Try28 is different because it stores two commands but keeps only one active
execute/commit datapath. It trades same-cycle dual execution for a small
registered issue queue.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_WAITING_COMMAND`
  - default `0`
- Reuse the try26 active command fields:
  - valid bit
  - lane index
  - active pair
  - rule kind
  - produced-pair buffer
  - produced-pair cursor
- Add a waiting command slot:
  - valid bit
  - lane index
  - active pair
  - rule kind
- SELECT behavior:
  - if an active or waiting command already exists, skip new selection and move
    to EXECUTE or COMMIT as appropriate;
  - otherwise capture lane 0 as the active command when available;
  - capture lane 1 as the waiting command only if it is independent of the
    active command and fits the step budget;
  - if lane 0 is empty, lane 1 may become the active command.
- EXECUTE behavior:
  - execute only the active command;
  - never execute the waiting command in the same phase.
- COMMIT behavior:
  - flush produced pairs only for the active command;
  - when active commit completes, promote waiting to active and return to
    EXECUTE;
  - otherwise return to REFILL.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_waiting_command_nostats.cfg`
- Keep the try26/try27 structural subset:
  - enabled: `VOID`, `ERASE`, `ANNIHILATE`
  - disabled: `LINK`, `COMMUTE`, `OPERATE`, `SWITCH`, `CALL`
  - disabled statistics for synthesis.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Focused waiting-command syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_WAITING_COMMAND=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t28_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t28_small
```

Focused waiting-command CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_waiting_command_nostats.cfg --work_dir csyn_t28_parallel_structural_phased_nolink_regqueue_waiting_command
```

## Results

Fill this after implementation.

- Host syntax: PASS
- Focused syntax: PASS
- Host execution:
- Full CSIM: PASS (`csim_t28_full`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`csim_t28_small`, `Overall Result: PASS`)
- CSYNTH: completed, but failed as an optimization because timing is much worse than try26.

## HLS Evidence

- Work directory: `csyn_t28_parallel_structural_phased_nolink_regqueue_waiting_command`
- Report paths:
  - `csyn_t28_parallel_structural_phased_nolink_regqueue_waiting_command/hls/syn/report/csynth.rpt`
  - `csyn_t28_parallel_structural_phased_nolink_regqueue_waiting_command/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t28_parallel_structural_phased_nolink_regqueue_waiting_command/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_773_3_csynth.rpt`
  - `csyn_t28_parallel_structural_phased_nolink_regqueue_waiting_command/hls/syn/report/csynth_design_size.rpt`
- LUT: `3790`
- FF: `1787`
- DSP: `0`
- BRAM: `0`
- II: main scheduler loop `II=2`, iteration latency `3`
- Timing/slack: top slack `-5.73`, estimated Fmax `76.74 MHz`, estimated clock `13.032 ns` for `10 ns` target with `2.70 ns` uncertainty
- Last Vitis log line if frozen: not applicable; synthesis completed.

Additional evidence:

- Compared with try27, this removes duplicated execute and commit calls:
  `execute_lane_active_pair_without_queue_commit` appears once instead of twice,
  and `commit_one_phased_produced_pair` appears once instead of twice.
- Area improves substantially over try27 (`3790 LUT` vs `5700 LUT`) and is
  close to try21/24, but timing becomes worse than both try26 and try27.
- The main loop still reports `II=2` with `Resource Limitation`, so the waiting
  slot does not improve issue interval.

## Conclusion

Try28 is useful negative evidence.

The waiting slot solves the specific duplicated-datapath problem seen in try27:
the design-size report shows only one execute path and one commit path. However,
the additional control state for active/waiting validity, promotion, execution
tracking, and phase selection creates a worse timing path. The scheduler remains
at `II=2` and timing falls to `76.74 MHz`.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try26 single selected command | 3170 | 1503 | 0 | 0 | 2 | `+0.12` | `139.31 MHz` |
| Try27 two-command buffer | 5700 | 1924 | 0 | 0 | 2 | `-2.76` | `99.36 MHz` |
| Try28 waiting command slot | 3790 | 1787 | 0 | 0 | 2 | `-5.73` | `76.74 MHz` |

For diploma wording, try28 separates datapath duplication from command-control
state. Try27 showed that duplicating the datapath is expensive; try28 shows that
naively adding a waiting slot can also hurt timing even when the datapath is not
duplicated.

## Next Recommendation

The next try should return to the try26 selected-command baseline and avoid
carrying second-command state through the same parent pipeline.

A better direction is to improve try26 incrementally rather than recover dual
issue immediately:

- reduce selected-command SELECT logic and conflict helpers;
- make command status/promotion a separate unpipelined or non-hot service;
- or reintroduce one disabled semantic feature, such as `LINK`, through a
  staged substitution service while keeping the try26 selected-command boundary.
