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

## Try11 Produced-Flush Activation Helper

Try11 keeps try10's compact service phase and factors the repeated
produced-buffer activation update into a forced-inline helper:

```text
activate_produced_pair_flush
```

The helper updates the flush cursor, the compact service phase, and the buffered
producer metadata. It does not change the runtime protocol; it only changes the
source structure around a repeated control assignment pattern.

Both full-profile CSIM and FPGA-small compact-service-phase CSIM passed.
FPGA-small synthesis completed:

| Build | LUT | FF | DSP | Main loop II | Estimated clock | Timing/slack | Main result |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| FPGA-small compact service phase try10 | 9646 | 4356 | 2 | 1 | `7.287 ns` | `+0.01` | timing estimate clears |
| FPGA-small flush activation helper try11 | 9646 | 4356 | 2 | 1 | `7.287 ns` | `+0.01` | synthesis-neutral cleanup |

The HLS log confirms the helper is inlined into `reducer_core`. Since the
resource and timing numbers are identical to try10, the repeated source
assignments were already optimized into the same hardware. This is useful
negative evidence: the extra FFs introduced by try10 are not due to repeated
activation code; they are the structural cost of the explicit service-phase
controller.

## Next Queue Architecture

The next area-recovery attempt should target a different source. Good candidates
are service metadata that only matters when statistics are enabled, or a
separate full-profile synthesis checkpoint to see how the improved controller
behaves when complete numeric semantics are restored. Repeating source-level
factoring around produced-buffer activation is unlikely to change hardware.

## Try12 Full-Profile Checkpoint

Try12 is a measurement checkpoint rather than a new small-profile queue
architecture. It keeps the current compact-service architecture and restores the
full numeric profile for CSIM/CSYNTH:

```text
staged CALL
+ produced-pair buffer
+ deferred completion accounting
+ selective small-rule inlining
+ compact completion state
+ compact service phase
+ full numeric subsystem
```

This matters because tries 2 through 11 used FPGA-small synthesis to isolate
runtime structure from known expensive numeric datapaths. That was the right
measurement for queue ownership and control-path experiments, but it leaves one
milestone question open: what does the best current controller look like when
full prototype semantics are restored?

The expected result is not necessarily a small design. If full-profile CSYNTH
returns to high LUT/FF/DSP use or a worse `II`, that is still useful evidence:
the controller can be FPGA-feasible while full numeric support remains a
separate resource policy problem. If full-profile CSYNTH improves over the try1
full staged/no-statistics reference, then the service-phase work helped even
under the complete profile.

Record the result against these references:

| Build | LUT | FF | DSP | Main loop II | Meaning |
| --- | ---: | ---: | ---: | ---: | --- |
| Try1 full staged/no-stats | 48457 | 32215 | 49 | 67 | early full-profile staged `CALL` reference |
| Try10/11 FPGA-small compact service | 9646 | 4356 | 2 | 1 | current FPGA-feasible runtime-controller baseline |
| Try12 full compact service | 18061 | 9785 | 12 | 1 | current milestone full-profile checkpoint |

Try12 full-profile CSIM passed and full-profile no-statistics CSYNTH completed.
The important architectural result is that the compact service controller
survives the complete semantic profile: the main reducer pipeline reports
`II=1`, and Vitis reports all loop constraints satisfied. The resource increase
over FPGA-small is real but localized. The report attributes `9175 LUT`,
`5306 FF`, and `10 DSP` to `apply_numeric_operation`, with generated hardware
for floating-point division, multiplication, comparison, `fmod`, and integer
division/remainder.

For diploma wording, this is the clean comparison:

- The staged/compact runtime controller is synthesizable in both profiles.
- FPGA-small is still the best per-try architecture signal because it isolates
  queue, `CALL`, and reducer-control behavior.
- Full-profile synthesis is now a milestone evidence point rather than a
  dead-end: it proves complete semantics can synthesize, but numeric policy is
  the next major resource question.

## Try13 F24 Remainder Resource Gate

Try13 tests the numeric-policy direction recommended after try12. It does not
change the reducer service controller. Instead, it adds one narrow gate:

```text
ENABLE_NUMERIC_F24_REMAINDER
```

The default value preserves full-profile behavior. When explicitly set to `0`,
only F24 remainder is rejected with `REDUCER_STATUS_INVALID_NUMERIC_OPERATION`.
U24/I24 remainder, U24/I24 division, F24 add/sub/mul/div/compare, staged `CALL`,
and the compact service controller remain enabled.

Validation passed in both semantic modes:

- default full compact-service CSIM: F24 remainder works;
- no-F24-rem full compact-service CSIM: F24 remainder reports invalid operation;
- no-F24-rem full no-statistics CSYNTH: completed.

| Build | LUT | FF | DSP | Main loop II | Numeric interpretation |
| --- | ---: | ---: | ---: | ---: | --- |
| Try12 full compact service | 18061 | 9785 | 12 | 1 | full numeric subsystem, including F24 `fmod` |
| Try13 full no F24 remainder | 14017 | 7902 | 7 | 1 | same controller, F24 `fmod` removed |

The delta is `-4044 LUT`, `-1883 FF`, and `-5 DSP`. The
`apply_numeric_operation` report shows the same delta directly:

| Build | Numeric LUT | Numeric FF | Numeric DSP |
| --- | ---: | ---: | ---: |
| Try12 full compact service | 9175 | 5306 | 10 |
| Try13 full no F24 remainder | 5131 | 3423 | 5 |

This is unusually clean evidence: the top-level saving equals the saving inside
`apply_numeric_operation`, and the try13 reports contain no `generic_fmod`
module. For the diploma, this supports describing numeric operations as a
separate resource policy layered on top of the interaction-net runtime.

## Try14 Shared Integer Div/Rem Operand Selection

Try14 continues from try13 and asks a narrower full-profile synthesis question:
after removing F24 remainder, are the remaining integer divide and remainder
operators duplicated because flipped and non-flipped operations are separate
switch cases?

The implementation adds:

```text
ENABLE_SHARED_INTEGER_DIV_REM
```

The gate defaults to `0`, preserving the ordinary full-profile source path.
When enabled, U24 and I24 divide/remainder first select numerator and
denominator based on the operation tag, then execute one `/` or `%` expression
per signedness and operation. This is a hardware-shaping change only: it does
not alter the numeric result, the division-by-zero status, or interaction-net
semantics.

Validation:

- try14 full-profile CSIM passed with the short Vitis work directory
  `csim_t14`;
- try14 full-profile no-statistics CSYNTH completed with `csyn_t14`;
- longer CSIM work directory names triggered a Vitis `.\csim.exe` launch
  `invalid argument` issue after compilation, so short work directories are
  preferred for repeatable Windows runs.

Top-level comparison:

| Build | LUT | FF | DSP | Main loop II | Interpretation |
| --- | ---: | ---: | ---: | ---: | --- |
| Try13 full no F24 remainder | 14017 | 7902 | 7 | 1 | F24 `fmod` removed |
| Try14 shared integer div/rem | 13333 | 6965 | 9 | 1 | integer div/rem duplicated modules removed |

Numeric-helper comparison:

| Build | Numeric LUT | Numeric FF | Numeric DSP |
| --- | ---: | ---: | ---: |
| Try13 full no F24 remainder | 5131 | 3423 | 5 |
| Try14 shared integer div/rem | 4562 | 2458 | 7 |

The try14 report contains one each of:

- `sdiv_24ns_24ns_24_28_seq_1`;
- `srem_24ns_24ns_24_28_seq_1`;
- `udiv_24ns_24ns_24_28_seq_1`;
- `urem_24ns_24ns_24_28_seq_1`.

The result is positive for LUT/FF and neutral for scheduling: main-loop `II=1`
is preserved. The tradeoff is DSP. Try14 increases top-level DSP from `7` to
`9`, not because integer division uses DSPs, but because HLS chooses a
full-DSP F24 add/sub implementation in this binding. For diploma discussion,
try14 should be presented as a resource-policy tradeoff rather than a universal
improvement:

- choose try13 when DSP is the tightest resource;
- choose try14 when LUT/FF pressure is more important and the extra two DSPs
  are acceptable.

## Try15 Fabric-Bound F24 Add/Sub With Shared Integer Div/Rem

Try15 follows directly from the try14 tradeoff. It keeps the same semantic
numeric subset as try14:

```text
ENABLE_NUMERIC_F24_REMAINDER=0
ENABLE_SHARED_INTEGER_DIV_REM=1
```

Then it adds an HLS binding policy:

```text
ENABLE_F24_ADD_SUB_FABRIC_BIND=1
```

Under this gate, `apply_numeric_operation` applies `BIND_OP` pragmas to the
F24 result variable so F24 add/sub is implemented in fabric. The purpose is not
to remove F24 add/sub from the full profile. The purpose is to decide where its
hardware cost should live.

Validation:

- host syntax passed;
- host executable passed all tests with `Overall Result: PASS`;
- Vitis CSIM compiled `csim.exe`, but the local Windows environment blocked or
  failed executable launch, so this is recorded as a tooling/policy issue;
