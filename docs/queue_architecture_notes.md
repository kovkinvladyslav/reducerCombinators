# Active-Pair Queue Architecture Notes

## Current Evidence

After try1, the FPGA-small reducer with staged `CALL` synthesizes on
`xc7a35t-cpg236-1`, but the main reducer loop remains limited by the active-pair
queue.

Try1 staged `CALL` result:

| Build | LUT | FF | DSP | Main loop II |
| --- | ---: | ---: | ---: | ---: |
| FPGA-small staged `CALL` | 7804 | 3254 | 2 | 4 |

The HLS report identifies the loop violation as a memory dependency on
`redex_memory`. The reducer pops the next active pair from the queue and, for
many rules, pushes newly produced active pairs back into the same queue memory
inside the same pipelined loop.

## Try2 Produced-Pair Buffer

Try2 adds an optional `ENABLE_PRODUCED_PAIR_BUFFER` build gate. When enabled,
`LINK`, `ERASE`, and `ANNIHILATE` emit produced active pairs into a small local
`ProducedActivePairs` buffer. The reducer then flushes one produced pair per
service tick into the existing global queue.

This keeps the interaction semantics intact:

- `performed_steps` increments only after all buffered pairs for an interaction
  have been flushed;
- queue overflow still reports `REDUCER_STATUS_REDEX_QUEUE_OVERFLOW`;
- the default full and FPGA-small configs keep the old direct path unless the
  gate is enabled.

The change is deliberately limited to small bounded rules. `COMMUTE`, `SWITCH`,
`OPERATE`, and staged `CALL` still use their existing queue paths in try2, so
this is an isolation experiment rather than a full queue-service rewrite.

## Try2 Evidence

Try2 completed FPGA-small synthesis with:

| Build | LUT | FF | DSP | Main loop II |
| --- | ---: | ---: | ---: | ---: |
| FPGA-small staged `CALL` try1 | 7804 | 3254 | 2 | 4 |
| FPGA-small produced-pair buffer try2 | 8197 | 3598 | 2 | 4 |

The local buffer did not reduce the final II. HLS still reports a memory
dependency on `redex_memory`, with remaining direct stores from staged `CALL`
and other rules in the same pipelined region. The buffer also introduced a small
local RAM for buffered pair ports.

## Interpretation

The important result is architectural: a local produced-pair buffer by itself is
not enough to solve the queue dependency while the reducer loop still contains
both queue pop and some direct queue push paths.

For the diploma, try2 is useful negative evidence. It shows that the queue issue
is not only caused by `LINK`, `ERASE`, and `ANNIHILATE`; it is a structural
property of the reducer loop and the shared `redex_memory` service.

## Try3 Single-Owner Queue Commit

Try3 avoided partial local buffering and routed all single-lane produced active
pairs through one commit service when `ENABLE_PRODUCED_PAIR_BUFFER=1`. That
includes `COMMUTE`, `SWITCH`, `OPERATE`, safe reference duplication, and staged
`CALL`.

Try3 completed CSIM and synthesis with:

| Build | LUT | FF | DSP | Main loop II | Main limiter |
| --- | ---: | ---: | ---: | ---: | --- |
| FPGA-small staged `CALL` try1 | 7804 | 3254 | 2 | 4 | `redex_memory` dependency |
| FPGA-small partial buffer try2 | 8197 | 3598 | 2 | 4 | `redex_memory` dependency |
| FPGA-small single commit try3 | 8323 | 3277 | 2 | 3 | resource/clock limitation |

The important architectural change is that the II violation changed from
`Memory Dependency` to `Resource Limitation`, and the final II improved from
`4` to `3`. This shows that single-owner queue writeback is a real scheduling
improvement, even though the implementation is not final.

The cost is timing. Try3 worsened estimated timing to about `16.195 ns` for the
10 ns target. The critical path moved into control around staged `CALL`
capacity/status handling and step completion. That means the next phase should
not keep widening the same reducer loop; it should split control into smaller
pipeline regions.

## Next Queue Architecture

The next try should split queue access into explicit phases:

```text
flush pending produced pair, if any
  else service active staged CALL, if any
  else pop one input active pair
  execute one interaction
  store produced pairs locally
```

For a stronger experiment, all rule-produced pairs should pass through one
commit service, as try3 demonstrated. The next improvement should therefore be
phase splitting, for example a small reducer control FSM with separate pop,
execute, staged-service, and commit states. That would keep the single-owner
queue write path while shortening the combinational path that now limits
timing.

## Try4 Phase-Specialized Control

