# Try 21: Register-Backed Lane Queue Probe

Current status: `csynth_passed`

Branch: `try21`

Architecture: reuse the try20 no-`LINK` phased structural parallel profile but
replace the lane-local BRAM queue with a tiny register-backed queue for the
focused synthesis build

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try20 completed at `3332 LUT`, `1448 FF`, `0 DSP`, `4 BRAM`, main loop `II=3`; HLS reported a resource limitation and the only remaining BRAMs were the four lane-local active-pair queue memories. |
| 2026-05-26 | `implemented` | Added `ENABLE_REGISTER_LANE_QUEUE`, narrowed focused-profile lane-local queue length to 4, partitioned local queue storage, narrowed local queue index/count types, and added a focused register-queue synthesis config. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and normal FPGA-small CSIM both passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | Focused no-`LINK` phased structural parallel CSYNTH with register-backed local queues completed in `csyn_t21_parallel_structural_phased_nolink_regqueue_final`. |

## Goal

Measure whether the residual `II=3` resource limit in the no-`LINK` phased
parallel scheduler comes from BRAM-backed lane-local active-pair queues.

## Hypothesis

Try20 removed `LINK` substitution-memory activity, but the scheduler still
could not reach `II=1`. The final report showed exactly four BRAMs remaining:
left/right port storage for two lane-local queues. The phased scheduler touches
local queues in several services: refill pushes work, select peeks work,
execute pops work, and commit pushes produced work.

If a focused profile replaces those local queues with a tiny register-backed
queue, HLS may remove the BRAM port resource limit. This may reduce II or expose
a new non-queue timing/control limiter. The profile is a measurement probe, not
a final memory design.

## Difference From Prior Tries

Try19 changed scheduler structure. Try20 changed the supported structural rule
subset to remove `LINK` and isolate substitution memory.

Try21 keeps the try20 scheduler and rule subset. It changes only the physical
implementation of lane-local queue storage in the focused synthesis profile.
That makes it a direct test of local queue port pressure rather than another
rule-deletion or scheduler-phase experiment.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_REGISTER_LANE_QUEUE`, default `0`.
- Under that gate, reduce `LANE_LOCAL_ACTIVE_PAIR_QUEUE_LENGTH` to a small
  bounded value suitable for complete array partitioning.
- Partition the lane-local queue arrays completely only when the gate is
  enabled.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_nostats.cfg`
- The config should enable:
  - `FPGA_SMALL_PROFILE`
  - `ENABLE_PARALLEL_SERVICE_PHASE`
  - `ENABLE_REGISTER_LANE_QUEUE`
- The config should disable:
  - `ENABLE_RULE_LINK`
  - `ENABLE_RULE_COMMUTE`
  - `ENABLE_RULE_OPERATE`
  - `ENABLE_RULE_SWITCH`
  - `ENABLE_RULE_CALL`
  - reducer and parallel statistics.
- Preserve normal behavior:
  - default queue length and BRAM-friendly queue storage remain unchanged when
    the gate is disabled;
  - full CSIM and normal FPGA-small CSIM must still pass.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Register-queue phased structural syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t21_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t21_small
```

Focused register-queue no-`LINK` phased structural parallel CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_nostats.cfg --work_dir csyn_t21_parallel_structural_phased_nolink_regqueue
```

## Results

- Host syntax: passed for the default full-profile build.
- Register-queue phased structural syntax: passed with `FPGA_SMALL_PROFILE`,
  `ENABLE_PARALLEL_SERVICE_PHASE=1`, `ENABLE_REGISTER_LANE_QUEUE=1`,
  `ENABLE_RULE_LINK=0`, structural rule gates, and statistics disabled.
- Host execution: not run as a direct local executable; Vitis CSIM was used for
  executable validation.
- Full CSIM: passed in `csim_t21_full` with `Overall Result: PASS`.
- FPGA-small CSIM: passed in `csim_t21_small` with `Overall Result: PASS`.
- CSYNTH: completed with
  `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_nostats.cfg`.

## HLS Evidence

- Work directory:
  `C:\diploma\interactReduce\csyn_t21_parallel_structural_phased_nolink_regqueue_final`
- Report paths:
  - `C:\diploma\interactReduce\csyn_t21_parallel_structural_phased_nolink_regqueue_final\hls\syn\report\csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t21_parallel_structural_phased_nolink_regqueue_final\hls\syn\report\reducer_core_parallel_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t21_parallel_structural_phased_nolink_regqueue_final\hls\syn\report\reducer_core_parallel_Pipeline_VITIS_LOOP_402_3_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t21_parallel_structural_phased_nolink_regqueue_final\hls\syn\report\csynth_design_size.rpt`
- LUT: `3914`
- FF: `2071`
- DSP: `0`
- BRAM: `0`
- Main scheduler loop: `VITIS_LOOP_402_3`
- Main scheduler loop II: `3`
- Violation type: `Resource Limitation`
- Timing/slack: `-0.02`
- Estimated Fmax: `136.57 MHz`
- Design size:
  - compile/link instructions: `14741`
  - HW transform optimized instructions: `635`
- HLS evidence:
  - Vitis reported `Adding 'reducer_lanes' into disaggregation list because there's array-partition pragma applied on the struct field`;
  - pipeline report lists `Memory: N/A`, confirming local queue BRAMs were removed from the focused profile;
  - the local queue head/tail operations use 2-bit indexes and 3-bit counts in the detailed expression report.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try21 succeeded as a local queue storage experiment, but it did not reduce the
main scheduler II.

Compared with try20:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try20 no-`LINK`, BRAM local queues | 3332 | 1448 | 0 | 4 | 3 | Resource Limitation | `-4.94` | `81.69 MHz` |
| Try21 no-`LINK`, register local queues | 3914 | 2071 | 0 | 0 | 3 | Resource Limitation | `-0.02` | `136.57 MHz` |

The result proves the local queues were consuming the remaining BRAM and were a
major timing burden. Removing them recovers all four BRAMs and improves
estimated Fmax by about `54.88 MHz`. However, the unchanged `II=3` means the
residual initiation interval is not caused only by BRAM port limits. It is now
more likely in the combined service-phase control path: select, execute, and
commit still share queue counters, global queue counters, candidate state, and
status-control logic in one pipelined loop body.

For diploma wording:

```text
Replacing lane-local BRAM queues with a tiny register-backed queue removed all
internal BRAM from the focused parallel structural profile and nearly closed
timing, but it did not reduce II. This shows that local queue storage format is
important for area/timing, while the remaining II limit is a scheduler resource
sharing problem rather than a pure memory-port problem.
```

## Next Recommendation

The next try should split the phased scheduler even further so only one kind of
queue/counter update occurs in a loop iteration. A useful direction is an
explicit single-service microstep profile:

- one loop/service for local refill;
- one loop/service for candidate select;
- one loop/service for lane execution;
- one loop/service for local/global produced-pair commit;
- avoid having HLS pipeline all services as one large `if/else` body.

Do not continue shrinking local queue storage as the main strategy. Try21 shows
that it helps BRAM and timing, but not II.
