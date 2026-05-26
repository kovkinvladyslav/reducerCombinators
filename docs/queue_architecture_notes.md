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

## Next Queue Architecture

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
