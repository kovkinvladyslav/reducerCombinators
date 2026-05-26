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
