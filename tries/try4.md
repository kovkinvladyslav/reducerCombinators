# Try 4: Phase-Specialized Reducer Control

Current status: `csynth_passed`

Branch: `try4`

Architecture: split commit/staged-service control from normal rule dispatch inside the reducer loop

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try3 improved II from `4` to `3` but moved the limiter to a long control/timing path. |
| 2026-05-26 | `implemented` | `reducer_core.cpp` now handles produced-pair flush and active staged `CALL` service as early loop phases with local status/step handling. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small produced-buffer CSIM passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | FPGA-small produced-buffer synthesis completed and generated reports in `interactReduce_csynth_small_phase_control_try4_rerun`. |

## Goal

Keep try3's single-owner queue commit service while shortening the reducer loop control path that now limits timing.

## Hypothesis

Try3 proved that routing all produced pairs through one commit service removes the main `redex_memory` dependency as the primary limiter. However, the loop still joins buffer flush, staged `CALL` service, normal rule dispatch, status mapping, and step accounting into one broad pipelined control region.

If the reducer handles commit-buffer flush and staged `CALL` service as early loop phases with their own local status/step handling, HLS should see fewer wide muxes feeding `interaction_completed`, `executed_steps`, and status outputs. This may improve timing or reduce control LUTs without changing interaction semantics.

## Difference From Prior Tries

Try1 staged `CALL`. Try2 added partial buffering. Try3 made queue writeback single-owner and improved II to `3`. Try4 does not add a new queue structure. It changes control staging so the existing commit service and staged `CALL` service are not merged into the same broad normal-dispatch control path.

## Implementation Plan

1. Keep `ENABLE_PRODUCED_PAIR_BUFFER` as the measurement gate.
2. Refactor only `reducer_core.cpp` control flow under the gate.
3. Handle active produced-pair flushing as an early phase:
   - flush one pair;
   - on overflow, map to `REDUCER_STATUS_REDEX_QUEUE_OVERFLOW`;
   - if the flushed interaction is complete, count the buffered rule and increment `executed_steps`;
   - continue the loop without falling through normal rule dispatch.
4. Handle active staged `CALL` as the next early phase:
   - service one staged step;
   - if it emitted produced pairs, activate the commit buffer and continue;
   - if it completed without produced pairs, count exactly one `CALL`;
   - map errors locally.
5. Leave normal rule dispatch mostly unchanged.
6. Preserve default full-profile behavior and all statuses.
7. Validate with full CSIM, FPGA-small produced-buffer CSIM, and FPGA-small synthesis.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try4
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_produced_buffer.cfg --work_dir interactReduce_csim_small_phase_control_try4
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_produced_buffer.cfg --work_dir interactReduce_csynth_small_phase_control_try4
```

## Results

- Host syntax: passed for default full profile and for `-DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1`.
- Host execution: not used; Vitis CSIM is the authoritative runtime check on this Windows setup.
- Full CSIM: passed in `interactReduce_csim_full_try4_rerun` with `Overall Result: PASS`.
- FPGA-small CSIM: passed in `interactReduce_csim_small_phase_control_try4_rerun` with `Overall Result: PASS`.
- CSYNTH: completed in `interactReduce_csynth_small_phase_control_try4_rerun`.
- Full-profile CSYNTH: not repeated for try4. Full-profile CSIM guarded semantics; FPGA-small CSYNTH was the primary architecture measurement because try1 already showed full-profile synthesis is dominated by full numeric datapaths (`48457 LUT`, `32215 FF`, `49 DSP`, `II=67` for full staged/no-statistics).

## HLS Evidence

- Work directory: `interactReduce_csynth_small_phase_control_try4_rerun`
- Report paths:
  - `interactReduce_csynth_small_phase_control_try4_rerun/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_phase_control_try4_rerun/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_phase_control_try4_rerun/hls/syn/report/csynth_design_size.rpt`
- LUT: `8306` (`39%` of `xc7a35t-cpg236-1`)
- FF: `3250` (`7%`)
- DSP: `2` (`2%`)
- BRAM: `0`
- II: target `1`, final `3`
- II violation type: resource limitation
- Timing/slack: estimated loop clock remains about `16.195 ns` for a `10 ns` target; summary slack `-8.89`
- Design size: `59115` compile/link instructions, reduced to `1422` after HW transforms
- Last Vitis log line if frozen: not applicable; synthesis completed.

Comparison:

```text
try1 FPGA-small staged CALL:
LUT 7804, FF 3254, DSP 2, II 4, redex_memory dependency

try2 FPGA-small partial produced-pair buffer:
LUT 8197, FF 3598, DSP 2, II 4, redex_memory dependency

try3 FPGA-small single-owner commit:
LUT 8323, FF 3277, DSP 2, II 3, resource/timing limitation

try4 FPGA-small phase-specialized control:
LUT 8306, FF 3250, DSP 2, II 3, resource/timing limitation
```

## Conclusion

Try4 preserved correctness and synthesized, but it did not materially improve the limiter. The early-phase control refactor slightly reduced LUT/FF versus try3, but final `II` stayed at `3` and timing slack stayed at `-8.89`.

This is useful negative evidence: adding early `continue` phases inside the same pipelined reducer loop is not enough to isolate the critical path. HLS still sees one broad pipeline region that contains commit-buffer state, staged `CALL` state, rule dispatch, status outputs, and step accounting.

## Next Recommendation

The next try should move beyond local control-flow refactoring and make the phase boundary architecturally visible. A meaningfully different approach is an explicit reducer service FSM or separate helper functions/states for commit, staged `CALL`, pop, execute, and completion accounting. The goal should be to prevent HLS from merging all phase decisions into the same hot-loop mux network.
