# Try 3: Single-Owner Queue Commit Service

Current status: `csynth_passed`

Branch: `try3`

Architecture: route all reducer-produced active pairs through one explicit commit service

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try2 showed partial local buffering synthesized but kept `II=4`; try3 plans a complete single-owner queue write path. |
| 2026-05-26 | `implemented` | Buffered variants added for remaining queue-producing rules and staged `CALL`. |
| 2026-05-26 | `csim_passed` | Full CSIM and FPGA-small single-commit CSIM passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | FPGA-small single-commit synthesis completed; main-loop II improved from `4` to `3`. |

## Goal

Remove direct rule-level writes to `redex_memory` from the reducer execution path when the produced-pair buffer is enabled, so queue writeback has one explicit owner.

## Hypothesis

Try2 failed to improve `II` because only `LINK`, `ERASE`, and `ANNIHILATE` used the local produced-pair buffer. `COMMUTE`, `SWITCH`, `OPERATE`, safe reference duplication, and staged `CALL` still had direct queue stores in the same pipelined reducer loop.

If every rule-produced active pair is appended to a local produced-pair buffer and only the commit service writes that buffer into `redex_memory`, then the queue memory boundary becomes structurally clean. This may still not achieve `II=1`, but it should remove arbitrary rule pushback from the execute branch and produce stronger evidence for the next banking or phase-splitting step.

## Difference From Prior Tries

Try1 staged `CALL` as a graph-instantiation service. Try2 buffered only selected small-rule outputs and proved partial buffering is not enough. Try3 is meaningfully different because it targets all single-lane produced active pairs under one queue commit service.

## Implementation Plan

1. Keep `ENABLE_PRODUCED_PAIR_BUFFER` as the gate.
2. Reuse the existing `ProducedActivePairs` struct and commit flush path.
3. Add buffered variants or buffered helpers for:
   - `COMMUTE`
   - `OPERATE`
   - `SWITCH`
   - safe reference duplication
   - staged `CALL` redex/root emission
4. Preserve the old direct-push implementations when `ENABLE_PRODUCED_PAIR_BUFFER=0`.
5. Keep `performed_steps` unchanged semantically:
   - a rule with produced pairs completes only after the commit service flushes them;
   - staged `CALL` counts once after its full transaction and produced root/internal redexes are committed.
6. Preserve status behavior:
   - queue overflow -> `REDUCER_STATUS_REDEX_QUEUE_OVERFLOW`
   - node/variable/book overflows unchanged
   - malformed adjusted book ports -> `REDUCER_STATUS_UNSUPPORTED_INTERACTION`
7. Run CSIM before synthesis.
8. Compare against try1 and try2 resource/II evidence.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try3
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_produced_buffer.cfg --work_dir interactReduce_csim_small_single_commit_try3
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_produced_buffer.cfg --work_dir interactReduce_csynth_small_single_commit_try3
```

## Results

- Host syntax: passed for default full profile and `-DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1`.
- Host execution: not used; Vitis CSIM is the authoritative runtime check on this Windows setup.
- Full CSIM: passed in `interactReduce_csim_full_try3` with `Overall Result: PASS`.
- FPGA-small CSIM: passed in `interactReduce_csim_small_single_commit_try3` with `Overall Result: PASS`.
- CSYNTH: completed in `interactReduce_csynth_small_single_commit_try3`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_single_commit_try3`
- Report paths:
  - `interactReduce_csynth_small_single_commit_try3/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_single_commit_try3/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_single_commit_try3/hls/syn/report/csynth_design_size.rpt`
- LUT: `8323` (`40%` of `xc7a35t-cpg236-1`)
- FF: `3277` (`7%`)
- DSP: `2` (`2%`)
- BRAM: `0` internal BRAM reported
- II: target `1`, final `3`
- II violation type: resource limitation, not the prior `redex_memory` memory dependency
- Timing/slack: estimated loop clock `16.195 ns` for a `10 ns` target; summary slack `-8.89`
- Design size: `58137` compile/link instructions, reduced to `1399` after HW transforms
- Last Vitis log line if frozen: not applicable; synthesis completed.

Comparison:

```text
try1 FPGA-small staged CALL:
LUT 7804, FF 3254, DSP 2, II 4, memory dependency

try2 FPGA-small partial produced-pair buffer:
LUT 8197, FF 3598, DSP 2, II 4, memory dependency

try3 FPGA-small single-owner commit:
LUT 8323, FF 3277, DSP 2, II 3, resource/timing limitation
```

## Conclusion

Try3 succeeded as a queue-architecture experiment. Routing all rule-produced
active pairs through one commit service changed the HLS limiter from queue
memory dependency to resource/timing limitation and improved final II from `4`
to `3`.

This is not yet a final architecture. The cost is a longer control path:
capacity checks, staged `CALL` completion, buffer state, and status mapping now
share one broad reducer loop. The result proves the queue ownership direction is
valid, but the next step must shorten the loop rather than add more logic to it.

## Next Recommendation

Try4 should split the reducer into a small control FSM or explicit phases:
flush commit buffer, service staged `CALL`, pop input active pair, execute one
rule, then commit status/step accounting. The aim is to keep try3's single-owner
queue write path while reducing the long combinational path that now limits the
clock.