- full-profile no-statistics CSYNTH completed with `csyn_t15`.

Top-level comparison:

| Build | LUT | FF | DSP | Main loop II | Interpretation |
| --- | ---: | ---: | ---: | ---: | --- |
| Try13 full no F24 remainder | 14017 | 7902 | 7 | 1 | no F24 `fmod`, duplicated integer div/rem |
| Try14 shared integer div/rem | 13333 | 6965 | 9 | 1 | integer div/rem shared, F24 add/sub uses DSP |
| Try15 shared int + F24 add/sub fabric | 13484 | 6900 | 7 | 1 | integer div/rem shared, F24 add/sub uses fabric |

Numeric-helper comparison:

| Build | Numeric LUT | Numeric FF | Numeric DSP |
| --- | ---: | ---: | ---: |
| Try13 full no F24 remainder | 5131 | 3423 | 5 |
| Try14 shared integer div/rem | 4562 | 2458 | 7 |
| Try15 shared int + F24 add/sub fabric | 4713 | 2393 | 5 |

The try15 report confirms:

- `faddfsub_32ns_32ns_32_6_no_dsp_1` is generated for
  `apply_numeric_operation`;
- the `BIND_OP` pragmas are listed in the report;
- integer div/rem sharing remains active with one signed divider, one signed
  remainder unit, one unsigned divider, and one unsigned remainder unit;
- main-loop `II=1` is preserved.

This is the best current full-profile no-F24-rem synthesis reference when DSP
must not increase over try13. It also gives a useful diploma lesson: full
numeric support is not a binary "enabled versus disabled" question. HLS binding
policy can move the cost of complete operations between DSP and LUT/FF while
preserving interaction-combinator semantics.

## Try16 Parallel Bank-Conflict Instrumentation

Try16 returns from numeric-resource policy to memory architecture preparation.
It does not implement banking yet. Instead, it adds measurement hooks that are
needed before a banking design can be justified.

The implementation adds explicit bank constants:

```text
SUBSTITUTION_MEMORY_BANK_COUNT = 4
NODE_MEMORY_BANK_COUNT = 2
```

The substitution value matches the existing HLS cyclic partition factor on
`substitution_memory`. The node value matches the planned low-address-bit
two-bank node-memory experiment. Two statistics counters were added:

```text
substitution_bank_conflict_count
node_bank_conflict_count
```

These counters are deliberately non-stalling. They record when two lanes touch
different variables or nodes in the same future bank, but they do not change
the current conflict decision. This distinction matters:

- exact same variable remains a true substitution conflict and stalls;
- exact same binary node remains a true node-access conflict and stalls;
- same bank but different address is only a future memory-bank pressure signal.

Host validation passed with `Overall Result: PASS`. The parallel scenario table
now includes `SubBk` and `NodeBk` columns. The targeted tests show:

| Scenario | Dual commit | True conflict | Bank metric |
| --- | ---: | ---: | ---: |
| Same substitution bank, different variables | yes | `SubstC = 0` | `SubBk = 1` |
| Same node bank, different node addresses | yes | `NodeC = 0` | `NodeBk = 1` |

Vitis CSIM generated `csim.exe` but failed to launch it in the local Windows
environment with `couldn't execute ".\csim.exe": invalid argument`, matching
earlier local launcher failures.

Two synthesis probes were attempted:

| Build | Result | Evidence |
| --- | --- | --- |
| Default full `hls_config_parallel.cfg` | timed out after 20 minutes | reached design-size reporting; no full `csynth.rpt` |
| Focused FPGA-small no-`CALL` bank-metric config | completed | `39085 LUT`, `51611 FF`, `2 DSP`, `4 BRAM`, `II=12`, `-9.93` slack |

The focused result is intentionally not presented as a feasible final parallel
runtime. It is evidence that the current experimental parallel top remains too
wide: the statistics port alone is `1152` bits, the main loop still has a
`Memory Dependency` II violation, and the top exceeds the target Artix-7 LUT/FF
budget. The useful outcome is architectural visibility. Future memory-banking
tries can now report whether workloads are blocked by true graph dependencies
or merely by same-bank memory pressure.

For diploma wording, try16 supports this statement:

```text
The parallel reducer is currently a measurement platform. It exposes
dual-commit opportunities, true conflicts, and potential bank conflicts, but
the synthesis report shows that a practical multi-lane runtime still requires
smaller control/statistics state and explicit memory arbitration.
```

## Try17 Parallel No-Statistics Checkpoint

Try17 answers the first follow-up question from try16: how much of the
experimental parallel reducer's cost comes from the statistics surface, and how
much remains in the scheduler/datapath itself?

The implementation adds only a focused synthesis config:

```text
hls_config_synth_parallel_fpga_small_nostats_nocall.cfg
```

It uses the same `reducer_core_parallel` top and the same FPGA-small/no-`CALL`
subset as try16, but disables:

```text
ENABLE_REDUCER_STATISTICS
ENABLE_PARALLEL_REDUCER_STATISTICS
```

Host validation still passed with `Overall Result: PASS`. The focused
no-statistics parallel CSYNTH completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Try16 bank metrics, statistics enabled | 39085 | 51611 | 2 | 4 | 12 | `-9.93` |
| Try17 no statistics | 11111 | 5759 | 2 | 4 | 12 | `-6.14` |

This is strong evidence that the statistics update/output network dominates
the resource explosion in the measurement build. Removing it cuts about
`27974 LUT` and `45852 FF` from the focused parallel top.

The result is still not a practical parallel runtime. The main scheduler loop
remains `II=12` with a `Memory Dependency` violation, and timing remains
negative. The report identifies a critical path that starts with a local active
pair queue read, flows through interaction-rule dispatch, and reaches a
substitution-memory write path. That means the current two-lane loop still
combines too much work in one HLS pipeline:

- local queue read/peek,
- candidate rule dispatch,
- conflict checking,
- lane rule execution,
- produced-pair enqueue,
- global/local memory updates.

For diploma wording, try17 refines try16 rather than replacing it:

```text
Statistics made the parallel measurement top much larger, but removing
statistics did not fix the initiation interval. The remaining blocker is the
monolithic two-lane scheduling and memory-access pipeline.
```

The next parallel experiment should therefore target datapath structure, not
only observation. Two useful directions are:

- a structural parallel-core profile containing only `VOID`, `LINK`, `ERASE`,
  and `ANNIHILATE`, to isolate the cost of `COMMUTE`, `SWITCH`, and `OPERATE`;
- an explicit parallel service-phase controller that separates local-queue pop,
  conflict decision, lane execution, and commit, mirroring the successful
  single-lane service-phase architecture.

## Try18 Structural Parallel-Core Probe

Try18 follows the first direction from try17. It adds an explicit
`ENABLE_RULE_COMMUTE` gate, matching the existing `CALL`, `OPERATE`, and
`SWITCH` gates, and then synthesizes the parallel top with only the small
structural rules enabled:

```text
VOID
LINK
ERASE
ANNIHILATE
```

The focused config is:

```text
hls_config_synth_parallel_fpga_small_structural_nostats.cfg
```

It disables:

```text
ENABLE_RULE_COMMUTE
ENABLE_RULE_OPERATE
ENABLE_RULE_SWITCH
ENABLE_RULE_CALL
ENABLE_REDUCER_STATISTICS
ENABLE_PARALLEL_REDUCER_STATISTICS
```

Default host validation still passed with `Overall Result: PASS`, so the new
`COMMUTE` gate preserves existing behavior when left at its default value.

The structural parallel CSYNTH completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Try17 no statistics, full FPGA-small/no-`CALL` rules | 11111 | 5759 | 2 | 4 | 12 | `-6.14` |
| Try18 structural small rules only | 4631 | 2873 | 0 | 4 | 8 | `-7.79` |

The report confirms that allocation-heavy rule bodies were removed from the
pipeline summary: `apply_lane_commute_rule`, `apply_lane_operate_rule`,
`apply_lane_switch_rule`, and `apply_numeric_operation` no longer appear in the
inlined pipeline helper list. `allocated_node_count` and
`allocated_variable_count` are marked unused because the remaining structural
rules do not allocate fresh nodes or variables.

This gives a clean architectural split:

- rule-body reduction is effective for area and improves II from `12` to `8`;
- the remaining limiter is still a `Memory Dependency`;
- therefore the parallel bottleneck is not only numeric/allocation rule logic.

Even the structural top still combines local queue access, candidate dispatch,
conflict checking, lane execution, produced-pair enqueue, and memory updates in
one pipelined scheduler loop. For the diploma, try18 is evidence that
multi-lane scaling requires explicit service phases or memory arbitration,
rather than only reducing the supported rule subset.

The next parallel try should use a service-phase split:

```text
refill local queues
select and conflict-check candidates
execute accepted lane work
commit produced pairs
```

This mirrors the successful single-lane compact service-phase architecture and
would make memory ownership clearer to HLS.

## Try19 Structural Parallel Service-Phase Scheduler

