# Try 47: Narrow Selected Class With Non-Link Service

Current status: `failed_csynth_resources`

Branch: `try47`

Architecture: combine the try46 dedicated non-link read service with a narrow
selected-command class field so the new service does not branch on the full
`InteractionRuleKind` enum.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try46 was semantically correct but regressed to `3752 LUT`, `1288 FF`, `II=1`, `81.81 MHz`, and `-4.92 ns` slack. The critical path moved through `get_interaction_rule_kind`, `selected_command_rule_kind`, and `parallel_service_phase` selection. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_NARROW_SELECTED_CLASS`, selected-class constants, class capture state, narrow-class helper routing, and a focused synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a47`), and FPGA-small CSIM (`b47`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed, but regressed to `4220 LUT`, `1298 FF`, `II=1`, `64.59 MHz`, and about `-8.18 ns` slack. The class wrapper still creates the compact rule-kind path and adds a conversion back to the full enum. |

## Goal

Keep the architectural intent of staging selected non-link node reads, but
avoid driving the service transition from the full selected rule enum.

## Hypothesis

Try46 added the right kind of service boundary but kept the wrong control
encoding. The focused profile only needs to route selected commands as:

```text
LINK
VOID
ERASE
ANNIHILATE
OTHER/UNSUPPORTED
```

If CLASSIFY produces a narrow class field and the non-link service branches on
that field instead of `InteractionRuleKind`, HLS may avoid the sparse rule-kind
mux that dominated try46. This tests whether the service split failed because
of the staging idea itself or because the controller carried a wide enum.

## Difference From Prior Tries

Try45 used compact classification but did not add non-link staging. Try46 added
non-link staging but still routed through the full rule kind. Try47 combines
the two lessons: use narrow selected-command class state with the non-link read
service.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_NARROW_SELECTED_CLASS`
  - default `0`
- Add small selected-class constants:
  - `SELECTED_CLASS_LINK`
  - `SELECTED_CLASS_VOID`
  - `SELECTED_CLASS_ERASE`
  - `SELECTED_CLASS_ANNIHILATE`
  - `SELECTED_CLASS_UNSUPPORTED`
- Add a helper that maps a focused selected active pair to that class.
- Store `selected_command_class` when the gate is enabled.
- In CLASSIFY:
  - compute the narrow class;
  - derive `selected_command_rule_kind` only as compatibility metadata;
  - route `ERASE`/`ANNIHILATE` classes to `PARALLEL_SERVICE_NON_LINK_READ`.
- In `NON_LINK_READ`:
  - branch on selected class, not full rule kind.
- In EXECUTE:
  - use selected class for `LINK` and validated non-link paths when the gate is
    enabled.
- Add a focused synthesis config enabling both:
  - `ENABLE_PARALLEL_STAGED_NON_LINK_SERVICE=1`
  - `ENABLE_PARALLEL_NARROW_SELECTED_CLASS=1`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try47 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_STAGED_NON_LINK_SERVICE=1 -DENABLE_PARALLEL_NARROW_SELECTED_CLASS=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a47
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b47
```

Focused narrow-class CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try47_narrow_class_non_link_service_nostats.cfg --work_dir csyn_t47_parallel_narrow_class_non_link_service
```

## Results

- Host syntax: PASS with the default full-profile testbench.
- Focused syntax: PASS for both `src/reducer_core_parallel.cpp` and
  `test/reducer_test.cpp` with the try47 flag set.
- Host execution: covered by Vitis CSIM runs.
- Full CSIM: PASS (`a47`, `Overall Result: PASS`).
- FPGA-small CSIM: PASS (`b47`, `Overall Result: PASS`).
- CSYNTH: completed, but timing and resources regressed versus try41/try45
  and try46.

## HLS Evidence

- Work directory: `csyn_t47_parallel_narrow_class_non_link_service`
- Report paths:
  - `csyn_t47_parallel_narrow_class_non_link_service/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t47_parallel_narrow_class_non_link_service/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1066_3_csynth.rpt`
  - `csyn_t47_parallel_narrow_class_non_link_service/hls/syn/report/csynth_design_size.rpt`
- LUT: `4220` top-level total, `4062` in the scheduler pipeline.
- FF: `1298` top-level total, `1227` in the scheduler pipeline.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1066_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`.
- Timing/slack: estimated clock `15.482 ns`, estimated Fmax `64.59 MHz`, and
  about `-8.18 ns` slack against the effective `7.30 ns` budget.
- Final HW-transform design size: `649` instructions.
- Design-size evidence:
  - `get_narrow_selected_class`: `2730` compile/link instructions.
  - `get_selected_compact_rule_kind`: `2619` compile/link instructions.
  - `get_rule_kind_from_selected_class`: `123` compile/link instructions.
- Critical path evidence: the intended narrow class did not remove the broad
  decode/control path. The reported path still includes produced-pair state,
  compact rule-kind decoding, selected-class/rule-kind conversion, and
  `parallel_service_phase` selection:

```text
selected_command_produced_active_pairs.produced_pairs.right_port
get_selected_compact_rule_kind / ports_have_same_tag
selected_command_class mux
get_rule_kind_from_selected_class / selected_command_rule_kind compatibility
parallel_service_phase select/store
```

- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try47 is a negative result.

The architectural idea was reasonable, but the implementation did not actually
produce a narrow hardware control path. It first computed the compact
`InteractionRuleKind`, then mapped that to a class, then mapped the class back
to `InteractionRuleKind` for compatibility with existing selected-command
logic. HLS therefore saw more muxing, not less. The attempt confirms that
wrapping the existing rule-kind path is not enough; the focused hot path must
be class-only if this direction is tried again.

## Next Recommendation

Do not repeat the `pair -> rule_kind -> class -> rule_kind` structure.

The next try should either return to the try41 baseline and specialize one
non-link rule at a time, or implement a true class-only selected executor where
the focused path no longer stores or branches on `selected_command_rule_kind`.
That would be a deeper refactor and should be justified against the try47
evidence before implementation.
