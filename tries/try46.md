# Try 46: Dedicated Non-Link Read Service

Current status: `failed_csynth_resources`

Branch: `try46`

Architecture: preserve the try41 focused scheduler base and add a dedicated
selected non-link read/validation service for `ERASE` and `ANNIHILATE`.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try45 matched try41 exactly at `3707 LUT`, `1150 FF`, `II=1`, `118.28 MHz`, and `-1.16 ns` slack. The critical path still runs through selected non-link node-address selection/range check into `local_status_code`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_STAGED_NON_LINK_SERVICE`, a dedicated `PARALLEL_SERVICE_NON_LINK_READ` phase, selected non-link capture registers, and a focused synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a46`), and FPGA-small CSIM (`b46`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed, but regressed to `3752 LUT`, `1288 FF`, `II=1`, `81.81 MHz`, and `-4.92 ns` slack because the added service phase widened selected-rule/service-phase control. |

## Goal

Split selected `ERASE`/`ANNIHILATE` node validation and node-data reads from
produced-pair emission and completion, without reusing the broad CLASSIFY
controller that failed in try43.

## Hypothesis

Try43's staged non-link validation was semantically correct but used
`PARALLEL_SERVICE_CLASSIFY` for both decode and validation. That widened the
classification/service-phase path. Try46 instead adds a dedicated service
phase:

```text
CLASSIFY -> NON_LINK_READ -> EXECUTE -> COMMIT
```

The `NON_LINK_READ` tick pops the local selected pair, validates node
addresses, reads node data, and captures two produced-pair outputs. EXECUTE
then only emits those captured pairs and counts the interaction.

This adds one service tick for selected `ERASE`/`ANNIHILATE`, but it should
separate node-memory/range-check work from produced-pair emission and
completion control.

## Difference From Prior Tries

Try42 directly specialized produced-pair writes and did not help. Try43 staged
non-link validation through CLASSIFY and made timing worse. Try45 optimized
classification in the correct phase but matched try41.

Try46 is different because it introduces a narrow dedicated non-link read
service instead of adding more behavior to CLASSIFY or SELECT.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_STAGED_NON_LINK_SERVICE`
  - default `0`
- Add a new service phase:
  - `PARALLEL_SERVICE_NON_LINK_READ`
- Add selected non-link capture registers:
  - validated flag
  - two captured produced pairs
- In CLASSIFY:
  - classify normally;
  - for selected `ERASE`/`ANNIHILATE`, route to `NON_LINK_READ`;
  - otherwise route to EXECUTE.
- In `NON_LINK_READ`:
  - pop the selected local active pair;
  - validate node addresses;
  - read node data;
  - capture two produced pairs;
  - route validation errors to status/commit;
  - route success to EXECUTE.
- In EXECUTE:
  - if selected non-link output is validated, write the two captured pairs to
    the compact selected produced buffer and count one committed interaction.
- Keep `ENABLE_PARALLEL_STAGED_NON_LINK_VALIDATION=0` in the measured profile.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try46 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_STAGED_NON_LINK_SERVICE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a46
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b46
```

Focused non-link-service CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try46_non_link_service_nostats.cfg --work_dir csyn_t46_parallel_non_link_service
```

## Results

- Host syntax: PASS with the default full-profile testbench.
- Focused syntax: PASS for both `src/reducer_core_parallel.cpp` and
  `test/reducer_test.cpp` with the try46 flag set.
- Host execution: covered by Vitis CSIM runs.
- Full CSIM: PASS (`a46`, `Overall Result: PASS`).
- FPGA-small CSIM: PASS (`b46`, `Overall Result: PASS`).
- CSYNTH: completed, but timing and FF count regressed significantly versus
  try41/try45.

## HLS Evidence

- Work directory: `csyn_t46_parallel_non_link_service`
- Report paths:
  - `csyn_t46_parallel_non_link_service/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t46_parallel_non_link_service/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1025_3_csynth.rpt`
  - `csyn_t46_parallel_non_link_service/hls/syn/report/csynth_design_size.rpt`
- LUT: `3752` top-level total, `3594` in the scheduler pipeline.
- FF: `1288` top-level total, `1217` in the scheduler pipeline.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1025_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`.
- Timing/slack: estimated clock `12.223 ns`, estimated Fmax `81.81 MHz`,
  slack `-4.92 ns` against the effective `7.30 ns` budget.
- Final HW-transform design size: `571` instructions.
- Critical path evidence: the top path moved from selected non-link
  node-address validation to selected-rule/service-phase control:

```text
selected_command_produced_active_pairs.produced_pairs.right_port
get_interaction_rule_kind / ports_have_same_tag
selected_command_rule_kind sparse mux
parallel_service_phase select/store
```

- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try46 is a negative result.

The dedicated service phase is semantically correct, but it recreates the same
timing class as try43. Even though validation no longer reuses CLASSIFY, adding
a new service phase and validation flags widens the selected-command controller
enough that the top path moves back into rule-kind/service-phase selection.

The FPGA lesson is precise: simply adding another service state is not enough.
The controller encoding itself is now the bottleneck when a new selected
non-link stage is introduced.

## Next Recommendation

Do not continue by adding more selected-command service phases with the same
wide `selected_command_rule_kind` enum and `parallel_service_phase` routing.

A different next attempt should either:

- combine the non-link service idea with a narrow one-bit class field produced
  by compact classification, so the service phase does not branch on the full
  `InteractionRuleKind`; or
- return to try41 and reduce the original non-link/status path by specializing
  `ERASE` only or `ANNIHILATE` only, avoiding a new general non-link service
  state.
