# Try 45: Compact Classify Service

Current status: `failed_csynth_resources`

Branch: `try45`

Architecture: return to the try41 focused scheduler base and use a compact
focused-rule decoder only inside the selected-command CLASSIFY service.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try44 proved that compact classification in SELECT is the wrong phase: it regressed to `3842 LUT`, `1156 FF`, `II=1`, `68.57 MHz`, and `-7.29 ns` slack by coupling rule decode to refill/local-queue movement. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_COMPACT_CLASSIFY_SERVICE`, reused the compact focused decoder in CLASSIFY only, and added the focused synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a45`), and FPGA-small CSIM (`b45_rerun`) passed. The first FPGA-small CSIM attempt hit Vitis `csim.exe invalid argument`, then passed in a fresh work directory. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed, but returned to the try41 point rather than improving it: `3707 LUT`, `1150 FF`, `II=1`, `118.28 MHz`, and `-1.16 ns` slack. |

## Goal

Reduce selected-command rule-decode fan-in while preserving the phase boundary
between candidate selection/refill and selected-command classification.

## Hypothesis

Try43 failed because staged non-link validation reused and widened the
CLASSIFY controller. Try44 failed because compact classification moved into
SELECT, where queue/refill movement already dominates the service tick.

The focused synthesis profile disables `COMMUTE`, `OPERATE`, `SWITCH`, and
`CALL`, so the selected-command CLASSIFY service only needs to distinguish:

```text
LINK
VOID
ERASE
ANNIHILATE
UNSUPPORTED
```

If the compact decoder is used only in `PARALLEL_SERVICE_CLASSIFY`, SELECT can
continue to capture a candidate and move to CLASSIFY without dragging decode
logic into the queue path. This should test whether rule-decode fan-in can be
reduced without repeating try44's phase-placement mistake.

## Difference From Prior Tries

Try44 computed the compact rule kind during SELECT and skipped CLASSIFY. Try45
keeps the SELECT-to-CLASSIFY boundary and changes only the decoder used by the
CLASSIFY service. It also does not enable try43's staged non-link validation
state.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_COMPACT_CLASSIFY_SERVICE`
  - default `0`
- Reuse the existing focused compact rule-kind helper or rename it to make its
  phase-neutral use clear.
- In selected-command SELECT:
  - preserve the try41 behavior and route to `PARALLEL_SERVICE_CLASSIFY`;
  - do not compute selected-command rule kind during SELECT.
- In `PARALLEL_SERVICE_CLASSIFY`:
  - when the new gate is enabled, compute `selected_command_rule_kind` with the
    compact focused decoder;
  - otherwise preserve `get_interaction_rule_kind`.
- Keep these gates disabled in the measured profile:
  - `ENABLE_PARALLEL_SELECTED_COMPACT_CLASSIFY`
  - `ENABLE_PARALLEL_DIRECT_NON_LINK_OUTPUTS`
  - `ENABLE_PARALLEL_STAGED_NON_LINK_VALIDATION`
- Add a focused synthesis config extending try41 with the new gate.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try45 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_COMPACT_CLASSIFY_SERVICE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a45
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b45
```

Focused compact-classify-service CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try45_compact_classify_service_nostats.cfg --work_dir csyn_t45_parallel_compact_classify_service
```

## Results

- Host syntax: PASS with the default full-profile testbench.
- Focused syntax: PASS for both `src/reducer_core_parallel.cpp` and
  `test/reducer_test.cpp` with the try45 flag set.
- Host execution: covered by Vitis CSIM runs.
- Full CSIM: PASS (`a45`, `Overall Result: PASS`).
- FPGA-small CSIM: first attempt in `b45` failed before execution with Vitis
  `csim.exe invalid argument`; rerun in `b45_rerun` passed with
  `Overall Result: PASS`.
- CSYNTH: completed, but did not improve resources or timing versus try41.

## HLS Evidence

- Work directory: `csyn_t45_parallel_compact_classify_service`
- Report paths:
  - `csyn_t45_parallel_compact_classify_service/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t45_parallel_compact_classify_service/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1025_3_csynth.rpt`
  - `csyn_t45_parallel_compact_classify_service/hls/syn/report/csynth_design_size.rpt`
- LUT: `3707` top-level total, `3549` in the scheduler pipeline.
- FF: `1150` top-level total, `1079` in the scheduler pipeline.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1025_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`.
- Timing/slack: estimated clock `8.455 ns`, estimated Fmax `118.28 MHz`.
  With `2.70 ns` uncertainty, slack remains `-1.16 ns` against the effective
  `7.30 ns` budget.
- Final HW-transform design size: `560` instructions.
- Critical path evidence: Vitis reports the same non-link/status path as
  try41:

```text
selected_command_produced_active_pairs.produced_pairs.left_port
is_erasure_port / node address select
node address range check
local_status_code phi/store
```

- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try45 is a neutral negative result.

The phase placement is correct relative to try44: compact classification no
longer couples to SELECT/refill, and timing returns from try44's `68.57 MHz`
back to try41's `118.28 MHz`. However, it does not improve over try41 at all.
This means the generic full rule decoder inside CLASSIFY was not the current
limiter for the focused scheduler profile.

The useful FPGA lesson is that once selected-command capture/classify is
properly phased, the remaining bottleneck is selected non-link execution:
`ERASE`/`ANNIHILATE` node-address selection and range checking still feed the
shared status register in one service tick.

## Next Recommendation

Return to try41 as the preferred focused base. Do not keep trying to optimize
classification alone.

The next distinct attempt should target the reported non-link/status path
without reusing the heavy try43 validation controller. A promising direction is
to split selected non-link execution into:

```text
READ_NODE_DATA
EMIT_NON_LINK_RESULT
```

where the first tick validates node addresses and captures node data/status,
and the second tick constructs produced pairs. This must use a dedicated narrow
non-link service state, not the existing CLASSIFY service, because try43 showed
that reusing CLASSIFY widens rule-decode/service-phase control.
