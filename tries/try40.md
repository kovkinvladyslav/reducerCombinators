# Try 40: Direct Staged LINK Implicit Success

Current status: `failed_csynth_resources`

Branch: `try40`

Architecture: return to the try38 focused parallel base and remove the staged
selected-command `LINK` success path from the `local_status_code` check in the
direct-append FPGA-small profile.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try39 completed but regressed slightly to `3751 LUT`, `1149 FF`, `II=1`, about `-2.00` slack; Vitis showed the cached unbound bit in the same `local_status_code` critical path. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_LINK_IMPLICIT_SUCCESS` and a focused implicit-success synthesis config on the try38 direct-append base. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a40`), and FPGA-small CSIM (`b40`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `3749 LUT`, `1148 FF`, `0 DSP`, `0 BRAM`, and estimated Fmax `107.85 MHz`; this matches try38 and remains timing-negative. |

## Goal

Reduce status/control mux pressure in selected-command staged `LINK` completion
without repeating the generic pending-status service from try33.

## Hypothesis

In the try38 focused profile, selected-command staged `LINK` uses a compact
produced-pair buffer and direct slot-0 append. That direct path has no runtime
queue-overflow branch in the append-result tick. Therefore, once the staged
`LINK` transaction reaches completion in that profile, success is implicit and
does not need to be guarded by a read of `local_status_code`.

Removing that success check may break the reported dependency from the
substitution-memory read/write path into `local_status_code` selection while
preserving the default full behavior behind a compile-time gate.

## Difference From Prior Tries

Try33 deferred many status writes through a generic pending-status service and
increased LUT/timing pressure. Try39 cached the unbound-marker predicate and
made the predicate itself part of the critical path. Try40 does neither:

- no pending-status service;
- no cached unbound predicate;
- no extra staged `LINK` phase;
- only the success-completion check in the already direct, compact staged
  `LINK` append-result path is specialized.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_LINK_IMPLICIT_SUCCESS`
  - default `0`
- Enable it only in a focused config together with:
  - `ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1`
  - `ENABLE_PARALLEL_DIRECT_LINK_APPEND=1`
  - `ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1`
- In selected-command staged `LINK` completion:
  - when the direct compact path is active, count the completed `LINK`
    unconditionally after resetting staged `LINK` state;
  - keep the existing `local_status_code == REDUCER_STATUS_SUCCESS` guard for
    all other profiles.
- Do not enable `ENABLE_PARALLEL_LINK_READ_DECODE`.
- Add a focused synthesis config for the implicit-success variant.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try40 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_IMPLICIT_SUCCESS=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a40
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b40
```

Focused implicit-success CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_direct_link_implicit_success_nostats.cfg --work_dir csyn_t40_parallel_link_implicit_success
```

## Results

- Host syntax: PASS
- Try40 focused syntax: PASS for both `src/reducer_core_parallel.cpp` and
  `test/reducer_test.cpp`.
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the full testbench still expects them.
- Full CSIM: PASS (`a40`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`b40`, `Overall Result: PASS`)
- CSYNTH: completed, matched the try38 focused hardware point, but did not meet
  timing acceptance.

## HLS Evidence

- Work directory: `csyn_t40_parallel_link_implicit_success`
- Report paths:
  - `csyn_t40_parallel_link_implicit_success/hls/syn/report/csynth.rpt`
  - `csyn_t40_parallel_link_implicit_success/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t40_parallel_link_implicit_success/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_902_3_csynth.rpt`
  - `csyn_t40_parallel_link_implicit_success/hls/syn/report/csynth_design_size.rpt`
- LUT: `3749` top-level total, `3591` in the scheduler pipeline
- FF: `1148` top-level total, `1077` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_902_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: estimated clock period `9.272 ns`, estimated Fmax
  `107.85 MHz`. With the configured `2.70 ns` uncertainty, this remains about
  `-1.97 ns` against the effective `7.30 ns` budget.
- Critical path evidence: Vitis still reports
  `substitution_memory_0_load -> selected_command_link_previous_port -> unbound-marker comparison -> local_status_code phi/store`.
  Removing the explicit success guard in the direct compact `LINK` completion
  path did not remove the status phi from the scheduler loop.
- Design-size evidence: final HW-transform design size is `574` instructions,
  matching try38 and below try39's `576`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try40 is a neutral/negative result. It confirms that the final
`local_status_code == SUCCESS` guard after selected-command staged `LINK`
completion was not the part of the status mux that HLS was reporting. The
focused netlist returns to the try38 resource/timing point:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try38 direct staged `LINK` append | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |
| Try39 staged `LINK` read-decode bit | 3751 | 1149 | 0 | 0 | 1 | about `-2.00` | `107.48 MHz` |
| Try40 direct staged `LINK` implicit success | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |

The FPGA lesson is useful: after direct append, HLS still couples
substitution-memory read/compare with the shared loop status phi. The success
guard after append is not a meaningful lever. The remaining pressure is earlier
in the staged `LINK` writeback tick, where the unbound-marker branch and
service-control/status merge still sit inside one scheduler pipeline.

## Next Recommendation

Return to try38 as the best focused base and stop changing completion/status
guards. The next distinct try should split the staged `LINK` writeback tick
itself:

- read previous substitution port in one service tick;
- compare/decide bind-vs-emit in a separate narrow service tick or state;
- write substitution memory in a separate tick from status/service selection;
- avoid the try39 cached-bit pattern that is consumed in the same loop-carried
  region.
