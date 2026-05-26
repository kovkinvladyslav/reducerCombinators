# Try 34: Split Parallel Refill Pop

Current status: `failed_csynth_resources`

Branch: `try34`

Architecture: preserve try31/try32 selected-command staging, avoid try33's
pending-status path, and split global refill selection from global queue count
mutation.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try33 completed synthesis but failed timing/resources at `4772 LUT`, `2282 FF`, slack `-3.16`. The critical path still ran from `global_queue_count_value` through refill/empty control into shared status/control. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SPLIT_REFILL`, `PARALLEL_SERVICE_REFILL_POP`, split-refill state, and a focused split-refill synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full CSIM, and FPGA-small CSIM passed. The first full CSIM work directory built but failed to launch `csim.exe`; short work directory `f34` passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `4680 LUT`, `2118 FF`, `0 DSP`, `0 BRAM`, but failed timing with slack `-4.11` and estimated Fmax `87.67 MHz`. |

## Goal

Reduce the remaining global scheduler-control critical path in the focused
parallel FPGA-small profile without adding another rule datapath or changing
interaction-combinator semantics.

## Hypothesis

Try33 showed that deferring status is not enough because the critical path is
still shaped by global queue refill control. The current REFILL phase both
detects empty local lanes and can pop from the global queue for multiple lanes
in the same pipelined service tick. Splitting this into:

```text
REFILL capture: decide which lanes need work
REFILL pop: mutate global queue head/count at most once per service tick
```

should narrow the logic driven by `global_queue_count_value`. This may spend an
extra scheduler tick during refill, but it is a better FPGA tradeoff if timing
improves while `II=1` is preserved.

## Difference From Prior Tries

Try31 split selected active-pair capture from rule classification. Try32 split
the final staged `LINK` writeback from produced-pair/completion accounting.
Try33 added a pending-status service and was a useful negative result.

Try34 does not deepen the status protocol and does not change rule execution.
It instead targets the queue/refill side of the reported critical path:

```text
local lane empty checks
global redex queue pop
global_queue_count_value mutation
transition to select/classify/execute
```

This is meaningfully different from try33 because the status path is left in
the try32 shape and the new service boundary is placed around global queue
ownership.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SPLIT_REFILL`
  - default `0`
- Add a service phase:
  - `PARALLEL_SERVICE_REFILL_POP`
- Use the new phase only when:
  - `ENABLE_PARALLEL_SERVICE_PHASE`
  - `ENABLE_PARALLEL_SELECTED_COMMAND`
  - `ENABLE_PARALLEL_SPLIT_REFILL`
- In the split REFILL capture phase:
  - inspect only local queue empty state;
  - record which lanes need refill;
  - if global queue and local work are both empty, terminate;
  - if any lane needs refill and the global queue is not empty, move to
    REFILL_POP;
  - otherwise move to SELECT.
- In REFILL_POP:
  - pop at most one global active pair into one recorded empty lane;
  - clear that lane's refill request;
  - stay in REFILL_POP only when another lane still needs refill and the
    previous pop succeeded;
  - otherwise move to SELECT.
- Preserve existing REFILL behavior when the gate is disabled.
- Keep try31/try32 boundaries enabled in the focused synthesis config.
- Do not enable `ENABLE_PARALLEL_PENDING_STATUS` in this try.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try34 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t34_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir x34
```

Focused split-refill CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_nostats.cfg --work_dir csyn_t34_parallel_split_refill
```

## Results

- Host syntax: PASS
- Try34 focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the testbench still expects them.
- Full CSIM: PASS on rerun (`f34`, `Overall Result: PASS`). The first work
  directory `csim_t34_full` compiled but Vitis failed to launch `csim.exe` with
  `invalid argument`; this matches prior Vitis launcher issues and was not a
  reducer mismatch.
- FPGA-small CSIM: PASS (`x34`, `Overall Result: PASS`)
- CSYNTH: completed, kept scheduler `II=1`, but failed timing/resource
  acceptance.

## HLS Evidence

- Work directory: `csyn_t34_parallel_split_refill`
- Report paths:
  - `csyn_t34_parallel_split_refill/hls/syn/report/csynth.rpt`
  - `csyn_t34_parallel_split_refill/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t34_parallel_split_refill/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_889_3_csynth.rpt`
  - `csyn_t34_parallel_split_refill/hls/syn/report/csynth_design_size.rpt`
- LUT: `4680` top-level total, `4522` in the scheduler pipeline
- FF: `2118` top-level total, `2047` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_889_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-4.11`; estimated Fmax
  `87.67 MHz`; estimated clock period `11.407 ns`
- Critical path evidence: the split-refill pop service did not remove the
  queue/status limiter. Vitis reports a path from `executed_steps` through the
  loop condition, a `redex_queue_count` write, a later `global_queue_count_value`
  load in `active_pair_queue_pop_global`, redex empty compare, and the
  `local_status_code` phi/store. The new refill state also appears as
  `refill_lane_needs_work` phi muxes in the pipeline report.
- Design-size evidence: final HW-transform design size is `872` instructions,
  higher than try32's `823` and try33's `867`. The report still shows two calls
  to `active_pair_queue_pop_global` because the lane0/lane1 priority pop code
  has two syntactic pop sites.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try34 is not an accepted endpoint. It confirms that simply splitting REFILL
capture from global queue pop is not sufficient.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try32 split `LINK` writeback | 4594 | 2669 | 0 | 0 | 1 | `-3.19` | `95.28 MHz` |
| Try33 pending status | 4772 | 2282 | 0 | 0 | 1 | `-3.16` | `95.58 MHz` |
| Try34 split refill pop | 4680 | 2118 | 0 | 0 | 1 | `-4.11` | `87.67 MHz` |

The FF reduction is real, but the timing regression is worse than both try32
and try33. The hardware reason is visible in the report: although the pop is
conceptually single-owner, the implementation still exposes `global_queue_count`
through queue pop/empty logic and then into the same shared loop/status
selection. It also leaves two syntactic `active_pair_queue_pop_global` call
sites, so HLS still has a broad priority structure rather than one narrow pop
datapath.

## Next Recommendation

The next try should keep try32 as the best timing base and make the global queue
pop datapath truly single-site:

- avoid try33 pending status and try34's duplicated lane0/lane1 pop calls;
- capture a one-bit `refill_target_lane` during REFILL;
- use exactly one `active_pair_queue_pop_global` call in REFILL_POP;
- after the pop, push to the selected lane with a small lane-target mux;
- alternatively, remove global refill from the parent loop entirely with a
  dedicated one-entry global-pop register before SELECT.

The key difference must be structural: one syntactic global pop service, not two
priority-coded pop call sites.
