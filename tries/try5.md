# Try 5: Deferred Completion Accounting

Current status: `csynth_passed`

Branch: `try5`

Architecture: split interaction completion accounting from rule execution in the produced-buffer reducer path

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try4 kept `II=3` and `-8.89` slack; HLS critical path still connects staged `CALL` setup/capacity logic to the `executed_steps` store. |
| 2026-05-26 | `implemented` | Added `ENABLE_DEFERRED_COMPLETION_ACCOUNTING`, pending-completion state, FPGA-small CSIM/synthesis configs, and removed an HLS-hostile numeric stats pointer comparison. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small deferred-completion CSIM passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | FPGA-small deferred-completion synthesis completed with final loop `II=1` and all loop constraints satisfied. |

## Goal

Keep try3's single-owner queue commit service, but break the long timing path from rule execution and staged `CALL` setup into `executed_steps` and rule-statistics accounting.

## Hypothesis

Try4 proved that early branch exits inside one pipelined loop are not enough. The HLS critical path still includes staged `CALL` header/capacity arithmetic, `interaction_completed` muxing, and the `executed_steps` store.

If completed interactions are recorded through a one-entry pending-completion state and the reducer increments `executed_steps` in a separate service tick, HLS should no longer need to route every rule's completion logic directly into the step counter in the same cycle. This may reduce timing pressure even if it adds one control tick per completed interaction.

## Difference From Prior Tries

Try1 staged `CALL`. Try2 buffered only some produced pairs. Try3 made queue writeback single-owner. Try4 rearranged branch order but still wrote completion state from the same broad execute region.

Try5 is meaningfully different because it changes the architectural timing boundary: rule execution and completion accounting become two separate reducer services under a compile-time gate. It intentionally trades one extra service tick for a shorter hot control path.

## Implementation Plan

1. Add a compile-time gate, `ENABLE_DEFERRED_COMPLETION_ACCOUNTING`, defaulting to disabled.
2. Enable it only for the try5 FPGA-small produced-buffer synthesis config.
3. Add a one-entry pending completion state:
   - active flag,
   - completed `InteractionRuleKind`.
4. In the produced-buffer reducer path:
   - when a flushed buffer completes, set pending completion and continue;
   - when staged `CALL` completes without pending produced pairs, set pending completion and continue;
   - when normal rule execution completes with no pending produced pairs, set pending completion and continue;
   - at the start of the loop, if pending completion is active, update rule statistics and increment `executed_steps`.
5. Preserve failure/status behavior as immediate, because errors must still stop the reducer with the same status code.
6. Preserve full-profile default behavior unless the gate is enabled.
7. Validate with full CSIM, FPGA-small produced-buffer CSIM, and FPGA-small deferred-completion CSYNTH.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try5
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_deferred_completion.cfg --work_dir interactReduce_csim_small_deferred_completion_try5
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_deferred_completion.cfg --work_dir interactReduce_csynth_small_deferred_completion_try5
```

## Results

- Host syntax: passed for default full profile and for `-DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1`.
- Host execution: not used; Vitis CSIM is the authoritative runtime check on this Windows setup.
- Full CSIM: passed in `interactReduce_csim_full_try5_after_numeric_fix` with `Overall Result: PASS`.
- FPGA-small CSIM: passed in `interactReduce_csim_small_deferred_completion_try5_after_numeric_fix` with `Overall Result: PASS`.
- CSYNTH: completed in `interactReduce_csynth_small_deferred_completion_try5_after_numeric_fix`.
- Initial CSYNTH attempt: failed pre-synthesis on an existing pointer comparison in `apply_numeric_operation`; fixed by replacing the runtime pointer test with `#if ENABLE_REDUCER_STATISTICS`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_deferred_completion_try5_after_numeric_fix`
- Report paths:
  - `interactReduce_csynth_small_deferred_completion_try5_after_numeric_fix/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_deferred_completion_try5_after_numeric_fix/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_deferred_completion_try5_after_numeric_fix/hls/syn/report/csynth_design_size.rpt`
- LUT: `10591` (`50%` of `xc7a35t-cpg236-1`)
- FF: `4425` (`10%`)
- DSP: `2` (`2%`)
- BRAM: `0`
- II: target `1`, final `1`
- Timing/slack: estimated clock `8.510 ns` for a `10 ns` target with `2.70 ns` uncertainty; summary slack `-1.21`
- Loop status: all loop constraints satisfied; estimated Fmax `117.51 MHz`
- Design size: `58823` compile/link instructions, reduced to `2148` after HW transforms
- Last Vitis log line if frozen: not applicable; synthesis completed.

Comparison:

```text
try3 FPGA-small single-owner commit:
LUT 8323, FF 3277, DSP 2, II 3, slack -8.89

try4 FPGA-small phase-specialized control:
LUT 8306, FF 3250, DSP 2, II 3, slack -8.89

try5 FPGA-small deferred completion:
LUT 10591, FF 4425, DSP 2, II 1, slack -1.21
```

Important interpretation:

The loop `VITIS_LOOP_129_1` reached `II=1`, but rule helpers became separate RTL modules with multi-cycle latencies, for example `begin_staged_call_buffered` up to 3 cycles, `apply_operate_rule_buffered` up to 5 cycles, and `step_staged_call_buffered` up to 2 cycles. Try5 improves the scheduler-visible loop II and timing by making the phase boundary real, at the cost of more LUT/FF and extra service cycles.

## Conclusion

Try5 is a successful architecture experiment. Deferring completion accounting changed the HLS structure enough to satisfy loop constraints and reach final loop `II=1`. This confirms the try4 diagnosis: the previous limiter was not just queue memory, but the direct combinational path from rule/CALL completion logic into the step/status accounting path.

The cost is meaningful: LUT increased to `10591` and FF to `4425`, and the design now uses separate helper modules instead of one compact inlined loop. This is still a useful FPGA-small result because it demonstrates a measurable throughput/timing tradeoff suitable for Chapter 4.

## Next Recommendation

The next try should keep the deferred completion boundary but reduce area. A good follow-up is to make the helper-module boundary intentional: selectively inline the small rules (`LINK`, `ERASE`, `ANNIHILATE`) while keeping `CALL`, `OPERATE`, `SWITCH`, and `COMMUTE` staged or non-inlined. The goal is to preserve `II=1` while recovering some of the LUT/FF growth.