Try19 follows the service-phase direction from try18. It adds a gated
`ENABLE_PARALLEL_SERVICE_PHASE` path for the structural parallel profile. The
normal parallel top remains unchanged unless the gate is enabled.

The phased path keeps the same small structural rule subset as try18, but
separates the scheduler into explicit services:

```text
REFILL   - move work from the global queue into empty lane-local queues
SELECT   - peek local queues, classify rules, and perform conflict checks
EXECUTE  - pop selected lane work and execute rules into per-lane buffers
COMMIT   - push one produced active pair back to local/global queues
```

The focused config is:

```text
hls_config_synth_parallel_fpga_small_structural_phased_nostats.cfg
```

Default host validation still passed with `Overall Result: PASS`; the new path
is isolated behind compile-time gates. The final phased synthesis used
`csyn_t19_parallel_structural_phased_rerun` after a small completion-condition
fix that lets the `COMMIT` phase finish produced-pair writeback even when the
step budget has just been reached.

Synthesis comparison:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try18 structural monolithic scheduler | 4631 | 2873 | 0 | 4 | 8 | `-7.79` | `66.26 MHz` |
| Try19 structural phased scheduler | 4344 | 1821 | 0 | 4 | 4 | `-2.00` | `107.48 MHz` |

This is the strongest parallel-control result so far. The explicit service
controller halves the structural parallel II and improves timing substantially.
It also reduces FFs, even though HLS now infers small local RAMs for per-lane
produced-pair buffers.

The result is not yet a practical final parallel reducer. HLS still reports a
`Memory Dependency`, now in a narrower place: the critical path begins at a
`substitution_memory_0` load in the `LINK` rule, passes through the previous
port comparison, and ends in `local_status_code` mux/store logic. This is much
more actionable than the earlier monolithic queue/rule/commit path.

For diploma wording:

```text
Parallelism was not improved by simply removing large rules. It improved when
the scheduler was expressed as explicit hardware services. The remaining
limiter is a specific substitution-memory dependency in structural LINK, which
points toward a future substitution-write service or banked arbitration.
```

The next experiment should target substitution memory rather than general
scheduler structure. Two focused options are:

- split `LINK` into read/decide/write services;
- synthesize a non-`LINK` structural parallel profile to quantify the cost of
  substitution memory separately from node-memory structural rules.

## Try20 Non-LINK Structural Parallel Probe

Try20 follows the second recommendation from try19. It keeps the phased
parallel scheduler but adds a default-on `ENABLE_RULE_LINK` gate so the focused
synthesis profile can disable `LINK` without changing normal reducer behavior.

The focused config is:

```text
hls_config_synth_parallel_fpga_small_structural_phased_nolink_nostats.cfg
```

It enables:

```text
ENABLE_PARALLEL_SERVICE_PHASE
```

and disables:

```text
ENABLE_RULE_LINK
ENABLE_RULE_COMMUTE
ENABLE_RULE_OPERATE
ENABLE_RULE_SWITCH
ENABLE_RULE_CALL
ENABLE_REDUCER_STATISTICS
ENABLE_PARALLEL_REDUCER_STATISTICS
```

The default full-profile CSIM and normal FPGA-small CSIM both passed after a
small profile-status fix in `apply_numeric_operation`: disabled F24 types are
now reported as unsupported numeric type before disabled division/remainder
operation checks. This keeps profile behavior clear for the diploma table.

Synthesis comparison:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try19 phased structural with `LINK` | 4344 | 1821 | 0 | 4 | 4 | Memory Dependency | `-2.00` | `107.48 MHz` |
| Try20 phased structural without `LINK` | 3332 | 1448 | 0 | 4 | 3 | Resource Limitation | `-4.94` | `81.69 MHz` |

The first no-`LINK` synthesis still carried dead `LINK` conflict-helper logic,
so the final measurement also guards those conflict paths with
`ENABLE_RULE_LINK`. The final report still keeps the four local lane-queue BRAMs
and reports a resource limitation at `II=3`.

This gives a useful architectural decomposition:

- `LINK` substitution-memory access is a real part of the try19 limiter;
- removing `LINK` improves II from `4` to `3` and removes substitution-memory
  fanout from the active datapath;
- the remaining non-substitution limiter is local active-pair queue port
  pressure, represented by the four lane-local queue memories.

For diploma wording:

```text
Try20 separates substitution-memory effects from general scheduler effects.
The result shows that a parallel reducer cannot be optimized only by fixing
LINK; lane-local queue ownership also needs explicit hardware treatment.
```

The next experiment should focus on local queue storage, either by using a tiny
register-backed queue in the focused structural profile or by splitting local
queue read/write service phases.

## Try21 Register-Backed Lane Queue Probe

Try21 follows the local-queue recommendation from try20. It adds a focused
`ENABLE_REGISTER_LANE_QUEUE` gate. When enabled, the lane-local active-pair
queue length is reduced from `32` to `4`, the local queue array is completely
partitioned, and the local queue head/tail/count fields are narrowed to 2-bit
indexes and a 3-bit count. Default builds keep the original 32-entry queue.

The focused config is:

```text
hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_nostats.cfg
```

It keeps the try20 no-`LINK` rule subset and adds:

```text
ENABLE_REGISTER_LANE_QUEUE
```

Full-profile CSIM and normal FPGA-small CSIM both passed. The focused synthesis
completed with:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try20 no-`LINK`, BRAM local queues | 3332 | 1448 | 0 | 4 | 3 | Resource Limitation | `-4.94` | `81.69 MHz` |
| Try21 no-`LINK`, register local queues | 3914 | 2071 | 0 | 0 | 3 | Resource Limitation | `-0.02` | `136.57 MHz` |

HLS evidence confirms the queue storage change:

- Vitis disaggregated `reducer_lanes` because of the struct-field partition;
- the pipeline report lists `Memory: N/A`;
- the remaining BRAM count is `0`;
- local queue index operations use 2-bit indexes and count operations use a
  3-bit count in the detailed expression table.

Interpretation:

- local queue BRAMs were the source of the remaining BRAM use and a major
  timing burden;
- replacing them with registers nearly closes timing and removes all internal
  BRAM from the focused profile;
- the unchanged `II=3` means the remaining initiation interval is not pure
  local-memory port pressure. It is likely service/control resource sharing
  inside the large phased scheduler loop.

For diploma wording:

```text
Try21 shows a classic FPGA tradeoff: replacing small RAM queues with registers
improves timing and removes BRAM pressure, but increases LUT/FF area and does
not by itself reduce scheduler II. The next architectural issue is service
granularity, not queue storage alone.
```

The next experiment should split scheduler services more explicitly rather than
continue shrinking the local queue.

## Try22 Helper-Factored Parallel Services

Try22 tests whether the remaining try21 `II=3` can be reduced by factoring the
phased scheduler branches into explicit C++ service helpers:

```text
parallel_refill_service
parallel_select_service
parallel_execute_service
parallel_commit_service
```

The focused profile keeps the same rule subset as try21:

```text
VOID
ERASE
ANNIHILATE
```

and keeps the same register-backed lane-local queues. The only intended
architectural difference is the C++ expression of the service bodies. This makes
try22 a clean test of whether helper boundaries are enough for HLS to schedule
the services independently.

Focused synthesis completed, but the result was worse:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try21 register local queues | 3914 | 2071 | 0 | 0 | 3 | Resource Limitation | `-0.02` | `136.57 MHz` |
| Try22 helper-factored services | 6309 | 5390 | 0 | 0 | 6 | Memory Dependency | `-3.76` | `90.42 MHz` |

The detailed HLS report shows why this did not help. HLS instantiated the four
helpers inside the same parent pipeline:

| Helper | Latency | Interval | Notes |
| --- | ---: | ---: | --- |
| `parallel_refill_service` | 1 | 1 | local/global queue refill |
| `parallel_select_service` | 5 | 1 | candidate selection and conflict checks |
| `parallel_execute_service` | 4 | 4 | visible memory-dependency limiter |
| `parallel_commit_service` | 1 | 1 | produced-pair writeback |

This is useful negative evidence for the diploma. A C++ function boundary is
not the same as a separate hardware service. In this case it added helper
instances and parent-loop coordination overhead while keeping all service
control under the same pipelined scheduler loop.

The next parallel queue/control architecture should use a true service-loop
split or a top-level FSM in which each service owns a narrower loop body. The
goal should be explicit memory and counter ownership, not merely non-inlined
helper functions.

## Try23 Unpipelined Parent Dispatch

Try23 follows the try22 conclusion by changing the scheduler constraint instead
of the C++ helper structure. The focused build keeps the try21 no-`LINK`
structural rule subset and register-backed local queues, but omits the parent
`PIPELINE II=1` pragma on the phased scheduler loop:

```text
ENABLE_PARALLEL_UNPIPELINED_SERVICE_DISPATCH=1
```

Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try21 parent pipeline, register queues | 3914 | 2071 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try22 helper-factored services | 6309 | 5390 | 0 | 0 | 6 | `-3.76` | `90.42 MHz` |
| Try23 unpipelined parent dispatch | 4124 | 1863 | 0 | 0 | n/a | `-2.19` | `105.35 MHz` |

