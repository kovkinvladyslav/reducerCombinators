# Try 30: Staged LINK For Selected Command

Current status: `failed_csynth_resources`

Branch: `try30`

Architecture: keep the try26/try29 selected-command scheduler boundary, but
split the selected-command `LINK` rule into bounded service ticks.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try29 restored `LINK` on the selected-command scheduler and synthesized at `3558 LUT`, `1422 FF`, `0 DSP`, `0 BRAM`, `II=2`, but failed timing with slack `-2.00` through the substitution-memory `LINK` path. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_STAGED_LINK`, a selected-command staged `LINK` path, and a focused staged-`LINK` synthesis config. Default and focused syntax checks passed. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small CSIM both completed with `Overall Result: PASS`. |
| 2026-05-26 | `failed_csynth_resources` | Corrected focused CSYNTH completed and reached scheduler `II=1`, but failed timing: `4770 LUT`, `2677 FF`, `0 DSP`, `0 BRAM`, slack `-7.29`, estimated Fmax `68.57 MHz`. |

## Goal

Test whether staging only the `LINK` substitution-memory operation can restore
positive timing while keeping the more realistic structural rule subset:

```text
VOID
LINK
ERASE
ANNIHILATE
```

## Hypothesis

Try29 failed because `LINK` performs substitution-memory read, previous-binding
comparison, substitution-memory write, produced-pair append, and status muxing
inside the selected-command execute phase. Splitting `LINK` into multiple
service ticks should remove the memory-read-to-status combinational path while
preserving the selected-command ownership model.

## Difference From Prior Tries

Try27 and try28 added more command state and made timing worse. Try29 restored
`LINK` without changing its rule body and exposed the `LINK` datapath as the
limiter.

Try30 is different because it does not add another command slot. It keeps one
selected command and changes only how selected-command `LINK` is executed:
begin, read previous substitution, then decide/write/produce.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_STAGED_LINK`
  - default `0`
- Use the gate only in the selected-command parallel path.
- Keep the normal `apply_lane_link_rule` path as the semantic reference for
  full/default builds.
- Add tiny staged `LINK` state for the selected command:
  - active flag
  - stage
  - variable index
  - other port
  - previous port
- On selected-command `LINK`:
  - pop the local active pair exactly once at staged `LINK` begin;
  - validate variable/non-variable cases;
  - read substitution memory in one service tick;
  - compare/write/append produced pair in a later service tick;
  - increment `executed_steps` exactly once when the `LINK` completes;
  - preserve queue-overflow and unsupported-interaction status behavior.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_nostats.cfg`

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Focused staged-`LINK` syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t30_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t30_small
```

Focused staged-`LINK` CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_nostats.cfg --work_dir csyn_t30_parallel_staged_link
```

## Results

Fill this after implementation.

- Host syntax: PASS
- Focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the testbench still expects them.
- Full CSIM: PASS (`csim_t30_full_final`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`csim_t30_small_final`, `Overall Result: PASS`)
- CSYNTH: completed, reached `II=1`, but failed timing/resource acceptance.

## HLS Evidence

- Work directory: `csyn_t30_parallel_staged_link_corrected`
- Report paths:
  - `csyn_t30_parallel_staged_link_corrected/hls/syn/report/csynth.rpt`
  - `csyn_t30_parallel_staged_link_corrected/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t30_parallel_staged_link_corrected/hls/syn/report/csynth_design_size.rpt`
- LUT: `4770`
- FF: `2677`
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_861_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-7.29`; estimated Fmax `68.57 MHz`
- Critical path evidence: the old try29 path through `substitution_memory`
  read and `previous_port` comparison is no longer the reported limiter.
  The corrected report instead shows a control/select path from
  `global_active_pair_memory_load` through lane-local queue selection,
  interaction-rule classification, `selected_command_rule_kind`, and the
  non-`LINK` execute-helper rule-kind store.
- Design-size evidence: the corrected focused design uses
  `execute_selected_non_link_active_pair_without_queue_commit`; the generic
  `execute_lane_active_pair_without_queue_commit` and `apply_lane_link_rule`
  are no longer reported as the selected-command execute body in the focused
  final design-size table.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try30 is a mixed result. It proves the staged-`LINK` idea is directionally
useful for scheduling: the focused structural profile with `LINK` enabled moves
from try29's `II=2` memory dependency to `II=1`. It also removes the
substitution-memory read/previous-port/status path as the reported critical
path.

It is not an acceptable FPGA architecture yet. Area increases substantially
from try29 (`3558 LUT`, `1422 FF`) to try30 (`4770 LUT`, `2677 FF`), and timing
falls to `68.57 MHz` with `-7.29` slack. The new limiter is broad
REFILL/SELECT/control classification feeding the selected-command execute
helper.

## Next Recommendation

Do not keep widening the selected-command parent pipeline. The next try should
keep the staged-`LINK` service idea but move SELECT/classification behind a
registered command boundary:

- split selected-command capture into at least two service ticks:
  1. lane queue peek/capture active pair;
  2. classify rule and execute or begin staged `LINK`;
- keep `LINK` read/decide/write staged;
- keep the non-`LINK` helper specialized so `apply_lane_link_rule` does not
  re-enter the focused datapath;
- measure whether the design can keep `II=1` while recovering timing.
