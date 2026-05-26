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

## Try6 Selective Small-Rule Inlining

Try6 kept try5's deferred completion boundary and tested only helper granularity.
Under `ENABLE_INLINE_SMALL_RULE_HELPERS`, the tiny buffered helpers for `LINK`,
`ERASE`, and `ANNIHILATE` are forced inline. Larger helpers remain as separate
service modules.

Try6 passed full-profile CSIM and FPGA-small CSIM. FPGA-small synthesis also
completed:

| Build | LUT | FF | DSP | Main loop II | Timing/slack | Main result |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| FPGA-small deferred completion try5 | 10591 | 4425 | 2 | 1 | `-1.21` | scheduler-clean baseline |
| FPGA-small inline small helpers try6 | 10292 | 4385 | 2 | 1 | `-1.21` | modest area recovery |

The small rule helpers no longer appear as separate modules in the synthesis
summary, while larger services such as staged `CALL`, `OPERATE`, `SWITCH`, and
`COMMUTE` remain separate. This confirms that the experiment changed helper
partitioning without changing interaction semantics.

The result is positive but limited: `299 LUT` and `40 FF` were recovered while
preserving final `II=1`, but timing did not improve. For the diploma, try6
shows that module granularity matters after the scheduler boundary is fixed, but
the main remaining cost is broader reducer state/control rather than tiny-rule
helper overhead.

## Next Queue Architecture

The next area-focused try should keep try6 as the baseline and reduce the width
or fanout of pending runtime state. Good candidates are the produced-pair buffer
state, deferred completion state, and status/result muxing around staged
services. The goal should be to preserve try5/try6's `II=1` while reducing LUT
pressure more substantially than small-helper inlining.

## Try7 Compact Completion Metadata

Try7 kept the try6 scheduler structure and added
`ENABLE_COMPACT_COMPLETION_STATE`. In no-statistics synthesis builds, the
pending completion service no longer carries a full `InteractionRuleKind`,
because the only required action is incrementing `executed_steps`. The
produced-buffer metadata is also reduced to a single `buffered_rule_is_call`
bit, which is enough to avoid completing a staged `CALL` before its transaction
finishes.

Try7 passed full-profile CSIM and FPGA-small CSIM. FPGA-small synthesis
completed:

| Build | LUT | FF | DSP | Main loop II | Timing/slack | Main result |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| FPGA-small inline small helpers try6 | 10292 | 4385 | 2 | 1 | `-1.21` | helper area baseline |
| FPGA-small compact completion try7 | 9997 | 4354 | 2 | 1 | `-1.21` | modest state-area recovery |

The result is a second successful area-recovery step: `295 LUT` and `31 FF`
were removed while preserving final `II=1`. The critical path did not change.
Vitis still reports a path from the `produced_pair_buffer_active` phi into the
pipeline `targetBlock` call, which means the remaining timing/control pressure
is around produced-buffer service selection rather than rule-kind metadata.

## Next Queue Architecture

The next try should directly target the produced-buffer service state. The
current design carries a separate active flag and a 32-bit flush index through a
wide control network. A useful next experiment is to replace that pair with a
compact produced-buffer cursor, such as a small remaining-count or service-phase
field, because the maximum number of produced active pairs per interaction is
bounded at compile time.

## Try8 Narrow Produced-Buffer Cursor

Try8 kept the try7 control structure and narrowed the produced-pair buffer count
and flush cursor from `ap_uint<32>` to a 5-bit `ProducedPairCount`. This matches
the actual buffer capacity: `MAXIMUM_PRODUCED_ACTIVE_PAIRS_PER_INTERACTION` is
`17`, so a 32-bit cursor was unnecessary hardware state.

Try8 passed full-profile CSIM and FPGA-small CSIM. FPGA-small synthesis
completed:

| Build | LUT | FF | DSP | Main loop II | Timing/slack | Main result |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| FPGA-small compact completion try7 | 9997 | 4354 | 2 | 1 | `-1.21` | compact metadata baseline |
| FPGA-small narrow buffer cursor try8 | 9626 | 4246 | 2 | 1 | `-1.21` | stronger state-width recovery |

The detailed utilization report confirms that `produced_pair_flush_index` and
`produced_active_pairs_produced_pair_count` are now 5-bit values. This is a
direct FPGA cleanup: fewer bits enter the produced-buffer comparisons, address
selection, and cursor increment logic.

The critical path is unchanged. Vitis still reports
`produced_pair_buffer_active` feeding the pipeline `targetBlock`, so try8
recovers area but does not solve the service-selection timing path.

## Next Queue Architecture