Vitis reported:

```text
Loop Constraint Status: All loop constraints were satisfied.
```

The parent scheduler loop is no longer pipelined, so the report does not provide
a meaningful active-pair issue interval. Its iteration latency is reported as
`2 ~ 89` cycles. This makes try23 a diagnostic result rather than a final
throughput architecture.

The useful evidence is the contrast with try22. Removing the parent pipeline
constraint avoids helper-instance overhead and keeps area close to try21. That
supports the conclusion that the try21 `II=3` was partly caused by forcing all
service branches into one parent pipeline schedule.

The remaining long path appears in conflict checking:

```text
lane_conflicts_with_committed_work
```

with reported latency up to `41` cycles and timing slack `-0.92`. For the fixed
two-lane reducer, the next useful architecture is therefore not another generic
service-dispatch probe. It should simplify the select/conflict service itself,
for example by replacing the generic committed-work loop with a direct two-lane
pairwise comparator.

## Try24 Fixed Two-Lane Pairwise Select

Try24 follows the conflict-check recommendation from try23. The focused profile
keeps the try21 parent pipeline, no-`LINK` structural rule subset, and
register-backed local queues, but enables:

```text
ENABLE_PARALLEL_PAIRWISE_SELECT=1
```

Instead of using the generic committed-work loop in SELECT, lane 0 is accepted
first when eligible, and lane 1 is accepted only when it does not conflict with
lane 0. The pairwise conflict check preserves the same conservative cases:

- unsupported rules conflict;
- `VOID`/`VOID` does not conflict;
- same binary node conflicts;
- allocation contention conflicts;
- bank conflicts remain metrics only when statistics are enabled.

Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try21 generic select/conflict | 3914 | 2071 | 0 | 0 | 3 | Resource Limitation | `-0.02` | `136.57 MHz` |
| Try24 fixed pairwise select | 3913 | 2070 | 0 | 0 | 3 | Resource Limitation | `-0.02` | `136.57 MHz` |

The result is almost identical to try21. That means the long conflict-check path
seen in try23 was mainly exposed by removing the parent pipeline; it is not the
active limiter in the real pipelined scheduler. In the pipelined design, the
remaining `II=3` comes from combined service-state updates rather than the
generic committed-work loop alone.

For diploma wording:

```text
Specializing the SELECT stage for exactly two lanes did not materially change
the synthesized pipelined scheduler. This narrows the remaining bottleneck to
service-state ownership: queue counters, produced-pair buffers, phase control,
and status updates still share one parent pipeline.
```

The next parallel experiment should therefore split writeback ownership, for
example by making commit/writeback the only service that mutates local/global
queue counters.

## Try25 VOID-Only Commit-Writeback Isolation

Try25 tests the try24 conclusion by removing produced-pair rules from the
focused synthesis profile. It adds default-on gates for:

```text
ENABLE_RULE_ERASE
ENABLE_RULE_ANNIHILATE
```

Normal full-profile and FPGA-small CSIM keep both gates enabled and passed with
`Overall Result: PASS`. The focused synthesis config disables every structural
rule except `VOID`, while keeping the try21/try24 phased scheduler and
register-backed lane queues:

```text
hls_config_synth_parallel_fpga_small_structural_phased_voidonly_regqueue_nostats.cfg
```

Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try24 fixed pairwise select | 3913 | 2070 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try25 `VOID` only | 2730 | 1469 | 0 | 0 | 1 | `-9.15` | `60.78 MHz` |

This is useful but not a new baseline. The positive result is that HLS reaches
`II=1` once `ERASE` and `ANNIHILATE` are removed from the focused profile,
which confirms that produced-pair/rule writeback contributes to the try21/try24
`II=3` limiter. The negative result is that timing becomes much worse even in
the tiny `VOID`-only subset. The remaining path is broad scheduler control:
global queue counters, local queue counters, selected-lane state, rule-kind
classification, and status/step updates still meet in one parent pipeline.

For diploma wording:

```text
Try25 shows that produced-pair writeback is part of the parallel scheduler's
initiation-interval limit, but simply removing producer rules is not a hardware
architecture. The scheduler still needs explicit registered boundaries between
selection, execution, and commit/writeback.
```

The next parallel try should keep the structural rule subset and add a real
selected-lane command/writeback boundary. SELECT should produce a small command
register, EXECUTE should consume that command, and COMMIT should be the only
phase that mutates local/global queue counters.

## Try26 Single Selected-Lane Command Register

Try26 follows the try25 recommendation with a gated selected-command path:

```text
ENABLE_PARALLEL_SELECTED_COMMAND
```

The focused profile keeps the try21/try24 no-`LINK` structural subset
(`VOID`, `ERASE`, `ANNIHILATE`) and register-backed lane queues, but SELECT now
writes one selected-lane command instead of carrying two-lane candidate,
commit, and produced-pair state into EXECUTE/COMMIT. EXECUTE consumes that
command, and COMMIT flushes that command's produced pairs.

This is intentionally diagnostic. It sacrifices dual issue in order to measure
whether registered command/writeback ownership is the correct hardware
direction.

The first synthesis exposed a tiny local memory issue: the selected command's
produced-pair buffer was inferred as a 17-entry RAM and the loop reported a
`Memory Dependency`. Partitioning that selected buffer removed the local memory
and changed the remaining limiter to `Resource Limitation`.

Final focused synthesis:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try21 register local queues | 3914 | 2071 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try24 fixed pairwise select | 3913 | 2070 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try25 `VOID` only | 2730 | 1469 | 0 | 0 | 1 | `-9.15` | `60.78 MHz` |
| Try26 single selected command | 3170 | 1503 | 0 | 0 | 2 | `+0.12` | `139.31 MHz` |

Interpretation:

```text
Registered command ownership is more useful than further conflict-check
specialization. It reduces the focused parallel scheduler's II from 3 to 2 and
keeps timing positive, but the one-command version is not the final parallel
architecture because it cannot demonstrate dual-lane throughput.
```

The next experiment should build a two-entry command buffer. SELECT should fill
up to two independent commands after conflict checks, while COMMIT remains the
only owner of local/global queue counters and produced-pair cursors.

## Try27 Two-Entry Selected Command Buffer

Try27 follows the try26 recommendation with:

```text
ENABLE_PARALLEL_TWO_COMMAND_BUFFER
```

The focused profile keeps the same no-`LINK` structural subset as try26:

```text
VOID
ERASE
ANNIHILATE
```

SELECT can now capture up to two non-conflicting commands. EXECUTE consumes both
valid command slots in the execute phase, and COMMIT serializes produced-pair
flushes from command 0 and then command 1. Both command produced-pair buffers
are partitioned so HLS does not infer the small local RAM that appeared in the
first try26 measurement.

Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try21 register local queues | 3914 | 2071 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try26 single selected command | 3170 | 1503 | 0 | 0 | 2 | `+0.12` | `139.31 MHz` |
| Try27 two-command buffer | 5700 | 1924 | 0 | 0 | 2 | `-2.76` | `99.36 MHz` |

This is useful negative evidence. The two-command architecture keeps the
registered ownership model and remains synthesizable, but duplicating
classification, conflict, execution, and commit selection for two command slots
costs too much LUT and timing. The loop stays at `II=2`, so the extra hardware
does not buy a better issue interval in the focused synthesis result.

For diploma wording:

```text
Try27 shows that dual-lane command capture is not enough by itself. The
parallel reducer needs registered command ownership, but same-phase duplicated
execution/commit datapaths are too expensive on the target Artix-7 device.
```

The next parallel attempt should preserve try26's single-command execute/commit
datapath and add a waiting command slot or tiny issue FIFO. That would let
SELECT observe two independent lanes without forcing HLS to duplicate the rule
execution and produced-pair commit bodies in the same scheduler phase.

## Try28 Waiting Command Slot

Try28 follows the try27 conclusion with:

```text
ENABLE_PARALLEL_WAITING_COMMAND
```

The architecture keeps only one active execute/commit command. SELECT may also
capture a second independent lane as waiting metadata, but that waiting command
is not executed until the active command is fully committed and promoted.

This directly tests whether the try27 problem was only duplicated datapaths.
The design-size report confirms the intended structural change:

- `execute_lane_active_pair_without_queue_commit` appears once, not twice;
- `commit_one_phased_produced_pair` appears once, not twice;
- only the active command owns a partitioned produced-pair buffer.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try26 single selected command | 3170 | 1503 | 0 | 0 | 2 | `+0.12` | `139.31 MHz` |
| Try27 two-command buffer | 5700 | 1924 | 0 | 0 | 2 | `-2.76` | `99.36 MHz` |
| Try28 waiting command slot | 3790 | 1787 | 0 | 0 | 2 | `-5.73` | `76.74 MHz` |

This is another useful negative result. Try28 fixes try27's obvious duplicated
execute/commit datapath cost, but the command-control path becomes worse:
active validity, execution state, waiting validity, promotion, and phase
selection all feed the same parent scheduler pipeline. The area lands between
try26 and try27, but timing is the worst of the three.

For diploma wording:

