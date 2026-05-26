# Try 2: Produced-Pair Buffer For Queue Decoupling

Current status: `csynth_passed`

Branch: `try2`

Architecture: single-lane produced-pair buffer between rule execution and the active-pair queue

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Queue decoupling architecture plan recorded after try1 identified `redex_memory` as the remaining FPGA-small II limiter. |
| 2026-05-26 | `implemented` | Added `ENABLE_PRODUCED_PAIR_BUFFER`, buffered `LINK`/`ERASE`/`ANNIHILATE`, and added CSIM/CSYNTH configs. |
| 2026-05-26 | `csim_passed` | Full CSIM and FPGA-small produced-buffer CSIM passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | FPGA-small produced-buffer synthesis completed and generated reports; II remained `4`. |

## Goal

Reduce the scheduling pressure caused by reading an active pair from the queue and pushing rule-produced active pairs back into the same queue RAM inside one reducer loop body.

## Hypothesis

The current single-lane reducer lets rule implementations push directly into the active-pair queue. HLS reports for try1 show the FPGA-small staged-`CALL` build now completes synthesis, but the main loop still finishes at `II=4` because of a memory dependency on `redex_memory`.

If small fixed-size rules first emit produced pairs into a local bounded buffer, and the reducer services that buffer separately from popping the next input pair, then the queue read/write dependency becomes explicit and measurable. The first implementation may not reach `II=1`, but it should create a cleaner hardware boundary for future queue banking or ping-pong services.

## Difference From Prior Tries

Try1 changed the architecture of `CALL` by moving packed-book instantiation into a staged service. Try2 does not modify `CALL` semantics or graph-instantiation policy. Instead, it targets the active-pair queue service that try1 synthesis identified as the remaining small-profile II limiter.

## Implementation Plan

1. Inspect `reducer_core.cpp`, `active_pair_queue.hpp`, `redex_queue.hpp`, and rule push paths to identify which single-lane rules write directly to `redex_memory`.
2. Add a small fixed-capacity `ProducedPairBuffer` type, using static arrays and simple counters.
3. Add helper functions to append produced pairs into the buffer and flush one produced pair per reducer service tick into the existing queue.
4. Start only with bounded, simple single-lane rules where the produced-pair count is clear:
   - `VOID`
   - `LINK`
   - `ERASE`
   - `ANNIHILATE`
5. Leave `COMMUTE`, `OPERATE`, `SWITCH`, and staged `CALL` on the existing direct queue path unless the code inspection shows a safe narrow extension.
6. Preserve full-profile correctness behavior and existing statuses:
   - queue full remains `REDUCER_STATUS_REDEX_QUEUE_OVERFLOW`
   - malformed interaction statuses remain unchanged
   - `performed_steps` still counts completed interactions, not buffer flush ticks
7. Add a compile-time gate:
   - `ENABLE_PRODUCED_PAIR_BUFFER`
   - default `0` initially
   - synthesis config can enable it for measurement
8. Add documentation describing the current queue dependency and the intended future banking/free-list path.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try2
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir interactReduce_csim_small_try2
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_produced_buffer.cfg --work_dir interactReduce_csynth_small_produced_buffer_try2
```

## Results

- Host syntax: passed for default full profile and `-DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1`.
- Host execution: not used; Vitis CSIM is the authoritative runtime check on this Windows setup.
- Full CSIM: passed in `interactReduce_csim_full_try2` with `Overall Result: PASS`.
- FPGA-small CSIM: passed in `interactReduce_csim_small_produced_buffer_try2` with `Overall Result: PASS`.
- CSYNTH: completed in `interactReduce_csynth_small_produced_buffer_try2`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_produced_buffer_try2`
- Report paths:
  - `interactReduce_csynth_small_produced_buffer_try2/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_produced_buffer_try2/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_produced_buffer_try2/hls/syn/report/csynth_design_size.rpt`
- LUT: `8197` (`39%` of `xc7a35t-cpg236-1`)
- FF: `3598` (`8%`)
- DSP: `2` (`2%`)
- BRAM: `0` internal BRAM reported
- II: target `1`, final `4`
- II violation type: memory dependency on `redex_memory`
- Timing/slack: estimated top clock `11.309 ns` for a `10 ns` target; summary slack `-4.01`
- Design size: `59814` compile/link instructions, reduced to `1434` after HW transforms
- Last Vitis log line if frozen: not applicable; synthesis completed.

Comparison with try1:

```text
try1 FPGA-small staged CALL:
LUT 7804, FF 3254, DSP 2, II 4

try2 FPGA-small produced-pair buffer:
LUT 8197, FF 3598, DSP 2, II 4
```

The local buffer introduced a small memory for buffered pair ports and did not
remove the queue dependency because staged `CALL` and other rules still contain
direct `redex_memory` stores inside the same pipelined loop.

## Conclusion

Try2 is a completed negative architecture result. It preserves correctness and
synthesizes, but partial buffering of only `LINK`, `ERASE`, and `ANNIHILATE`
does not improve the main-loop `II=4`. The result is still valuable because it
shows that queue hardening must be structural and complete, not a small-rule
local buffer bolted onto the existing loop.

## Next Recommendation

The next try should make queue access single-owner across the reducer by routing
all produced pairs through one commit service, including `COMMUTE`, `SWITCH`,
`OPERATE`, and staged `CALL`. If that is too invasive, try3 should instead
split the reducer loop into explicit pop, execute, and commit phases so HLS no
longer sees queue pop and arbitrary rule pushback as one mixed memory region.
