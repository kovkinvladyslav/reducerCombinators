# Try 53: ERASE-Only Request/Response Service

Current status: `failed_csynth_resources`

Branch: `try53`

Architecture: keep the try49 true class-only selected-command base, but move
selected `ERASE` into a narrow request/response service instead of calling the
direct helper inside the selected EXECUTE tick.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try52 proved that a noinline helper improves Fmax but creates a loop-carried call dependency and worsens scheduler II to `3`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SELECTED_ERASE_SERVICE`, ERASE-only READ/EMIT service phases, narrow request registers, and focused config `hls_config_try53_selected_erase_service_nostats.cfg`. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full-profile CSIM rerun, and FPGA-small CSIM rerun passed. The first full/small CSIM attempts hit the known Vitis `csim.exe: invalid argument` launcher issue, then passed in fresh work dirs. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with `3549 LUT`, `1260 FF`, `0 DSP`, `0 BRAM`, scheduler `II=1`, and estimated Fmax `116.18 MHz`; area improved but timing did not. |

## Goal

Test whether selected `ERASE` can be staged as a narrow hardware protocol
without reintroducing the broad generic non-link controller that failed in
try43 and try46.

## Hypothesis

Try52 shows that a module boundary is useful only when it is paired with a
real scheduler protocol boundary. A direct helper call inside the same
`II=1` scheduler loop creates call/return scheduling pressure.

Try53 keeps selected `ERASE` in the scheduler, but changes the shape from:

```text
EXECUTE -> execute_selected_erase_direct(...) -> COMMIT
```

to:

```text
EXECUTE request -> SELECTED_ERASE_READ -> SELECTED_ERASE_EMIT -> COMMIT
```

The request tick only pops the selected local active pair and captures the
erasure/node addresses. The read tick validates the node address and reads
node memory. The emit tick writes the two produced pairs. This avoids a
function-call resource inside the pipelined loop and avoids try46's generic
`ERASE`/`ANNIHILATE` non-link service.

## Difference From Prior Tries

This is not try46 repeated. Try46 staged both `ERASE` and `ANNIHILATE` through
a generic `PARALLEL_SERVICE_NON_LINK_READ` path, with rule-kind/class branches
and generic selected-non-link capture registers.

Try53 is intentionally narrower:

- selected `ERASE` only;
- true class-only selected command is required;
- selected `ANNIHILATE` stays on the existing path;
- no noinline helper call;
- no generic non-link validation pending flag;
- no full `InteractionRuleKind` decision inside the new service.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SELECTED_ERASE_SERVICE`
  - default `0`
- Add two service phases:
  - `PARALLEL_SERVICE_SELECTED_ERASE_READ`
  - `PARALLEL_SERVICE_SELECTED_ERASE_EMIT`
- Add narrow selected-ERASE request registers:
  - active flag
  - erasure port
  - node address
  - two captured auxiliary ports
- In selected EXECUTE, when `selected_command_class == SELECTED_CLASS_ERASE`:
  - pop the selected local active pair;
  - capture erasure port and node address;
  - route to `PARALLEL_SERVICE_SELECTED_ERASE_READ`.
- In `SELECTED_ERASE_READ`:
  - validate node address;
  - read node data;
  - capture the two auxiliary ports;
  - route to `SELECTED_ERASE_EMIT` on success or `COMMIT` on error.
- In `SELECTED_ERASE_EMIT`:
  - initialize selected produced-pair buffer;
  - write the two `ERASE` produced pairs;
  - record selected-command completion;
  - route to completion/commit as existing selected-command flow expects.
- Add focused config:
  - `hls_config_try53_selected_erase_service_nostats.cfg`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try53 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1 -DENABLE_PARALLEL_DIRECT_SELECTED_ERASE=1 -DENABLE_PARALLEL_SELECTED_ERASE_SERVICE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a53
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b53
```

Focused CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try53_selected_erase_service_nostats.cfg --work_dir csyn_t53_parallel_selected_erase_service
```

## Results

- Host syntax: passed for default testbench, focused top, and focused testbench syntax.
- Host execution: not run separately; covered by Vitis CSIM testbench.
- Full CSIM: first work directory `a53` hit Vitis launcher error `couldn't execute ".\csim.exe": invalid argument`; rerun in `a53_rerun` passed with `Overall Result: PASS`.
- FPGA-small CSIM: first work directory `b53` hit the same Vitis launcher error; rerun in `b53_rerun` passed with `Overall Result: PASS`.
- CSYNTH: completed in `csyn_t53_parallel_selected_erase_service`, but did not meet the timing-improvement goal.

## HLS Evidence

- Work directory: `csyn_t53_parallel_selected_erase_service`
- Report paths:
  - `csyn_t53_parallel_selected_erase_service/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t53_parallel_selected_erase_service/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1202_3_csynth.rpt`
  - `csyn_t53_parallel_selected_erase_service/hls/syn/report/csynth_design_size.rpt`
- LUT: `3549` top-level total, `3391` in the scheduler pipeline.
- FF: `1260` top-level total, `1189` in the scheduler pipeline.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1202_3` target `II=1`, achieved `II=1`, iteration latency `2`.
- Timing/slack: estimated clock `8.607 ns`, uncertainty `2.70 ns`, effective budget `7.30 ns`, estimated Fmax `116.18 MHz`; approximately `-1.307 ns` against the effective budget.
- Design size: `21,557` compile/link instructions and `609` HW-transform instructions.
- Last Vitis log line if frozen: not applicable; synthesis completed.

Critical path evidence:

```text
while.cond
executed_steps / maximum_steps loop condition
local_status_code phi/mux
local_status_code_write_ln1060
```

The relevant Vitis log lines are:

```text
Cannot meet target clock period from basic block while.cond to
local_status_code_write_ln1060 ... combination delay: 10.3146 ns

Pipelining result : Target II = 1, Final II = 1, Depth = 3, loop
VITIS_LOOP_1202_3

Loop Constraint Status: All loop constraints were satisfied.
Estimated Fmax: 116.18 MHz
```

## Conclusion

Try53 is a useful but rejected result.

The narrow ERASE-only request/response protocol avoids try52's loop-carried
helper-call dependency and restores scheduler `II=1`. It also gives the best
LUT count in this focused sequence:

| Build | LUT | FF | Main loop II | Fmax |
| --- | ---: | ---: | ---: | ---: |
| Try49 direct selected `ERASE` | 3661 | 1165 | 1 | `118.28 MHz` |
| Try52 outlined selected `ERASE` helper | 3821 | 1555 | 3 | `131.66 MHz` |
| Try53 ERASE-only service | 3549 | 1260 | 1 | `116.18 MHz` |

This means the protocol boundary is better than a noinline call for throughput
and area. However, timing is still worse than try49/try51 because the top path
has shifted to the scheduler-wide `local_status_code` phi and loop-continuation
condition. The selected `ERASE` datapath is no longer the only problem; the
shared status/control surface is now the dominant timing object.

## Next Recommendation

The next try should target the scheduler status/control surface directly.

Do not add another selected `ERASE` service phase. The better next direction is
to separate the common successful scheduler path from error/status production,
for example:

- a focused valid-graph fast path that keeps full semantic error handling in
  the default profile but removes `local_status_code` writes from the measured
  hot scheduler profile; or
- a two-register status protocol where service phases raise a narrow pending
  error bit and a separate non-pipelined exit/status block writes the final
  `status_code`.

For FPGA architecture, try53 shows that rule-local staging can control II and
area, but timing closure now requires reducing global control muxing.