```text
Try27 and try28 separate two scaling costs. Duplicating lane command datapaths
is too expensive, and adding waiting-command control inside the same hot
scheduler pipeline is also timing-hostile. The measured baseline for continued
parallel work is therefore try26's single selected-command boundary.
```

The next parallel step should not keep adding command state inside the same
pipeline. Better options are to return to try26 and either reduce SELECT/control
logic, or reintroduce `LINK` with a staged substitution service while preserving
the single-command execute/commit boundary.

## Try29 Selected Command With LINK Restored

Try29 follows the second recommendation from try28. It returns to the try26
single selected-command boundary and restores `LINK` in the focused structural
parallel synthesis profile. Allocation-heavy and numeric/control rules remain
disabled, so the measured subset is:

```text
VOID
LINK
ERASE
ANNIHILATE
```

The focused config is:

```text
hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_nostats.cfg
```

Full-profile CSIM and normal FPGA-small CSIM both passed. Focused synthesis
completed, but failed timing:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try19 phased structural with `LINK` | 4344 | 1821 | 0 | 4 | 4 | Memory Dependency | `-2.00` | `107.48 MHz` |
| Try26 selected command without `LINK` | 3170 | 1503 | 0 | 0 | 2 | Resource Limitation | `+0.12` | `139.31 MHz` |
| Try29 selected command with `LINK` | 3558 | 1422 | 0 | 0 | 2 | Memory Dependency | `-2.00` | `107.48 MHz` |

The result is useful because it isolates the remaining structural-rule
bottleneck. The try26 selected-command boundary is still valuable: with `LINK`
restored, it keeps the scheduler at `II=2` and avoids local queue BRAM. However,
the timing failure returns to the same shape as try19. Vitis reports a critical
path that starts at the `substitution_memory_0` load in `LINK`, passes through
the `previous_port` sparse mux and comparison in `lane_interaction.hpp`, and
ends in `local_status_code` mux/store logic.

For diploma wording:

```text
Try29 shows that restoring the structural LINK rule on the best selected-command
scheduler does not fail because of generic queue storage. It fails because LINK
combines substitution-memory access, previous-port validation, and status
selection inside the execute phase. This makes LINK the next rule that needs a
hardware service boundary.
```

The next parallel experiment should not add more command slots or waiting-state
control. It should stage `LINK` itself while preserving the try26 selected
command/writeback boundary. A focused staged-`LINK` try should split
substitution handling into read, decide, and write service ticks, then measure
whether the focused structural profile can keep `LINK` enabled with positive
timing.

## Try30 Staged LINK For Selected Command

Try30 implements the staged-`LINK` recommendation from try29 under:

```text
ENABLE_PARALLEL_STAGED_LINK
```

The gate is default-off. In the focused selected-command profile, a selected
`LINK` no longer calls the complete inline `apply_lane_link_rule`. Instead, it
uses small service ticks:

```text
begin LINK and pop local active pair
read previous substitution binding
decide/write substitution result and optionally emit produced pair
commit produced pairs through the existing selected-command commit service
```

The first staged version still let the generic non-`LINK` execute helper carry
`apply_lane_link_rule`. The corrected version added a focused
`execute_selected_non_link_active_pair_without_queue_commit` helper so the
selected-command non-`LINK` path does not reintroduce the old `LINK` body.

Full-profile CSIM and normal FPGA-small CSIM both passed. Corrected focused
synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try26 selected command without `LINK` | 3170 | 1503 | 0 | 0 | 2 | `+0.12` | `139.31 MHz` |
| Try29 selected command with inline `LINK` | 3558 | 1422 | 0 | 0 | 2 | `-2.00` | `107.48 MHz` |
| Try30 selected command with staged `LINK` | 4770 | 2677 | 0 | 0 | 1 | `-7.29` | `68.57 MHz` |

This is a useful mixed result. The positive evidence is that staging `LINK`
removes the substitution-memory dependency from the reported limiter and
reaches scheduler `II=1`. The negative evidence is that timing becomes much
worse, and area rises sharply. The new reported critical path starts at global
active-pair memory refill/queue selection and runs through interaction-rule
classification into selected-command rule-kind/control storage.

For diploma wording:

```text
Try30 confirms that LINK needs a service boundary: splitting substitution read
and write is enough to remove the substitution-memory II limiter. However, a
service boundary inserted inside the same parent SELECT/EXECUTE pipeline merely
moves the problem into broad command-control logic. The next design must also
register the selected command before classification/execution.
```

The next parallel experiment should preserve staged `LINK` but split SELECT
itself into narrower services. A useful next boundary is:

```text
refill
capture selected active pair
classify selected rule
execute or advance staged LINK
commit produced pair
```

This tests whether the `II=1` gain from staged `LINK` can be kept while
recovering timing from the selected-command classification path.

## Try31 Split Selected-Command Classification

Try31 implements the selected-command split recommended by try30 under:

```text
ENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY
```

The gate is default-off. In the focused structural profile, SELECT no longer
both captures a lane candidate and classifies its interaction rule. Instead, the
service sequence is:

```text
refill
capture selected active pair
classify selected rule
execute or advance staged LINK
commit produced pair
```

This required widening `ReducerServicePhase` from two bits to three bits,
because the new `PARALLEL_SERVICE_CLASSIFY` phase has value `4`. That change is
small but important: an undersized HLS state variable would truncate the phase
value and make the service FSM incorrect in hardware.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try30 selected command with staged `LINK` | 4770 | 2677 | 0 | 0 | 1 | `-7.29` | `68.57 MHz` |
| Try31 split classify | 4602 | 2667 | 0 | 0 | 1 | `-4.35` | `85.82 MHz` |

The result is a useful partial improvement. The classification split reduces
LUTs slightly and improves timing, so try30's SELECT/classification/control path
really was part of the limiter. However, the design is still not acceptable.
After the classification path is shortened, the reported critical path returns
to the staged `LINK` result/writeback region: Vitis reports a path from
`substitution_memory_0_load` through the produced-pair left-port store,
`local_status_code`, and `executed_steps`.

For diploma wording:

```text
Try31 shows that service boundaries must be placed around both scheduling and
rule writeback. Splitting active-pair capture from classification improves the
timing failure from try30 while preserving II=1, but the remaining critical path
is now the final LINK result tick, where substitution-memory data, produced-pair
append, status selection, and completion accounting still meet in one cycle.
```

The next experiment should preserve the try31 capture/classify split and staged
`LINK`, but split the final `LINK` write/result tick into two service ticks:

```text
write or clear substitution binding
append produced pair and update completion/status
```

That would directly target the current reported path instead of adding more
queue-selection or command-buffer state.

## Try32 Split Staged LINK Writeback

Try32 implements the try31 recommendation under:

```text
ENABLE_PARALLEL_SPLIT_LINK_WRITEBACK
```

It keeps the try31 selected-command capture/classify split and adds one more
stage to selected-command `LINK`:

```text
begin selected LINK
read previous substitution binding
write or clear substitution binding
append produced pair and update completion/status
commit produced pair
```

This is deliberately not a new scheduler shape. It adds only one emit-pair bit
and one `PARALLEL_LINK_STAGE_APPEND_RESULT` stage, so the experiment measures
whether the final `LINK` write/result tick was still too wide.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try30 selected command with staged `LINK` | 4770 | 2677 | 0 | 0 | 1 | `-7.29` | `68.57 MHz` |
| Try31 split classify | 4602 | 2667 | 0 | 0 | 1 | `-4.35` | `85.82 MHz` |
| Try32 split `LINK` writeback | 4594 | 2669 | 0 | 0 | 1 | `-3.19` | `95.28 MHz` |

The result is again not an accepted endpoint, because the 10 ns target with
2.70 ns uncertainty is still missed. It is nevertheless strong architectural
evidence. The design keeps `II=1`, uses no DSP or BRAM in the focused local
queue profile, and improves timing without increasing LUTs. The reported
critical path moves away from the staged `LINK` writeback body and into shared
control/status logic: `global_queue_count_value`, redex-queue empty/refill
selection, and the shared `local_status_code` store.

For diploma wording:

```text
Try32 shows that incremental staging can move the limiter out of a specific
interaction rule. Once LINK read/write and produced-pair emission are separated,
the remaining critical path is not LINK arithmetic or substitution memory, but
shared scheduler status/control muxing. This motivates a status-commit service
or narrower status protocol rather than deeper rule-specific staging.
```

The next experiment should keep the try31/try32 boundaries and target the
shared status path. A small pending-status register/service would test whether
`local_status_code` can be removed from every service branch:

```text
service phase writes pending_status_valid/status_code
status service commits pending error once
success cases avoid driving the global status mux
```

This would be a cleaner FPGA architecture than adding more command slots or
duplicating rule datapaths.

## Try33 Pending Status Commit Service

Try33 tests the status-control recommendation from try32 under:

```text
ENABLE_PARALLEL_PENDING_STATUS
```

It keeps the try31 capture/classify split and try32 staged-`LINK` writeback
split. Instead of adding another interaction-rule stage, selected-command error
paths record:

