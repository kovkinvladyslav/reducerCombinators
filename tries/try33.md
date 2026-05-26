# Try 33: Pending Status Commit Service

Current status: `failed_csynth_resources`

Branch: `try33`

Architecture: preserve try31/try32 selected-command staging, but defer selected
command error/status writes through a narrow pending-status service.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try32 kept scheduler `II=1` and improved timing to `4594 LUT`, `2669 FF`, slack `-3.19`, but Vitis reported the remaining critical path through shared scheduler status/control into `local_status_code`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_PENDING_STATUS`, `PARALLEL_SERVICE_STATUS`, pending-status registers, focused selected-command deferred status writes, and a focused pending-status synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, try33 focused syntax, full CSIM, and FPGA-small CSIM passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `4772 LUT`, `2282 FF`, `0 DSP`, `0 BRAM`, but still failed timing with slack `-3.16` and estimated Fmax `95.58 MHz`. |

## Goal

Test whether the focused selected-command scheduler is limited by the shared
`local_status_code` phi/store rather than by a specific interaction rule
datapath.

## Hypothesis

The try32 critical path is broad because multiple service branches can write
`local_status_code` directly. Deferring selected-command errors into a tiny
pending-status register and committing them through one status service should
reduce shared status mux pressure while preserving externally visible status
semantics.

## Difference From Prior Tries

Try31 split selected-command capture from classification. Try32 split staged
`LINK` writeback from produced-pair/completion accounting. Try33 does not add
another rule stage. It changes only how errors from the focused selected-command
path reach `local_status_code`:

```text
service phase detects error
service phase records pending status
status service writes local_status_code
main loop exits on committed non-success status
```

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_PENDING_STATUS`
  - default `0`
- Add a service phase:
  - `PARALLEL_SERVICE_STATUS`
- Add local pending-status registers:
  - `pending_status_valid`
  - `pending_status_code`
- Use the pending-status path only in the focused selected-command profile.
- Defer selected-command staged-`LINK` errors:
  - non-variable `LINK` produced-pair overflow
  - invalid substitution variable index
  - staged `LINK` append-result overflow
  - selected-command commit overflow
- Preserve direct status behavior when the gate is disabled.
- Preserve full/default behavior because the gate is default-off.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_pending_status_nostats.cfg`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try33 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_PENDING_STATUS=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t33_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir x33
```

Focused pending-status CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_pending_status_nostats.cfg --work_dir csyn_t33_parallel_pending_status
```

## Results

- Host syntax: PASS
- Try33 focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the testbench still expects them.
- Full CSIM: PASS (`csim_t33_full`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`x33`, `Overall Result: PASS`)
- CSYNTH: completed, kept scheduler `II=1`, but failed timing/resource
  acceptance.

## HLS Evidence

- Work directory: `csyn_t33_parallel_pending_status`
- Report paths:
  - `csyn_t33_parallel_pending_status/hls/syn/report/csynth.rpt`
  - `csyn_t33_parallel_pending_status/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t33_parallel_pending_status/hls/syn/report/csynth_design_size.rpt`
- LUT: `4772`
- FF: `2282`
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_880_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-3.16`; estimated Fmax
  `95.58 MHz`; estimated clock period `10.463 ns`
- Critical path evidence: the critical path still flows from
  `global_queue_count_value` through redex-queue empty/refill control into the
  `local_status_code` phi/store. The incoming status is now a selected value
  around `src/reducer_core_parallel.cpp:1294`, so the pending-status service
  did not remove the shared status mux; it only changed which status-producing
  branch feeds it.
- Design-size evidence: final HW-transform design size increased from try32's
  `823` to `867` instructions. `defer_parallel_status` appears as a 159
  instruction contribution with three calls. FF decreases from try32, but LUT
  rises.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try33 is not an accepted endpoint. It confirms that a naive pending-status
service is not enough to remove the remaining critical path.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try31 split classify | 4602 | 2667 | 0 | 0 | 1 | `-4.35` | `85.82 MHz` |
| Try32 split `LINK` writeback | 4594 | 2669 | 0 | 0 | 1 | `-3.19` | `95.28 MHz` |
| Try33 pending status | 4772 | 2282 | 0 | 0 | 1 | `-3.16` | `95.58 MHz` |

The pending-status idea reduces FF usage substantially, but it increases LUTs
and barely improves timing. The critical path remains dominated by global
queue/refill control feeding the shared `local_status_code` phi/store. This
means the remaining issue is not simply "too many branches write status";
instead, the parent pipeline still combines global queue refill control and
status/control resolution in one loop body.

## Next Recommendation

The next try should not deepen the status protocol further. A more direct next
probe is to split REFILL itself from global queue count mutation:

- keep try31/try32 staging;
- avoid the try33 pending-status path;
- add a refill-capture phase that decides which lanes need refill;
- add a separate refill-pop phase that mutates `global_queue_count_value`;
- leave selected-command execution/status unchanged from try32.

This targets the reported path from `global_queue_count_value` through
redex-queue empty/refill selection into status/control.
