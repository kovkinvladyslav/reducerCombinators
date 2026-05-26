# Try 22: Split Parallel Service Helper Probe

Current status: `failed_csynth_resources`

Branch: `try22`

Architecture: keep the try21 no-`LINK` phased structural parallel profile, but
factor the four scheduler services into explicit helper functions for a focused
HLS scheduling experiment.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try21 completed at `3914 LUT`, `2071 FF`, `0 DSP`, `0 BRAM`, main loop `II=3`; HLS reported a resource limitation after local queue BRAMs were removed. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SERVICE_HELPERS`, four explicit phased scheduler helpers, and the focused helper-gated synthesis config. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and normal FPGA-small CSIM both passed with `Overall Result: PASS`. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed, but helper factoring worsened the focused result to `6309 LUT`, `5390 FF`, `0 DSP`, `0 BRAM`, main loop `II=6`, and slack `-3.76`. |

## Goal

Measure whether the remaining `II=3` in the no-`LINK`, register-backed,
structural parallel scheduler is caused by HLS scheduling all service phases as
one large pipelined `if/else` body.

## Hypothesis

Try21 removed the obvious local-memory port pressure but did not reduce II. The
remaining loop still contains refill, select, execute, and commit code in one
pipelined loop body. Even though only one phase is active per iteration, HLS must
schedule the resources and control paths for all branches together.

Factoring the phase bodies into explicit service helpers may give HLS a clearer
resource boundary. This can either reduce II, improve timing, or produce useful
negative evidence that the next step must be separate service loops rather than
helper factoring.

## Difference From Prior Tries

Try19 introduced service phases. Try20 removed `LINK` to isolate substitution
memory. Try21 changed local queue storage from BRAM to registers.

Try22 keeps the same rule subset and register-backed queue as try21. It changes
only the expression of the scheduler services, so it measures service granularity
rather than rule cost or queue memory format.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SERVICE_HELPERS`, default `0`.
- Under that gate, route the phased scheduler through four explicit helpers:
  - refill local queues from the global queue;
  - select candidates and run conflict checks;
  - execute accepted lane work into per-lane produced-pair buffers;
  - commit one produced pair from the selected lanes.
- Preserve the existing phased scheduler as the default implementation when the
  gate is disabled.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_helpers_nostats.cfg`
- The config should enable:
  - `FPGA_SMALL_PROFILE`
  - `ENABLE_PARALLEL_SERVICE_PHASE`
  - `ENABLE_PARALLEL_SERVICE_HELPERS`
  - `ENABLE_REGISTER_LANE_QUEUE`
- The config should disable:
  - `ENABLE_RULE_LINK`
  - `ENABLE_RULE_COMMUTE`
  - `ENABLE_RULE_OPERATE`
  - `ENABLE_RULE_SWITCH`
  - `ENABLE_RULE_CALL`
  - reducer and parallel statistics.
- Preserve normal behavior:
  - default full-profile CSIM should still pass;
  - normal FPGA-small CSIM should still pass;
  - try21 focused behavior should remain available without the new helper gate.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Service-helper phased structural syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_PARALLEL_SERVICE_HELPERS=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t22_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t22_small
```

Focused service-helper no-`LINK` phased structural parallel CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_helpers_nostats.cfg --work_dir csyn_t22_parallel_structural_phased_nolink_regqueue_helpers
```

## Results

Fill this after implementation.

- Host syntax: passed for the default full-profile build.
- Service-helper focused syntax: passed with `FPGA_SMALL_PROFILE`,
  `ENABLE_PARALLEL_SERVICE_PHASE=1`, `ENABLE_PARALLEL_SERVICE_HELPERS=1`,
  `ENABLE_REGISTER_LANE_QUEUE=1`, no-`LINK` structural rule gates, and
  statistics disabled.
- Host execution: not run as a direct local executable; Vitis CSIM was used for
  executable validation.
- Full CSIM: passed in `csim_t22_full` with `Overall Result: PASS`.
- FPGA-small CSIM: passed in `csim_t22_small` with `Overall Result: PASS`.
- CSYNTH: completed with
  `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_helpers_nostats.cfg`,
  but the result is worse than try21 and is classified as
  `failed_csynth_resources`.

## HLS Evidence

Fill this when reports or logs exist.

- Work directory:
  `C:\diploma\interactReduce\csyn_t22_parallel_structural_phased_nolink_regqueue_helpers`
- Report paths:
  - `C:\diploma\interactReduce\csyn_t22_parallel_structural_phased_nolink_regqueue_helpers\hls\syn\report\csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t22_parallel_structural_phased_nolink_regqueue_helpers\hls\syn\report\reducer_core_parallel_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t22_parallel_structural_phased_nolink_regqueue_helpers\hls\syn\report\reducer_core_parallel_Pipeline_VITIS_LOOP_593_3_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t22_parallel_structural_phased_nolink_regqueue_helpers\hls\syn\report\csynth_design_size.rpt`
- LUT: `6309`
- FF: `5390`
- DSP: `0`
- BRAM: `0`
- Main scheduler loop: `VITIS_LOOP_593_3`
- Main scheduler loop II: `6`
- Violation type: `Memory Dependency`
- Timing/slack: `-3.76`
- Estimated Fmax: `90.42 MHz`
- Design size:
  - compile/link instructions: `14855`
  - HW transform optimized instructions: `1132`
- Service helper evidence:
  - `parallel_execute_service`: latency `4`, interval `4`, memory dependency;
  - `parallel_select_service`: latency `5`, interval `1`;
  - `parallel_refill_service`: latency `1`, interval `1`;
  - `parallel_commit_service`: latency `1`, interval `1`;
  - HLS instantiated all four helpers inside
    `reducer_core_parallel_Pipeline_VITIS_LOOP_593_3`, so the parent loop
    still schedules the service calls together.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try22 failed as an optimization, but it is useful negative evidence.

Compared with try21:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try21 register local queues | 3914 | 2071 | 0 | 0 | 3 | Resource Limitation | `-0.02` | `136.57 MHz` |
| Try22 helper-factored services | 6309 | 5390 | 0 | 0 | 6 | Memory Dependency | `-3.76` | `90.42 MHz` |

The helper boundary did not give HLS a better hardware schedule. It created
explicit service instances with their own latency and handshake scheduling, and
the parent loop still had to coordinate all four services from one pipelined
body. The execute helper became the visible limiter with interval `4`, and the
parent scheduler loop settled at `II=6`.

For diploma wording:

```text
Factoring service phases into helper functions is not equivalent to creating
separate hardware services. In HLS, non-inlined helpers can become additional
scheduled instances inside the same parent pipeline, increasing control and
handshake overhead. Try22 therefore shows that the next parallel architecture
must split ownership at the loop/service-controller level, not only at the C++
function boundary.
```

## Next Recommendation

The next try should avoid non-inlined helper calls inside the same pipelined
parent loop. A more meaningful architecture is a true service-loop split:

- one pipelined loop for refill/select bookkeeping;
- one pipelined loop or state for execute;
- one pipelined loop for commit/writeback;
- or a top-level FSM without a single `PIPELINE II=1` pragma over all service
  dispatch.

The purpose should be memory/counter ownership, not C++ helper factoring.