```text
pending_status_valid
pending_status_code
```

Then a narrow `PARALLEL_SERVICE_STATUS` phase commits the pending status into
`local_status_code`. The intended hardware effect was to reduce the number of
service branches feeding the shared status phi/store.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try31 split classify | 4602 | 2667 | 0 | 0 | 1 | `-4.35` | `85.82 MHz` |
| Try32 split `LINK` writeback | 4594 | 2669 | 0 | 0 | 1 | `-3.19` | `95.28 MHz` |
| Try33 pending status | 4772 | 2282 | 0 | 0 | 1 | `-3.16` | `95.58 MHz` |

This is a useful negative result. The pending-status service reduces FF usage,
but it increases LUTs and barely moves timing. The critical path remains shaped
by `global_queue_count_value`, redex-queue empty/refill control, and the
`local_status_code` phi/store. In other words, the problem is not only that
selected-command rule branches write status directly. The parent service loop
still combines global queue mutation/refill selection with status/control
resolution.

For diploma wording:

```text
Try33 shows that status deferral by itself is insufficient. The remaining
timing path is a global scheduler-control path, not a local rule-status path.
The next architectural boundary should split refill/count mutation from
selected-command execution rather than adding more status protocol state.
```

The next experiment should return to try32 as the better base and split refill
into narrower services:

```text
decide which local lanes need refill
pop from global active-pair queue and update global count
select/classify/execute selected command
commit produced pairs
```

This directly targets the `global_queue_count_value` portion of the current
critical path.

## Try34 Split Parallel Refill Pop

Try34 implements the refill split recommended by try33 under:

```text
ENABLE_PARALLEL_SPLIT_REFILL
```

The focused scheduler sequence becomes:

```text
refill capture: record empty local lanes
refill pop: pop at most one global active pair into one recorded lane
capture selected active pair
classify selected rule
execute or advance staged LINK
commit produced pair
```

The intended hardware effect was to stop the REFILL phase from both selecting
empty lanes and mutating `global_queue_count_value` for multiple lanes in one
pipeline tick. That is a sensible FPGA idea because queue count is a global
state register, and every extra comparator/mux attached to it tends to enlarge
the scheduler control path.

Full-profile CSIM and FPGA-small CSIM passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try32 split `LINK` writeback | 4594 | 2669 | 0 | 0 | 1 | `-3.19` | `95.28 MHz` |
| Try33 pending status | 4772 | 2282 | 0 | 0 | 1 | `-3.16` | `95.58 MHz` |
| Try34 split refill pop | 4680 | 2118 | 0 | 0 | 1 | `-4.11` | `87.67 MHz` |

This is useful negative evidence. The split reduced FFs but worsened timing.
The HLS critical path still connects loop control, `redex_queue_count`/
`global_queue_count_value`, redex empty comparison, and the shared
`local_status_code` phi/store. The design-size report also still shows two
syntactic calls to `active_pair_queue_pop_global`, because the lane0/lane1
priority selection was written as two pop sites.

For diploma wording:

```text
Try34 shows that a conceptual service split is not enough if the C structure
still presents HLS with duplicated queue-pop call sites and broad priority
control. The next hardware-oriented queue step must expose a true single pop
datapath: first choose a target lane, then perform one global pop, then route
the result to the selected lane.
```

The next experiment should not keep deepening refill in this same form. It
should either use a single `refill_target_lane` register with exactly one
global-pop call site, or move global pop into a dedicated one-entry pop buffer
before selected-command capture.

## Try35 Single-Site Parallel Refill Pop

Try35 implements the first try34 recommendation under:

```text
ENABLE_PARALLEL_SINGLE_REFILL_POP
```

It keeps `ENABLE_PARALLEL_SPLIT_REFILL`, but changes REFILL_POP from two
priority-coded global pop call sites to:

```text
REFILL: choose refill_target_lane
REFILL_POP: call active_pair_queue_pop_global exactly once
REFILL_POP: route the popped pair to refill_target_lane
```

This distinction matters for HLS. Try34 was architecturally single-owner in
intent, but its C structure still showed HLS two queue-pop sites. Try35 exposes
one global pop datapath.

Full-profile CSIM and FPGA-small CSIM passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try32 split `LINK` writeback | 4594 | 2669 | 0 | 0 | 1 | `-3.19` | `95.28 MHz` |
| Try33 pending status | 4772 | 2282 | 0 | 0 | 1 | `-3.16` | `95.58 MHz` |
| Try34 split refill pop | 4680 | 2118 | 0 | 0 | 1 | `-4.11` | `87.67 MHz` |
| Try35 single-site refill pop | 4003 | 2114 | 0 | 0 | 1 | `-2.05` | `107.01 MHz` |

This is not a final timing-clean endpoint, but it is the best focused parallel
scheduler result so far. The design-size report confirms the intended
structural change: `active_pair_queue_pop_global` drops from try34's two-call
`904` instruction contribution to one `452` instruction contribution. The
critical path also moves away from global queue pop/count and into
`executed_steps` completion accounting.

For diploma wording:

```text
Try35 demonstrates that the source-level shape of a service boundary matters
for HLS. A conceptual "single refill service" still synthesized poorly when
written as two priority-coded pop sites. Capturing the target lane first and
calling the global queue pop exactly once reduced area and improved timing,
moving the limiter from queue ownership to completion accounting.
```

The next experiment should preserve try35 as the base and split selected-command
success completion:

```text
execute selected command
commit produced pair
complete selected command and increment executed_steps
```

The goal is to remove `executed_steps` update from the same broad control path
that checks the loop condition and selects service status.

## Try36 Selected-Command Completion Service

Try36 implements the try35 follow-up under:

```text
ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION
```

It keeps the try35 single-site refill pop and adds a narrow completion service:

```text
selected command executes
selected command records completion metadata
completion service increments executed_steps
commit service flushes produced pairs
```

This was meant to remove `executed_steps` from the selected-command execution
tick. Full-profile CSIM and FPGA-small CSIM both passed, and focused synthesis
completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try35 single-site refill pop | 4003 | 2114 | 0 | 0 | 1 | `-2.05` | `107.01 MHz` |
| Try36 selected completion service | 4091 | 2117 | 0 | 0 | 1 | `-2.86` | `98.40 MHz` |

This is a negative result. The report no longer identifies `executed_steps` as
the main critical-path endpoint, but the new completion-valid routing widens the
service-phase and status-selection control. Vitis reports a critical path from
`selected_command_produced_active_pairs.produced_pairs.left_port` through
non-link rule checks and the `local_status_code` phi/store, plus an unmet path
to the `parallel_service_phase` store at `select_ln1715`.

For diploma wording:

```text
Try36 shows that deferring success completion is not automatically beneficial
in the parallel scheduler. The selected-command controller is already dominated
by produced-pair state, rule-status classification, and service-phase routing.
Adding a completion-valid protocol removes one register update from the path
but increases the control fan-in feeding the same scheduler state.
```

The next experiment should return to try35 and target the produced-pair/status
path directly, for example by narrowing selected-command produced-pair state or
splitting non-link append/status work, rather than adding a generic completion
phase.

## Try37 Compact Parallel Produced-Pair Buffer

Try37 implements the produced-pair state-width recommendation under:

```text
ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER
```

The gate is default-off. In the focused parallel synthesis profile, it is used
only together with disabled `COMMUTE`, `OPERATE`, `SWITCH`, and `CALL`. Under
that rule subset, `LINK` emits at most one produced pair and `ERASE`/
`ANNIHILATE` emit at most two, so the generic full-runtime capacity of `17` is
not needed. The focused produced-pair buffer is reduced to capacity `2`, and
the produced-pair counter width is reduced to `2` bits.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try35 single-site refill pop | 4003 | 2114 | 0 | 0 | 1 | `-2.05` | `107.01 MHz` |
| Try36 selected completion service | 4091 | 2117 | 0 | 0 | 1 | `-2.86` | `98.40 MHz` |
| Try37 compact produced buffer | 3998 | 1148 | 0 | 0 | 1 | `-2.00` | `107.48 MHz` |

This is still not timing-clean under the current clock/uncertainty settings,
but it is the best focused parallel scheduler result so far. The design-size
report drops to `592` final HW-transform instructions from try35's `808` and
try36's `864`. The key FPGA lesson is that semantic-complete buffer sizing can
pollute a focused hardware experiment: the 17-entry produced-pair array exists
for full `CALL` expansion, not for the restricted parallel scheduler rule
subset.

For diploma wording:

```text
Try37 demonstrates why runtime profiles need not only operation gating but also
state-width gating. Once high-fanout rules are disabled in the focused parallel
synthesis profile, retaining a full CALL-sized produced-pair buffer synthesizes
unnecessary registers and muxes. Narrowing that buffer preserves the measured
rule subset and nearly halves the scheduler FF estimate.
```

The remaining critical path now points into staged `LINK` substitution read and
append-result status handling. The next scheduler experiment should preserve
the compact buffer and split or specialize that `LINK` append/status path.

## Try38 Direct Staged LINK Append

Try38 specializes the selected-command staged `LINK` append path under:

