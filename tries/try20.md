# Try 20: Non-LINK Structural Parallel Probe

Current status: `csynth_passed`

Branch: `try20`

Architecture: reuse the try19 phased structural parallel scheduler while
disabling `LINK` to isolate substitution-memory cost

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try19 completed at `4344 LUT`, `1821 FF`, `0 DSP`, `4 BRAM`, main loop `II=4`; HLS reported the remaining critical path through `substitution_memory_0` in `apply_lane_link_rule`. |
| 2026-05-26 | `implemented` | Added default-on `ENABLE_RULE_LINK`, gated dispatch/rule/conflict paths, added focused no-`LINK` phased structural synthesis config, and fixed FPGA-small numeric status ordering for disabled F24 operations. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and normal FPGA-small CSIM both passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | Focused no-`LINK` phased structural parallel CSYNTH completed in `csyn_t20_parallel_structural_phased_nolink_final`. |

## Goal

Measure whether the remaining `II=4` limiter in the phased structural parallel
scheduler is specifically caused by `LINK` substitution-memory access, rather
than by the refill/select/execute/commit scheduler itself.

## Hypothesis

Try19 made memory ownership explicit and moved the bottleneck to a narrow
critical path: `substitution_memory_0` load in `apply_lane_link_rule`, through
previous-port/unbound comparison, into status-control muxing.

If the same phased scheduler is synthesized with `LINK` disabled, the design
should no longer touch substitution memory in the structural rule subset. HLS
should then either lower the main-loop II or expose a new limiter in local queue
or node-memory handling. Either result is valuable because it separates
substitution-memory cost from general two-lane scheduler cost.

## Difference From Prior Tries

Try18 removed large allocation/numeric rules but kept `LINK`. Try19 changed the
parallel scheduler structure but still kept `LINK`.

Try20 is not another scheduler restructuring. It keeps the try19 architecture
and changes only the focused rule subset by adding a `LINK` gate for synthesis
measurement. This directly tests the bottleneck named by the try19 HLS report.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_RULE_LINK`, default `1`.
- In the parallel lane interaction path, treat `INTERACTION_RULE_LINK` as
  unsupported when `ENABLE_RULE_LINK=0`.
- Preserve default behavior:
  - full profile and normal FPGA-small builds keep `LINK` enabled;
  - default host tests remain unchanged.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nolink_nostats.cfg`
- The config should enable the try19 phased scheduler and disable:
  - `ENABLE_RULE_LINK`
  - `ENABLE_RULE_COMMUTE`
  - `ENABLE_RULE_OPERATE`
  - `ENABLE_RULE_SWITCH`
  - `ENABLE_RULE_CALL`
  - reducer and parallel statistics.
- Do not claim this profile is a complete reducer. It is a controlled
  synthesis probe for `VOID`, `ERASE`, and `ANNIHILATE` only.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Non-LINK phased structural syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Default host execution:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src test\reducer_test.cpp src\reducer_core.cpp src\reducer_core_parallel.cpp -o try20_host_test.exe
.\try20_host_test.exe
```

Focused non-LINK phased structural parallel CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nolink_nostats.cfg --work_dir csyn_t20_parallel_structural_phased_nolink
```

## Results

- Host syntax: passed for the default full-profile build.
- Non-LINK phased structural syntax: passed with `FPGA_SMALL_PROFILE`,
  `ENABLE_PARALLEL_SERVICE_PHASE=1`, `ENABLE_RULE_LINK=0`, structural rule
  gates, and statistics disabled.
- Host execution: local `try20_host_test.exe` built, but Windows Application
  Control blocked direct execution. Vitis CSIM was used for executable
  validation.
- Full CSIM: passed in `csim_t20_full_rerun` with `Overall Result: PASS`.
- FPGA-small CSIM: initially exposed a profile-status ordering bug where
  disabled F24 remainder reported invalid numeric operation before unsupported
  numeric type. After moving the disabled-F24 type check before disabled
  division/remainder checks, FPGA-small CSIM passed in `csim_t20_small_rerun`
  with `Overall Result: PASS`.
- CSYNTH: completed with
  `hls_config_synth_parallel_fpga_small_structural_phased_nolink_nostats.cfg`.

## HLS Evidence

- Work directory:
  `C:\diploma\interactReduce\csyn_t20_parallel_structural_phased_nolink_final`
- Report paths:
  - `C:\diploma\interactReduce\csyn_t20_parallel_structural_phased_nolink_final\hls\syn\report\csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t20_parallel_structural_phased_nolink_final\hls\syn\report\reducer_core_parallel_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t20_parallel_structural_phased_nolink_final\hls\syn\report\reducer_core_parallel_Pipeline_VITIS_LOOP_399_3_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t20_parallel_structural_phased_nolink_final\hls\syn\report\csynth_design_size.rpt`
- LUT: `3332`
- FF: `1448`
- DSP: `0`
- BRAM: `4`
- Main scheduler loop: `VITIS_LOOP_399_3`
- Main scheduler loop II: `3`
- Violation type: `Resource Limitation`
- Timing/slack: `-4.94`
- Estimated Fmax: `81.69 MHz`
- Design size:
  - compile/link instructions: `14719`
  - HW transform optimized instructions: `588`
- Relevant HLS warnings:
  - `substitution_memory_*` ports have no fanin/fanout, confirming the no-`LINK`
    profile does not use substitution memory;
  - book memories and allocation counters also dangle, as expected with
    `CALL`, allocation-heavy rules, and statistics disabled.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try20 succeeded as a diagnostic synthesis experiment.

Compared with try19, disabling `LINK` and its conflict helpers reduces area and
improves the phased parallel main loop from `II=4` to `II=3`:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try19 phased structural with `LINK` | 4344 | 1821 | 0 | 4 | 4 | Memory Dependency | `-2.00` | `107.48 MHz` |
| Try20 phased structural without `LINK` | 3332 | 1448 | 0 | 4 | 3 | Resource Limitation | `-4.94` | `81.69 MHz` |

This proves that `LINK` substitution-memory access is a real contributor to the
try19 bottleneck, but it is not the only limiter. Once substitution memory is
removed, the scheduler is still bound by a resource limitation around the
two-lane local active-pair queues. The remaining BRAM count is exactly the four
local queue memories: left/right port storage for two lanes.

For diploma wording:

```text
Removing LINK from the phased structural parallel profile improved II from 4
to 3 and removed substitution-memory fanout from the synthesized datapath.
The remaining limiter became a resource limitation on lane-local active-pair
queue storage. This separates the parallel reducer problem into two concrete
hardware issues: substitution-memory commit for LINK and local-queue port
ownership for multi-lane scheduling.
```

## Next Recommendation

The next try should not disable more semantics. It should target the resource
limitation visible after removing `LINK`:

- replace the per-lane local active-pair RAM with a tiny register-backed queue
  for the structural phased synthesis profile; or
- split local queue service into explicit read/write phases so HLS no longer
  tries to satisfy pop/peek/push pressure against the same local queue RAM in
  one pipelined loop.

After that local-queue experiment, return to `LINK` with an explicit
substitution read/decide/write service. Try20 shows both directions matter, but
the local queue is now the isolated non-substitution limiter.
