# Try 51: Prevalidated Selected ERASE

Current status: `failed_csynth_resources`

Branch: `try51`

Architecture: keep try49's true class-only selected command and direct selected
`ERASE`, but move selected `ERASE` operand validation and node-data capture
into the CLASSIFY service before the selected EXECUTE service.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try50 synthesized at `3659 LUT`, `1165 FF`, `II=1`, and `118.28 MHz`, but the critical path still included selected ERASE node-address range check into `local_status_code`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_PREVALIDATED_SELECTED_ERASE`, CLASSIFY-side selected ERASE pair capture, EXECUTE-side captured-pair copy, and focused config `hls_config_try51_prevalidated_selected_erase_nostats.cfg`. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full-profile CSIM, and FPGA-small CSIM passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed, but timing stayed at `118.28 MHz`; resources regressed to `3698 LUT` and `1300 FF`. |

## Goal

Test whether selected `ERASE` timing improves when EXECUTE no longer performs
erasure-port selection, node-address extraction, node-address range checking,
and node-memory read in the same tick as produced-pair emission.

## Hypothesis

Try50 showed that helper-local pending status does not separate the hardware,
because HLS still merges the invalid-address path into the shared
`local_status_code` phi. A more structural separation is to perform selected
`ERASE` validation immediately after selected-command classification, capture
the two produced pairs, and let EXECUTE only pop/commit the selected command and
copy captured pairs into the compact produced buffer.

If HLS keeps that separation, the selected EXECUTE critical path should lose:

```text
is_erasure_port -> node-address select -> address range check -> node read
```

The tradeoff is an extra classify/prevalidation tick for selected `ERASE`.
This is acceptable for the experiment because it targets timing closure and
clear hardware staging, not immediate maximum throughput.

## Difference From Prior Tries

This is not another pending-status try. Try50 only moved the invalid status code
to a tiny register and left the direct selected `ERASE` helper doing validation
inside EXECUTE.

This is also narrower than try43 and try46. Those tries staged both selected
`ERASE` and selected `ANNIHILATE`, and try46 added a new
`PARALLEL_SERVICE_NON_LINK_READ` phase. Try51:

- applies only to selected `ERASE`;
- uses the existing CLASSIFY-to-EXECUTE boundary;
- keeps true class-only selected command;
- keeps selected `ANNIHILATE` on the existing path;
- does not add another generic service phase.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_PREVALIDATED_SELECTED_ERASE`
  - default `0`
- Add selected ERASE capture registers only when the gate is enabled:
  - valid flag
  - pair0 left/right
  - pair1 left/right
- In CLASSIFY, when the selected class is `ERASE`:
  - select erasure and binary-node ports;
  - validate the node address;
  - read node data;
  - capture the two produced pairs;
  - record unsupported interaction on malformed address;
  - route success to EXECUTE.
- In EXECUTE, when the selected class is `ERASE` and prevalidation is enabled:
  - pop the selected local active pair;
  - copy the captured pairs to `selected_command_produced_active_pairs`;
  - clear the valid flag;
  - count/commit one selected interaction.
- Keep the old direct selected `ERASE` path when the new gate is disabled.
- Add focused config:
  - `hls_config_try51_prevalidated_selected_erase_nostats.cfg`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try51 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1 -DENABLE_PARALLEL_DIRECT_SELECTED_ERASE=1 -DENABLE_PARALLEL_PREVALIDATED_SELECTED_ERASE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a51
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b51
```

Focused CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try51_prevalidated_selected_erase_nostats.cfg --work_dir csyn_t51_parallel_prevalidated_selected_erase
```

## Results

- Host syntax: passed for default testbench, focused top, and focused testbench syntax.
- Host execution: not run separately; covered by Vitis CSIM testbench.
- Full CSIM: passed with `hls_config.cfg` in work directory `a51`.
- FPGA-small CSIM: passed with `hls_config_fpga_small.cfg` in work directory `b51`.
- CSYNTH: completed in `csyn_t51_parallel_prevalidated_selected_erase`, but did not meet the timing or resource goal.

## HLS Evidence

- Work directory: `csyn_t51_parallel_prevalidated_selected_erase`
- Report paths:
  - `csyn_t51_parallel_prevalidated_selected_erase/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t51_parallel_prevalidated_selected_erase/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1192_3_csynth.rpt`
  - `csyn_t51_parallel_prevalidated_selected_erase/hls/syn/report/csynth_design_size.rpt`
- LUT: `3698` top-level total, `3540` in the scheduler pipeline.
- FF: `1300` top-level total, `1229` in the scheduler pipeline.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1192_3` reached `II=1`.
- Timing/slack: estimated clock `8.455 ns`, uncertainty `2.70 ns`, effective budget `7.30 ns`, estimated Fmax `118.28 MHz`; approximately `-1.155 ns` against the effective budget.
- Design size: `21,857` compile/link instructions and `609` HW-transform instructions.
- Last Vitis log line if frozen: not applicable; synthesis completed.

Critical path evidence:

```text
selected_command_produced_active_pairs.produced_pairs.left_port
is_erasure_port / selected ERASE prevalidation
node_address select
node-address range check
local_status_code phi/store
```

The visible HLS log reports:

```text
Cannot meet target clock period ... selected_command_lane_index ... combination delay: 9.01814 ns
critical path ... node_address ... local_status_code_write_ln1057
All loop constraints were satisfied.
Estimated Fmax: 118.28 MHz
```

## Conclusion

Try51 is a negative result.

The implementation preserves correctness under full and FPGA-small CSIM, and
it synthesizes without freezing. However, moving selected `ERASE` validation
into CLASSIFY does not improve the timing point because CLASSIFY and EXECUTE
remain arms of the same pipelined scheduler loop. HLS still merges the
prevalidation range-check failure into the shared `local_status_code` phi.

Compared with try50, LUT increases from `3659` to `3698` and FF increases from
`1165` to `1300`. The extra captured-pair registers and muxing are visible
hardware cost, but they do not isolate the status path enough to improve Fmax.

## Next Recommendation

Do not keep moving selected `ERASE` validation between existing service arms.
The next different approach should reduce the scheduler's shared status/control
surface itself. Two plausible directions:

- create a focused profile with a dedicated status-free fast loop for known
  valid selected `VOID`/`ERASE`/`ANNIHILATE`, with error detection handled by a
  separate pre-scan or debug profile; or
- split the scheduler into separate HLS functions/modules for refill/classify,
  execute, and commit so Vitis cannot merge all service arms into one
  `local_status_code` phi in a single pipelined loop.