The next try should remove or restructure the separate
`produced_pair_buffer_active` phi itself. A good experiment is to encode the
produced-buffer service using the remaining count or a compact service phase,
so buffer activity is derived from one narrow state instead of maintained as a
separate 1-bit flag with many incoming rule-specific assignments.

## Try9 Derived Produced-Buffer Activity

Try9 removed the separate `produced_pair_buffer_active` flag. The reducer now
derives produced-buffer service activity from the bounded produced-pair count:
if `ProducedActivePairs::produced_pair_count` is nonzero, the reducer has
pending commit-buffer work.

This preserves the try8 architecture:

- staged `CALL` remains a bounded multi-cycle service;
- all produced active pairs still pass through the single-owner queue commit
  service;
- deferred completion accounting still increments `performed_steps` separately
  from rule execution;
- the produced-pair count and flush cursor remain 5-bit state.

Try9 full-profile CSIM passed. The FPGA-small CSIM executable was blocked by
Windows Application Control before test execution, so it is recorded as an
external validation limitation rather than a reducer mismatch. FPGA-small
synthesis completed:

| Build | LUT | FF | DSP | Main loop II | Timing/slack | Main result |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| FPGA-small narrow buffer cursor try8 | 9626 | 4246 | 2 | 1 | `-1.21` | active-buffer flag still critical |
| FPGA-small derived buffer activity try9 | 9615 | 4269 | 2 | 1 | `-1.21` | active-buffer flag removed from critical path |

The resource change is nearly neutral: `-11 LUT`, `+23 FF`. The important
evidence is the critical-path movement. Try8 reported:

```text
produced_pair_buffer_active phi -> targetBlock call
```

Try9 reports:

```text
pending_completion_active phi -> targetBlock call
```

This means the produced-buffer active flag was successfully removed as an
independent scheduler limiter. The next limiter is the pending-completion
service flag. For diploma analysis, try9 is useful because it narrows the
remaining timing problem from "produced-buffer active fanout" to the more
general reducer service-selection state.

## Next Queue Architecture

The next queue/control experiment should not reintroduce separate activity
flags. A better direction is a compact service-phase encoding that represents
pending completion, produced-pair flush, active staged `CALL`, and normal
dispatch as mutually exclusive reducer services. That would test whether HLS
can schedule one small phase field more cleanly than several independent active
bits feeding the pipeline target block.

## Try10 Compact Service Phase

Try10 implements the service-phase direction suggested by try9. Under
`ENABLE_COMPACT_SERVICE_PHASE`, the produced-buffer reducer uses one compact
phase register for the mutually exclusive runtime services:

- `REDUCER_SERVICE_COMPLETE`: increment completed interaction count;
- `REDUCER_SERVICE_FLUSH`: commit one produced active pair to `redex_memory`;
- `REDUCER_SERVICE_CALL`: advance one staged `CALL` transaction step;
- `REDUCER_SERVICE_DISPATCH`: pop and execute a normal active pair.

This does not change interaction-combinator semantics. It changes how the FPGA
controller represents the reducer's service priority. The prior implementation
used separate active predicates, which HLS reported in critical paths. The
compact phase makes that priority explicit as a small FSM-like state.

Try10 FPGA-small CSIM passed. Full CSIM was blocked by Windows Application
Control before the generated executable could run, but the compact phase is
disabled in the default full profile and full-profile syntax still passed.

FPGA-small synthesis completed:

| Build | LUT | FF | DSP | Main loop II | Estimated clock | Timing/slack | Main result |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| FPGA-small derived buffer activity try9 | 9615 | 4269 | 2 | 1 | `8.510 ns` | `-1.21` | service flag still timing-limited |
| FPGA-small compact service phase try10 | 9646 | 4356 | 2 | 1 | `7.287 ns` | `+0.01` | timing estimate clears |

Try10 adds `31 LUT` and `87 FF` over try9, but it improves estimated timing by
`1.223 ns` and reaches positive top-level slack. This is the first queue/control
try after staged `CALL` to preserve `II=1` and clear the 10 ns estimate with
the configured `2.70 ns` uncertainty.

For the diploma, this is strong evidence that the reducer should be described
as a small service controller around the interaction rules, not as one
monolithic rule-dispatch loop. The FPGA-friendly view is:

```text
complete pending interaction
  else flush one produced pair
  else service staged CALL
  else dispatch one active pair
```

The explicit phase register is the hardware boundary that makes this priority
visible to HLS.

## Next Queue Architecture

The compact service phase should be treated as the current FPGA-small control
baseline. The next attempt should try to recover the small FF increase without
returning to independent active flags. Candidates include factoring repeated
produced-buffer setup, simplifying phase-update assignments, or narrowing any
remaining service metadata that is only needed in statistics-enabled builds.