```text
ENABLE_PARALLEL_DIRECT_LINK_APPEND
```

The gate is default-off and is used only with the try37 compact produced-pair
profile. In that profile, selected-command `LINK` emits at most one produced
pair and the buffer is freshly initialized for the selected command. Therefore
the staged `LINK` append-result tick can write slot `0` directly and set the
produced-pair count to `1`, instead of calling the generic checked
`append_produced_active_pair` helper.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try35 single-site refill pop | 4003 | 2114 | 0 | 0 | 1 | `-2.05` | `107.01 MHz` |
| Try37 compact produced buffer | 3998 | 1148 | 0 | 0 | 1 | `-2.00` | `107.48 MHz` |
| Try38 direct staged `LINK` append | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |

This is still not timing-clean, but it improves the best focused parallel
scheduler point. The design-size report drops to `574` final HW-transform
instructions from try37's `592`, and the reported critical path no longer
passes through the generic append helper. The remaining path is now clearer:

```text
substitution_memory_0_load
selected_command_link_previous_port
unbound-marker comparison
local_status_code phi/store
```

For diploma wording:

```text
Try38 shows that rule-specific specialization can remove generic runtime
bookkeeping from the scheduler datapath. In the focused parallel profile,
staged LINK has a statically bounded one-pair emission, so a direct slot write
is enough. The improvement is measurable, but the next limiting structure is
the staged LINK substitution read/compare/status dependency itself.
```

The next experiment should preserve try38 and split the `LINK` previous-port
comparison or unbound-marker decision from the status/control update.

## Try39 Staged LINK Read-Decode Bit

Try39 tests that recommendation under:

```text
ENABLE_PARALLEL_LINK_READ_DECODE
```

It preserves the try38 focused base:

```text
compact produced-pair buffer
direct selected-command staged LINK append
single-site global refill pop
```

The change is deliberately small. During the staged `LINK` substitution-read
tick, the reducer stores the previous substitution port and also stores a
one-bit predicate:

```text
selected_command_link_previous_is_unbound
```

The writeback tick then uses that bit instead of comparing the full previous
port against the unbound marker again. Full-profile CSIM and FPGA-small CSIM
both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try38 direct staged `LINK` append | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |
| Try39 staged `LINK` read-decode bit | 3751 | 1149 | 0 | 0 | 1 | about `-2.00` | `107.48 MHz` |

This is a negative result. The cached predicate does not remove the critical
path; it becomes part of it. Vitis reports:

```text
substitution_memory_0_load
previous_port
selected_command_link_previous_is_unbound
selected_command_link_previous_is_unbound_load
local_status_code phi/store
```

For diploma wording:

```text
Try39 shows that simply caching a decoded control predicate is insufficient
when the scheduler still consumes that predicate in the same loop-carried
service controller. The limiting structure is not only the full-width equality
comparison; it is the coupling between substitution-memory read results and
status/service selection in the selected-command pipeline.
```

The next experiment should return to try38 and remove successful selected
`LINK` execution from the `local_status_code` mux, for example by making success
the implicit fall-through path and routing only error cases through an explicit
error-valid/status register.

## Try40 Direct Staged LINK Implicit Success

Try40 tests the narrowest version of that idea under:

```text
ENABLE_PARALLEL_LINK_IMPLICIT_SUCCESS
```

It returns to the try38 base and does not enable the try39 read-decode bit. In
the focused compact/direct `LINK` profile, the append-result tick cannot
overflow because the selected-command staged `LINK` writes produced-pair slot
`0` directly. Try40 therefore treats completion as an implicit success in that
specific gated path, instead of checking `local_status_code == SUCCESS` before
counting the completed `LINK`.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try38 direct staged `LINK` append | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |
| Try40 direct staged `LINK` implicit success | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |

This is a neutral negative result. HLS produced the same focused resource and
timing point as try38. The critical path still reports:

```text
substitution_memory_0_load
selected_command_link_previous_port
unbound-marker comparison
local_status_code phi/store
```

For diploma wording:

```text
Try40 shows that the visible status/control bottleneck is not caused by the
final success guard after staged LINK completion. Removing that guard in the
direct compact profile synthesizes to the same hardware point as try38. The
remaining issue is the writeback tick itself: substitution-memory read,
unbound-marker decision, substitution writeback, and scheduler status/control
still share one pipelined service region.
```

The next experiment should split the staged `LINK` writeback tick rather than
adjust completion guards.

## Try41 Staged LINK Decide-Write Split

Try41 implements that writeback split under:

```text
ENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT
```

Unlike try39, it does not compute the unbound predicate in the read stage. It
adds a separate selected-command staged `LINK` phase:

```text
READ_PREVIOUS -> DECIDE_RESULT -> WRITE_RESULT -> APPEND_RESULT
```

The `DECIDE_RESULT` tick compares the previously read substitution port against
the unbound marker and stores only the one-bit emit decision. The later
`WRITE_RESULT` tick mutates substitution memory based on that decision. This
adds one service tick for variable `LINK`, but separates the unbound-marker
comparison from substitution-memory writeback and interaction completion.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try38 direct staged `LINK` append | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |
| Try40 direct staged `LINK` implicit success | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |

This is the strongest focused parallel scheduler result so far. It is still
not timing-clean under the current uncertainty setting, but it moves the top
critical path away from staged `LINK` substitution memory. Vitis now reports a
path through selected-command produced-pair state and non-link erasure/node
checks feeding `local_status_code`.

For diploma wording:

```text
Try41 validates that HLS-visible service boundaries matter more than the
software-level size of the rule. Splitting staged LINK decision from writeback
adds one reducer service tick, but removes the substitution-memory
read/compare/write chain from the top critical path. This is an FPGA-oriented
tradeoff: slightly higher rule latency for a shorter and more bankable
scheduler datapath.
```

The next experiment should preserve try41 and target the new non-link
produced-pair/node-check path, especially `ERASE`/`ANNIHILATE` output
construction in the focused rule subset.

## Try42 Direct Non-Link Outputs

Try42 tests the most direct version of that recommendation under:

```text
ENABLE_PARALLEL_DIRECT_NON_LINK_OUTPUTS
```

It preserves the try41 staged `LINK` decide/write split and adds direct
selected-command output helpers for `ERASE` and `ANNIHILATE`. In the focused
profile, the produced buffer is already compacted to two entries and
`COMMUTE`, `OPERATE`, `SWITCH`, and `CALL` are disabled. Therefore valid
`ERASE` and `ANNIHILATE` interactions always emit exactly two produced pairs.
The try writes those two slots directly instead of calling
`append_produced_active_pair` twice.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try42 direct non-link outputs | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |

This is a negative result. Direct two-slot writes for `ERASE`/`ANNIHILATE` do
not change the hardware point, so the generic append helper was not the current
limiter after try37's compact produced buffer. The critical path remains:

```text
selected_command_produced_active_pairs.produced_pairs.left_port
port tag / erasure selection
node address selection
node address range check
local_status_code phi/store
```

For diploma wording:

```text
Try42 shows that once the produced-pair buffer is compacted, replacing generic
append calls with direct slot writes does not further improve the focused
parallel scheduler. HLS already reduces that structure well enough. The
remaining timing pressure is control/status coupling around selected non-link
node validation, not produced-pair writeback.
```

The next experiment should therefore avoid another append specialization.
Instead, it should stage selected non-link node validation and node-data capture
before produced-pair emission, separating `ERASE`/`ANNIHILATE` validity checks
from the `local_status_code` update.

## Try43 Staged Non-Link Validation

Try43 implements that staged non-link validation idea under:

```text
ENABLE_PARALLEL_STAGED_NON_LINK_VALIDATION
```

The design preserves try41's staged `LINK` decide/write split. For selected
`ERASE`/`ANNIHILATE`, it reuses the existing `PARALLEL_SERVICE_CLASSIFY`
service for a second tick:

```text
CLASSIFY rule kind
CLASSIFY validate non-link node ports and capture two produced pairs
EXECUTE emit captured produced pairs and count the rule
COMMIT push produced pairs
```

The choice deliberately avoids widening `ReducerServicePhase`, but it adds
validation-pending and validated state for the selected command.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try42 direct non-link outputs | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try43 staged non-link validation | 3754 | 1288 | 0 | 0 | 1 | `-4.92` | `81.81 MHz` |

This is a negative result. The staged non-link idea is semantically valid and
keeps `II=1`, but reusing `CLASSIFY` creates a worse control path. Vitis
reports the new critical path through:

```text
selected_command_produced_active_pairs.produced_pairs.right_port
get_interaction_rule_kind
selected_command_rule_kind sparse mux
parallel_service_phase select/store
```

For diploma wording:

```text
Try43 shows that adding a service tick is only useful when the new service has
a smaller control cone. Reusing the classify controller for staged non-link
validation moved the bottleneck from node validation into rule-decode and
service-phase selection, reducing Fmax from 118.28 MHz to 81.81 MHz.
```

