# Try 35: Single-Site Parallel Refill Pop

Current status: `failed_csynth_resources`

Branch: `try35`

Architecture: keep the try34 refill service split, but replace the lane0/lane1
priority-coded global pop with a captured target-lane register and exactly one
syntactic `active_pair_queue_pop_global` call site.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try34 completed synthesis but failed timing/resources at `4680 LUT`, `2118 FF`, slack `-4.11`. Its design-size report still showed two `active_pair_queue_pop_global` call sites, so the global pop datapath was not truly single-site for HLS. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SINGLE_REFILL_POP`, `refill_target_lane`, a single-pop REFILL_POP implementation, and a focused single-pop synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full CSIM, and FPGA-small CSIM passed. The first full and small CSIM work directories compiled but failed to launch `csim.exe`; short work directories `a35` and `b35` passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `4003 LUT`, `2114 FF`, `0 DSP`, `0 BRAM`, but still failed timing with slack `-2.05` and estimated Fmax `107.01 MHz`. |

## Goal

Test whether the refill split becomes more FPGA-friendly when the global queue
pop is exposed to HLS as one datapath instead of two priority-coded pop call
sites.

## Hypothesis

Try34's concept was reasonable, but the implementation still had two syntactic
global pop calls:

```text
if lane0 needs refill: pop global
else if lane1 needs refill: pop global
```

HLS therefore still built a broad priority structure around global queue count
and local lane routing. Capturing `refill_target_lane` in REFILL and performing
exactly one global pop in REFILL_POP should reduce queue-pop duplication and
make the service boundary more explicit in hardware.

## Difference From Prior Tries

Try34 split refill into capture and pop phases, but the pop phase still had two
pop sites. Try35 changes the structure of the pop service itself:

```text
REFILL: choose one target lane
REFILL_POP: perform one global pop call
REFILL_POP: route result to target lane
```

This is not the try33 pending-status path and not a repeat of try34; the
meaningful difference is one HLS-visible global queue pop datapath.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SINGLE_REFILL_POP`
  - default `0`
- Use this gate only with `ENABLE_PARALLEL_SPLIT_REFILL`.
- Add one state bit:
  - `refill_target_lane`
- In split REFILL when single-pop mode is enabled:
  - inspect lane0/lane1 empty state;
  - choose lane0 first when both are empty;
  - store the selected lane in `refill_target_lane`;
  - move to `PARALLEL_SERVICE_REFILL_POP` only when a selected empty lane and
    global work both exist.
- In REFILL_POP:
  - call `active_pair_queue_pop_global` exactly once;
  - push the popped pair to `refill_target_lane`;
  - return to REFILL so the next tick re-checks queue/local state before
    selecting work.
- Preserve try34 behavior when `ENABLE_PARALLEL_SINGLE_REFILL_POP` is disabled.
- Keep try31/try32 boundaries enabled.
- Do not enable try33 pending status.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try35 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir f35
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir x35
```

Focused single-pop refill CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_nostats.cfg --work_dir csyn_t35_parallel_single_refill_pop
```

Final measured CSYNTH rerun after removing unused try34 refill state from the
single-pop build:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_nostats.cfg --work_dir csyn_t35_parallel_single_refill_pop_clean
```

## Results

- Host syntax: PASS
- Try35 focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the testbench still expects them.
- Full CSIM: PASS on rerun (`a35`, `Overall Result: PASS`). The first work
  directory `f35` compiled but Vitis failed to launch `csim.exe` with
  `invalid argument`; this matches prior Vitis launcher issues and was not a
  reducer mismatch.
- FPGA-small CSIM: PASS on rerun (`b35`, `Overall Result: PASS`). The first
  work directory `x35` hit the same launcher issue.
- CSYNTH: completed, kept scheduler `II=1`, but failed timing/resource
  acceptance.

## HLS Evidence

- Work directory: `csyn_t35_parallel_single_refill_pop_clean`
- Report paths:
  - `csyn_t35_parallel_single_refill_pop_clean/hls/syn/report/csynth.rpt`
  - `csyn_t35_parallel_single_refill_pop_clean/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t35_parallel_single_refill_pop_clean/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_894_3_csynth.rpt`
  - `csyn_t35_parallel_single_refill_pop_clean/hls/syn/report/csynth_design_size.rpt`
- LUT: `4003` top-level total, `3845` in the scheduler pipeline
- FF: `2114` top-level total, `2043` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_894_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-2.05`; estimated Fmax
  `107.01 MHz`; estimated clock period `9.345 ns`
- Critical path evidence: the queue-pop path is no longer the reported limiter.
  Vitis reports the critical path from `executed_steps` through the loop
  condition, `local_status_code` phi/control, and back into the
  `executed_steps` select/store. This is a scheduler completion-accounting path,
  not the try34 global queue pop/count path.
- Design-size evidence: final HW-transform design size is `808` instructions,
  down from try34's `872`. The design-size report shows one
  `active_pair_queue_pop_global` contribution at `452` instructions instead of
  try34's two-call `904` contribution.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try35 is not an accepted endpoint because timing still misses the configured
10 ns clock with 2.70 ns uncertainty. However, it is the strongest focused
parallel scheduler result in this sequence and validates the single-pop
hypothesis.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try32 split `LINK` writeback | 4594 | 2669 | 0 | 0 | 1 | `-3.19` | `95.28 MHz` |
| Try33 pending status | 4772 | 2282 | 0 | 0 | 1 | `-3.16` | `95.58 MHz` |
| Try34 split refill pop | 4680 | 2118 | 0 | 0 | 1 | `-4.11` | `87.67 MHz` |
| Try35 single-site refill pop | 4003 | 2114 | 0 | 0 | 1 | `-2.05` | `107.01 MHz` |

The key evidence is structural: the design-size report now shows one global
pop call contribution instead of two. This removes the queue-pop path from the
reported critical path and reduces LUTs substantially. The remaining limiter is
now completion/status control around `executed_steps`, which is a different
problem from the refill pop datapath.

## Next Recommendation

Keep try35 as the new best focused parallel base. The next try should target
completion accounting rather than refill:

- preserve single-site refill pop;
- avoid try33's broad pending-status service;
- split selected-command success completion so `executed_steps` is updated in a
  narrow completion service, similar in spirit to the single-lane
  deferred-completion win from try5;
- keep error/status handling direct unless the report proves status is again
  the limiter.

The expected hardware goal is to remove the loop-condition to
`executed_steps` select/store path from the same tick as selected-command
execution/control.