Try4 kept try3's single-owner queue commit service and refactored
`reducer_core.cpp` so two service cases are handled before normal dispatch:

- pending produced-pair buffer flush,
- active staged `CALL` service.

The intention was to reduce the mux/control pressure around
`interaction_completed`, `executed_steps`, and status outputs without changing
queue ownership or interaction semantics.

Try4 passed full-profile CSIM and FPGA-small produced-buffer CSIM. FPGA-small
synthesis also completed:

| Build | LUT | FF | DSP | Main loop II | Main limiter |
| --- | ---: | ---: | ---: | ---: | --- |
| FPGA-small single commit try3 | 8323 | 3277 | 2 | 3 | resource/clock limitation |
| FPGA-small phase control try4 | 8306 | 3250 | 2 | 3 | resource/clock limitation |

The result is neutral-to-negative. Resource use moved slightly down, but final
II and timing did not improve: the report still shows `II=3`, resource
limitation, and `-8.89` slack. This means HLS still treats the reducer as one
large pipelined control region. Early `continue` branches are not an
architecture boundary from the scheduler's point of view.

## Validation Profile Policy

Full-profile CSIM remains the semantic guard for these queue experiments. It
proves that the complete interaction-combinator reducer still behaves correctly
after control-flow changes.

FPGA-small CSYNTH is the primary architecture measurement. Full-profile CSYNTH
is a reference experiment, not a requirement for every queue try, because try1
already showed full-profile synthesis is dominated by the complete numeric
subsystem: full staged/no-statistics synthesis completed at `48457 LUT`,
`32215 FF`, `49 DSP`, and `II=67`.

## Next Queue Architecture

The next queue attempt should use a phase boundary that is explicit to HLS, not
only expressed as branches inside one pipelined loop. Candidate directions:

- an explicit reducer service FSM with states for commit, staged `CALL`, pop,
  execute, and completion accounting;
- separate non-inlined helper regions for queue commit and staged `CALL`
  service;
- a two-loop or dataflow-shaped experiment where queue pop and queue push are
  no longer scheduled as peer operations in one monolithic pipeline.

The next try should be judged by whether it changes the current limiter from
the broad resource/timing path and improves either slack or final II without
undoing try3's single-owner queue writeback.

## Try5 Deferred Completion Accounting

Try5 added a one-entry pending-completion service under
`ENABLE_DEFERRED_COMPLETION_ACCOUNTING`. Instead of writing
`executed_steps` and rule statistics in the same cycle as rule execution or
staged `CALL` setup, the reducer records the completed rule kind and performs
completion accounting in the next service tick.

This is a real hardware boundary, not just reordered branches. It directly
targets the try4 critical path from staged `CALL` capacity logic into the
`executed_steps` store.

Try5 passed full-profile CSIM and FPGA-small deferred-completion CSIM. The
FPGA-small synthesis result was:

| Build | LUT | FF | DSP | Main loop II | Timing/slack | Main result |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| FPGA-small single commit try3 | 8323 | 3277 | 2 | 3 | `-8.89` | queue dependency removed, timing limited |
| FPGA-small phase control try4 | 8306 | 3250 | 2 | 3 | `-8.89` | branch reordering not enough |
| FPGA-small deferred completion try5 | 10591 | 4425 | 2 | 1 | `-1.21` | all loop constraints satisfied |

The key architectural result is that the reducer loop reached final `II=1`.
Vitis reported all loop constraints satisfied and estimated Fmax
`117.51 MHz`. The estimated clock was `8.510 ns` against a `10 ns` target with
`2.70 ns` uncertainty.

The tradeoff is area and service latency. Rule helpers are now visible as
separate RTL modules with multi-cycle latencies, for example:

- `begin_staged_call_buffered`: up to 3 cycles;
- `apply_operate_rule_buffered`: up to 5 cycles;
- `step_staged_call_buffered`: up to 2 cycles.

For the diploma, this is strong evidence that the reducer needs explicit
runtime services around queue commit, staged `CALL`, and completion accounting.
It also gives a clean throughput/area tradeoff table: try3 is smaller but
`II=3`; try5 is larger but scheduler-clean at `II=1`.

## Next Area Recovery Step

The next architecture attempt should keep try5's completion boundary and reduce
the extra LUT/FF cost. A focused experiment is selective helper partitioning:
keep expensive rules such as `CALL`, `OPERATE`, `SWITCH`, and `COMMUTE` as
separate staged/service modules, while allowing tiny rules such as `VOID`,
`LINK`, `ERASE`, and `ANNIHILATE` to inline or share a smaller local datapath.