The next experiment should return to try41 and reduce selected-command
classification fan-in directly. One candidate is a compact selected-command
class field computed during SELECT, so EXECUTE routes on a few one-bit class
flags rather than repeatedly carrying the full interaction-rule decode through
the service controller.

## Try44 Compact Selected-Command Classify

Try44 tests that candidate directly under:

```text
ENABLE_PARALLEL_SELECTED_COMPACT_CLASSIFY
```

The focused parallel synthesis profile disables `COMMUTE`, `OPERATE`,
`SWITCH`, and `CALL`, so the selected-command scheduler only needs to
distinguish `LINK`, `VOID`, `ERASE`, `ANNIHILATE`, and unsupported pairs.
Try44 adds a compact classifier for that subset and computes the selected rule
kind during SELECT, routing directly to EXECUTE instead of going through the
split CLASSIFY service.

Full-profile CSIM and FPGA-small CSIM both passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try43 staged non-link validation | 3754 | 1288 | 0 | 0 | 1 | `-4.92` | `81.81 MHz` |
| Try44 compact classify in SELECT | 3842 | 1156 | 0 | 0 | 1 | `-7.29` | `68.57 MHz` |

This is a negative result. The classifier is smaller than the generic
interaction dispatch, but SELECT already owns global active-pair load,
lane-local queue refill, and candidate selection. Placing classification in
that same service tick couples queue movement to rule decoding and creates the
new top critical path:

```text
global active-pair load
local lane queue right-port store/load
candidate active pair
compact rule classifier
selected-command rule-kind mux
execute selected non-link path
```

For diploma wording:

```text
Try44 demonstrates that reducing a Boolean rule classifier is not sufficient
when it is placed in the wrong hardware service phase. Computing selected-rule
classification during SELECT moved work into the queue/refill path and
regressed timing from 118.28 MHz to 68.57 MHz. The scheduler therefore needs
both compact rule encoding and phase placement that keeps queue movement
separate from rule execution control.
```

The next experiment should return to try41 and keep classification outside
SELECT. A more promising direction is a compact classifier in the existing
CLASSIFY service, or a narrow class field produced by a dedicated classify tick
that does not also perform lane refill/global queue movement.

## Try45 Compact Classify Service

Try45 tests the first try44 recommendation under:

```text
ENABLE_PARALLEL_COMPACT_CLASSIFY_SERVICE
```

Unlike try44, it keeps the try41 SELECT-to-CLASSIFY service boundary. SELECT
only captures the selected candidate and routes to CLASSIFY. The compact
focused-rule decoder is used inside `PARALLEL_SERVICE_CLASSIFY`, where the
generic `get_interaction_rule_kind` call previously ran.

Full-profile CSIM and FPGA-small CSIM passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try44 compact classify in SELECT | 3842 | 1156 | 0 | 0 | 1 | `-7.29` | `68.57 MHz` |
| Try45 compact classify in CLASSIFY | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |

This is a neutral negative result. It confirms that try44's regression came
from putting decode in SELECT, not from the compact decoder itself. However,
using the compact decoder in CLASSIFY does not improve over try41. The
critical path remains:

```text
selected produced-pair left port
erasure-port / node-address selection
node-address range check
local_status_code phi/store
```

For diploma wording:

```text
Try45 separates two effects from try44. Compact classification is not harmful
when kept in the classify service, but it is also not sufficient to improve the
focused parallel scheduler. The limiting path is no longer rule classification;
it is selected non-link execution and status production.
```

The next experiment should stop optimizing classification alone. A better
direction is a dedicated narrow selected non-link service that validates/reads
node data in one tick and emits produced pairs in a later tick, without
reusing the broad `PARALLEL_SERVICE_CLASSIFY` controller that failed in try43.

## Try46 Dedicated Non-Link Read Service

Try46 tests that recommendation under:

```text
ENABLE_PARALLEL_STAGED_NON_LINK_SERVICE
```

The implementation adds a dedicated `PARALLEL_SERVICE_NON_LINK_READ` phase for
selected `ERASE`/`ANNIHILATE`. CLASSIFY still identifies the selected rule, but
node-address validation and node-data reads move into the new service. EXECUTE
then emits the two captured produced pairs and counts the interaction.

Full-profile CSIM and FPGA-small CSIM passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try45 compact classify in CLASSIFY | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try46 dedicated non-link read service | 3752 | 1288 | 0 | 0 | 1 | `-4.92` | `81.81 MHz` |

This is a negative result. The new service phase is functionally correct, but
it widens the selected-command controller. Vitis reports the new critical path
through:

```text
selected produced-pair right port
get_interaction_rule_kind / ports_have_same_tag
selected_command_rule_kind sparse mux
parallel_service_phase select/store
```

For diploma wording:

```text
Try46 shows that a new service phase must also have a narrow control encoding.
Moving node validation out of EXECUTE is not enough when the transition into
that phase still depends on the full selected rule kind and the widened
parallel service-phase mux. The architecture needs narrower selected-command
class state, not just more states.
```

The next experiment should not add another general service phase using the full
`InteractionRuleKind` enum. It should either combine non-link staging with a
narrow class bitfield, or specialize only one non-link rule at a time to avoid
widening the shared selected-command controller.

## Try47 Narrow Selected Class With Non-Link Service

Try47 tests the first try46 recommendation under:

```text
ENABLE_PARALLEL_NARROW_SELECTED_CLASS
```

The implementation keeps the try46 `PARALLEL_SERVICE_NON_LINK_READ` service
and adds a selected-command class field with only five values:

```text
LINK, VOID, ERASE, ANNIHILATE, UNSUPPORTED
```

The intent was to avoid steering the non-link service from the full
`InteractionRuleKind` enum. Full-profile CSIM and FPGA-small CSIM passed.
Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try46 dedicated non-link read service | 3752 | 1288 | 0 | 0 | 1 | `-4.92` | `81.81 MHz` |
| Try47 narrow selected class wrapper | 4220 | 1298 | 0 | 0 | 1 | `~ -8.18` | `64.59 MHz` |

This is a negative result. The class was narrow in C++ type shape, but not in
hardware structure. The focused path still computed the compact rule kind,
then mapped it to the class, then mapped the class back to the full rule kind
for compatibility with the rest of the selected-command controller.

Vitis design-size evidence shows the added path explicitly:

```text
get_narrow_selected_class: 2730 compile/link instructions
get_selected_compact_rule_kind: 2619 compile/link instructions
get_rule_kind_from_selected_class: 123 compile/link instructions
```

The reported critical path stayed in selected-command control:

```text
selected produced-pair right port
compact rule classifier / ports_have_same_tag
selected class mux
rule-kind compatibility mux
parallel_service_phase select/store
```

For diploma wording:

```text
Try47 distinguishes semantic narrowing from hardware narrowing. A small class
field does not reduce FPGA control depth if the implementation still computes
and preserves the wider rule-kind representation in the hot path. This result
supports treating selected-rule control as an architectural interface, not a
local helper-function substitution.
```

The next experiment should avoid compatibility conversion in the focused hot
path. Either specialize one non-link rule from the try41 baseline, or build a
true class-only selected executor where the focused scheduler no longer stores
or branches on `selected_command_rule_kind`.

## Try48 True Class-Only Selected Command

Try48 tests the second try47 recommendation under:

```text
ENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED
```

Unlike try47, this profile does not classify through
`InteractionRuleKind -> class -> InteractionRuleKind`. It adds a direct
selected-command class decoder and uses that class in the measured focused
selected path. The focused CLASSIFY tick stores only:

```text
LINK, VOID, ERASE, ANNIHILATE, UNSUPPORTED
```

Selected `VOID` is handled directly in EXECUTE. Selected `LINK` still uses the
existing staged LINK service. Selected `ERASE`/`ANNIHILATE` still use the
existing focused non-link helper, so this try isolates the control-interface
change before attempting another non-link datapath split.

Full-profile CSIM and FPGA-small CSIM passed. Focused synthesis completed:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try41 staged `LINK` decide/write split | 3707 | 1150 | 0 | 0 | 1 | `-1.16` | `118.28 MHz` |
| Try47 narrow selected class wrapper | 4220 | 1298 | 0 | 0 | 1 | `~ -8.18` | `64.59 MHz` |
| Try48 true class-only selected command | 3791 | 1162 | 0 | 0 | 1 | `~ -1.16` | `118.28 MHz` |

This is a neutral negative result. It removes the try47 timing damage, proving
that the compatibility round trip was the problem. It does not beat try41,
because the remaining critical path returns to selected non-link
node-address/status production:

```text
selected produced-pair left port
erasure-port / node-address selection
node-address range check
local_status_code phi/store
```

For diploma wording:

```text
Try48 shows the difference between a narrow software wrapper and a narrow
hardware interface. Removing the rule-kind compatibility conversion restores
the previous scheduler timing point, but the next bottleneck is rule datapath
validation, not selected-rule classification.
```

The next experiment should not revisit selected-command classification alone.
It should specialize one selected non-link datapath at a time, beginning with
`ERASE`, and measure whether separating node-address validation/status from
produced-pair emission reduces the `local_status_code` critical path without
reintroducing broad service-phase muxing.
