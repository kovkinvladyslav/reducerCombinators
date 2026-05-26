# Try 48: True Class-Only Selected Command

Current status: `failed_csynth_resources`

Branch: `try48`

Architecture: replace the try47 compatibility wrapper with a true focused
class-only selected-command path for the measured FPGA-small scheduler profile.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try47 proved that `pair -> rule_kind -> class -> rule_kind` regresses to `4220 LUT`, `1298 FF`, `II=1`, `64.59 MHz`, and about `-8.18 ns` slack. The next attempt must remove the compatibility rule-kind round trip from the focused hot path. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED`, a direct class decoder, class-only selected CLASSIFY routing, direct selected `VOID` handling, and a focused synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a48`), and FPGA-small CSIM (`b48`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed at `3791 LUT`, `1162 FF`, `II=1`, `118.28 MHz`, and about `-1.16 ns` slack. This removes try47's large regression but does not improve the try41 baseline. |

## Goal

Reduce selected-command control depth in the focused parallel reducer by making
the measured FPGA-small selected-command path route on a direct class encoding
instead of `InteractionRuleKind`.

## Hypothesis

The try47 regression came from preserving the full rule-kind representation for
compatibility. A direct class decoder that returns only:

```text
LINK
VOID
ERASE
ANNIHILATE
UNSUPPORTED
```

and a selected executor that consumes that class without assigning
`selected_command_rule_kind` should remove the rule-kind compatibility mux from
the focused hot path. This should be more representative of the hardware
architecture we actually want: a small scheduler command class feeding
specialized rule services.

## Difference From Prior Tries

Try47 added a narrow class field but still computed and restored
`InteractionRuleKind`. Try48 is different because the focused profile will not
assign `selected_command_rule_kind` during selected-command classification and
will not route selected `VOID` or unsupported focused commands through the
generic rule-kind executor.

Try48 also avoids adding a new non-link service phase. It first tests whether
class-only selected execution itself is cheaper before combining it with
additional staging.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED`
  - default `0`
- Add `get_selected_direct_class(...)`, a direct port-pattern to class decoder
  that does not return `InteractionRuleKind`.
- Widen the existing `selected_command_class` state guard so it is available
  for try48 without enabling the try47 wrapper.
- In CLASSIFY when the gate is enabled:
  - compute only `selected_command_class`;
  - do not assign `selected_command_rule_kind`;
  - route directly to EXECUTE.
- In EXECUTE when the gate is enabled:
  - use class `LINK` for the existing staged LINK path;
  - handle class `VOID` directly by popping the local pair and committing one
    interaction with zero produced pairs;
  - leave class `ERASE` and `ANNIHILATE` on the existing focused non-link
    helper for this try unless a later try specializes them;
  - treat class `UNSUPPORTED` as `REDUCER_STATUS_UNSUPPORTED_INTERACTION`.
- Add a focused synthesis config enabling the try41 base plus:
  - `ENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1`
  - no try46/try47 staged non-link service.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try48 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a48
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b48
```

Focused class-only CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try48_true_class_only_selected_nostats.cfg --work_dir csyn_t48_parallel_true_class_only_selected
```

## Results

- Host syntax: PASS with the default full-profile testbench.
- Focused syntax: PASS for both `src/reducer_core_parallel.cpp` and
  `test/reducer_test.cpp` with the try48 flag set.
- Host execution: covered by Vitis CSIM runs.
- Full CSIM: PASS (`a48`, `Overall Result: PASS`).
- FPGA-small CSIM: PASS (`b48`, `Overall Result: PASS`).
- CSYNTH: completed. Timing returned to the try41/try45 level, but LUT and FF
  are higher than try41, so this is not an improvement.

## HLS Evidence

- Work directory: `csyn_t48_parallel_true_class_only_selected`
- Report paths:
  - `csyn_t48_parallel_true_class_only_selected/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t48_parallel_true_class_only_selected/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1106_3_csynth.rpt`
  - `csyn_t48_parallel_true_class_only_selected/hls/syn/report/csynth_design_size.rpt`
- LUT: `3791` top-level total, `3633` in the scheduler pipeline.
- FF: `1162` top-level total, `1091` in the scheduler pipeline.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1106_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`.
- Timing/slack: estimated clock `8.455 ns`, estimated Fmax `118.28 MHz`, and
  about `-1.16 ns` slack against the effective `7.30 ns` budget.
- Final HW-transform design size: `599` instructions.
- Design-size evidence:
  - `get_selected_direct_class`: `2726` compile/link instructions, `47`
    unroll/inline instructions, `45` array/struct instructions.
  - No `get_rule_kind_from_selected_class` compatibility function appears in
    the try48 report.
- Critical path evidence: the try47 rule-kind/service-phase path is gone, but
  the limiter returns to selected non-link node/status handling:

```text
selected_command_produced_active_pairs.produced_pairs.left_port
is_erasure_port / node-address select
node-address range check
local_status_code phi/store
```

- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try48 is a neutral negative result.

It proves that the try47 damage came from the compatibility round trip: once
the selected path uses a direct class decoder and avoids assigning
`selected_command_rule_kind` in focused CLASSIFY, timing returns from
`64.59 MHz` to `118.28 MHz`. However, it does not beat the try41 baseline:
`3791 LUT` and `1162 FF` are higher than try41's `3707 LUT` and `1150 FF`,
with the same estimated Fmax and slack.

Architecturally, this means class-only selected control is a valid interface
direction, but it is not the next resource/timing lever by itself. The current
limiter is again selected non-link node/status work.

## Next Recommendation

Return to the try41/try48 timing point and specialize the selected non-link
data path itself. The next try should target one rule at a time, most likely
selected `ERASE`, by separating node-address validation/status production from
produced-pair emission without adding a broad service-phase or rule-kind mux.
