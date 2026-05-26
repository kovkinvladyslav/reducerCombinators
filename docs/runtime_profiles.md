# Runtime Profiles

The reducer has two compile-time runtime profiles. They are not two different
implementations. They are two build modes for the same interaction-combinator
runtime.

## Why Profiles Exist

The full reducer supports the intended prototype semantics, including numeric
features such as `F24`, division, and remainder. Those operations are useful for
correctness experiments and for showing the expressive scope of the model.

On a small FPGA, however, those same operations are expensive hardware blocks.
The synthesis report showed that floating-point arithmetic, division, remainder,
and `fmod` were major contributors to LUT, FF, and DSP usage inside the hot
reducer loop.

The profile split therefore separates two research goals:

- **functional completeness**, where the reducer keeps the full prototype
  semantics;
- **FPGA feasibility**, where the reducer keeps the same runtime architecture
  but removes the highest-cost numeric datapaths from synthesis.

This keeps the project honest: the complete model remains available, while the
small-FPGA build gives a realistic resource target for Artix-7 experiments.

## FULL_SIMULATION_PROFILE

`FULL_SIMULATION_PROFILE` is the default profile.

If no profile macro is supplied, the reducer builds as:

```text
FULL_SIMULATION_PROFILE
```

This profile keeps the current full numeric subsystem:

- `U24`
- `I24`
- `F24`
- addition, subtraction, multiplication
- division
- remainder
- comparisons
- bitwise operations
- shifts

Use this profile for:

- CSIM correctness tests;
- semantic validation;
- complete-model diploma discussion;
- tests that intentionally exercise `F24`, division, or remainder.

## FPGA_SMALL_PROFILE

`FPGA_SMALL_PROFILE` is the resource-oriented FPGA profile.

Enable it with:

```text
-DFPGA_SMALL_PROFILE=1
```

For CSIM, the flag must be applied to both the design and the testbench:

```ini
syn.cflags=-DFPGA_SMALL_PROFILE=1
tb.cflags=-DFPGA_SMALL_PROFILE=1
```

This profile keeps the core interaction-combinator runtime:

- `LINK`
- `VOID`
- `ERASE`
- `ANNIHILATE`
- `COMMUTE`
- `OPERATE`
- `SWITCH`
- `CALL`

It also keeps lower-cost integer numeric operations:

- `U24`
- `I24`
- addition, subtraction, multiplication
- comparisons
- bitwise operations
- shifts

It disables the expensive numeric paths:

- `F24`
- division
- remainder

Disabled operations still fail explicitly:

| Disabled operation | Status |
| --- | --- |
| `F24` numeric operation | `REDUCER_STATUS_UNSUPPORTED_NUMERIC_TYPE` |
| division | `REDUCER_STATUS_INVALID_NUMERIC_OPERATION` |
| remainder | `REDUCER_STATUS_INVALID_NUMERIC_OPERATION` |

Use this profile for:

- Artix-7 synthesis experiments;
- LUT/FF/DSP reduction measurements;
- initiation interval experiments after removing expensive numeric datapaths;
- Chapter 4 resource-feasibility discussion.

## Existing Config Files

The project currently provides:

| Config file | Purpose |
| --- | --- |
| `hls_config.cfg` | Full single-lane baseline, default full profile |
| `hls_config_parallel.cfg` | Experimental parallel reducer top |
| `hls_config_synth_parallel_fpga_small_bank_metrics.cfg` | Focused parallel synthesis probe with `FPGA_SMALL_PROFILE`, `CALL` disabled, statistics enabled, and bank-conflict counters present |
| `hls_config_synth_parallel_fpga_small_nostats_nocall.cfg` | Focused parallel synthesis probe with `FPGA_SMALL_PROFILE`, `CALL` disabled, and reducer/parallel statistics disabled |
| `hls_config_synth_parallel_fpga_small_structural_nostats.cfg` | Focused structural parallel synthesis probe with only `VOID`, `LINK`, `ERASE`, and `ANNIHILATE` enabled |
| `hls_config_synth_parallel_fpga_small_structural_phased_nostats.cfg` | Focused structural parallel synthesis probe with explicit refill/select/execute/commit service phases |
| `hls_config_synth_parallel_fpga_small_structural_phased_nolink_nostats.cfg` | Diagnostic structural parallel synthesis probe with the try19 phased scheduler and `LINK` disabled to isolate substitution-memory cost |
| `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_nostats.cfg` | Diagnostic structural parallel synthesis probe with `LINK` disabled and register-backed lane-local queues |
| `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_helpers_nostats.cfg` | Failed diagnostic structural parallel probe that factors phased scheduler services into non-inlined helpers; useful evidence that C++ helper boundaries are not enough to reduce scheduler II |
| `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_unpipelined_nostats.cfg` | Diagnostic structural parallel probe that removes the parent scheduler-loop pipeline constraint to measure service-dispatch pressure separately from explicit II |
| `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_pairwise_select_nostats.cfg` | Diagnostic structural parallel probe that specializes SELECT/conflict logic for the fixed two-lane case |
| `hls_config_synth_parallel_fpga_small_structural_phased_voidonly_regqueue_nostats.cfg` | Failed diagnostic parallel probe that leaves only `VOID` enabled to isolate produced-pair/writeback pressure; useful evidence that removing produced-pair rules can reach `II=1` but with unacceptable timing |
| `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_selected_command_nostats.cfg` | Diagnostic structural parallel probe that replaces two-lane selected state with one registered selected-lane command; improves the focused scheduler to `II=2` with positive timing but sacrifices dual issue |
| `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_two_command_nostats.cfg` | Failed diagnostic structural parallel probe that extends the selected-command idea to two command slots; synthesis completes but duplicated execute/commit datapaths raise LUT and break timing |
| `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_waiting_command_nostats.cfg` | Failed diagnostic structural parallel probe with one active selected command plus one waiting command slot; avoids duplicated datapaths but adds timing-heavy command-control state |
| `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_nostats.cfg` | Failed diagnostic structural parallel probe that restores `LINK` on the try26 selected-command boundary; confirms that the substitution-memory read/previous-port/status path is the remaining timing limiter |
| `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_nostats.cfg` | Failed-but-informative structural parallel probe that stages selected-command `LINK`; reaches scheduler `II=1`, but fails timing because SELECT/classification/control becomes the new critical path |
| `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_nostats.cfg` | Failed-but-informative structural parallel probe that keeps staged selected-command `LINK` and adds a service boundary between selected active-pair capture and interaction-rule classification; improves timing versus try30 but still fails on staged-`LINK` result/writeback |
| `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_nostats.cfg` | Failed-but-informative structural parallel probe that keeps try31's capture/classify split and further splits staged-`LINK` substitution writeback from produced-pair/completion accounting; improves timing but leaves shared status/control as the limiter |
| `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_pending_status_nostats.cfg` | Failed-but-informative structural parallel probe that keeps try31/try32 staging and defers selected-command errors through a pending-status service; barely improves timing and shows shared refill/status control remains the limiter |
| `hls_config_fpga_small.cfg` | Single-lane reducer with `FPGA_SMALL_PROFILE` enabled |
| `hls_config_synth_full.cfg` | Lean full-profile synthesis config |
| `hls_config_synth_fpga_small.cfg` | Lean FPGA-small synthesis config |
| `hls_config_synth_full_nostats.cfg` | Full-profile synthesis probe with statistics disabled |
| `hls_config_synth_full_staged_call_nostats.cfg` | Full-profile synthesis probe with statistics disabled and staged `CALL` enabled |
| `hls_config_full_compact_service_phase.cfg` | Full-profile CSIM checkpoint with staged `CALL` and the compact service-phase architecture enabled |
| `hls_config_synth_full_compact_service_phase_nostats.cfg` | Full-profile synthesis checkpoint with statistics disabled and compact service-phase architecture enabled |
| `hls_config_full_compact_service_phase_no_f24_rem.cfg` | Full-profile CSIM checkpoint with compact service phase and F24 remainder disabled |
| `hls_config_synth_full_compact_service_phase_no_f24_rem_nostats.cfg` | Full-profile synthesis checkpoint with compact service phase, statistics disabled, and F24 remainder disabled |
| `hls_config_full_compact_service_phase_shared_int_div_rem_no_f24_rem.cfg` | Full-profile CSIM checkpoint with compact service phase, F24 remainder disabled, and shared integer div/rem operand selection enabled |
| `hls_config_synth_full_compact_service_phase_shared_int_div_rem_no_f24_rem_nostats.cfg` | Full-profile synthesis checkpoint with statistics disabled, F24 remainder disabled, and shared integer div/rem operand selection enabled |
| `hls_config_full_compact_service_phase_f24_addsub_fabric_shared_int_no_f24_rem.cfg` | Full-profile CSIM checkpoint with F24 remainder disabled, shared integer div/rem, and F24 add/sub bound to fabric |
| `hls_config_synth_full_compact_service_phase_f24_addsub_fabric_shared_int_no_f24_rem_nostats.cfg` | Full-profile synthesis checkpoint with statistics disabled, F24 remainder disabled, shared integer div/rem, and F24 add/sub bound to fabric |
| `hls_config_synth_fpga_small_nostats.cfg` | FPGA-small synthesis probe with statistics disabled |
| `hls_config_synth_fpga_small_nostats_nocall.cfg` | FPGA-small synthesis probe with statistics and `CALL` disabled |
| `hls_config_synth_fpga_small_staged_call.cfg` | FPGA-small synthesis probe with staged single-lane `CALL` enabled |
| `hls_config_fpga_small_produced_buffer.cfg` | FPGA-small CSIM config with staged `CALL` and produced-pair buffer enabled |
| `hls_config_synth_fpga_small_produced_buffer.cfg` | FPGA-small synthesis probe with staged `CALL` and produced-pair buffer enabled |
| `hls_config_fpga_small_deferred_completion.cfg` | FPGA-small CSIM config with staged `CALL`, produced-pair buffer, and deferred completion accounting enabled |
| `hls_config_synth_fpga_small_deferred_completion.cfg` | FPGA-small synthesis probe with staged `CALL`, produced-pair buffer, and deferred completion accounting enabled |
| `hls_config_fpga_small_inline_small_rules.cfg` | FPGA-small CSIM config with staged `CALL`, produced-pair buffer, deferred completion accounting, and small-rule helper inlining enabled |
| `hls_config_synth_fpga_small_inline_small_rules.cfg` | FPGA-small synthesis probe with staged `CALL`, produced-pair buffer, deferred completion accounting, and small-rule helper inlining enabled |
| `hls_config_fpga_small_compact_completion.cfg` | FPGA-small CSIM config with staged `CALL`, produced-pair buffer, deferred completion accounting, small-rule helper inlining, and compact completion metadata enabled |
| `hls_config_synth_fpga_small_compact_completion.cfg` | FPGA-small synthesis probe with staged `CALL`, produced-pair buffer, deferred completion accounting, small-rule helper inlining, and compact no-statistics completion metadata enabled |
| `hls_config_fpga_small_narrow_buffer_cursor.cfg` | FPGA-small CSIM config with staged `CALL`, produced-pair buffer, deferred completion accounting, compact completion metadata, and narrowed produced-buffer cursor enabled |
| `hls_config_synth_fpga_small_narrow_buffer_cursor.cfg` | FPGA-small synthesis probe with staged `CALL`, produced-pair buffer, deferred completion accounting, compact completion metadata, and narrowed produced-buffer cursor enabled |
| `hls_config_fpga_small_derived_buffer_activity.cfg` | FPGA-small CSIM config deriving produced-buffer activity from the narrow produced-pair count instead of a separate active flag |
| `hls_config_synth_fpga_small_derived_buffer_activity.cfg` | FPGA-small synthesis probe deriving produced-buffer activity from the narrow produced-pair count instead of a separate active flag |
| `hls_config_fpga_small_compact_service_phase.cfg` | FPGA-small CSIM config with compact reducer service-phase control enabled |
| `hls_config_synth_fpga_small_compact_service_phase.cfg` | FPGA-small synthesis probe with compact reducer service-phase control enabled |
| `hls_config_synth_structural_core.cfg` | Structural IC core synthesis probe: `LINK`, `VOID`, `ERASE`, `ANNIHILATE`, `COMMUTE` |

The `nostats` synthesis configs disable `ENABLE_REDUCER_STATISTICS` and
`ENABLE_PARALLEL_REDUCER_STATISTICS`. They should be used only to measure the
cost of the reducer datapath without the wide statistics output network. CSIM
and Chapter 4 activity-counter measurements should continue to use the normal
configs with statistics enabled.

The `nocall` and `structural_core` configs are synthesis probes, not replacement
semantic baselines. They help isolate whether HLS is blocked by the packed-book
`CALL` path or by the core interaction-combinator rewrite datapath itself.

The staged-call config is the first synthesis-oriented attempt to keep `CALL`
available without inlining the full packed-book instantiation loop into the
small-rule reducer datapath. It should be compared against the no-`CALL` control
before being described as a final FPGA-small `CALL` implementation.

The produced-buffer configs are an explicit queue-architecture experiment. They
route rule outputs through a local produced-pair buffer before writing back to
`redex_memory`. They are not yet the final queue architecture. Try2 showed that
partial buffering preserves correctness and completes synthesis, but does not
reduce the main-loop `II=4` queue-memory dependency. Try3 extended the same
gate to all single-lane produced active pairs and improved final II to `3`, but
moved the limiter to a long control/timing path.

The deferred-completion configs are a scheduler-boundary experiment. They keep
the produced-pair buffer and add a one-entry pending completion service so rule
execution no longer writes `executed_steps` in the same scheduled cycle. Try5
showed this can reach final loop `II=1` in FPGA-small synthesis, at higher LUT
and FF cost.

The inline-small-rules configs are an area-recovery experiment on top of the
try5 scheduler boundary. They keep staged `CALL`, the single-owner produced-pair
commit service, and deferred completion accounting, but force the tiny buffered
`LINK`, `ERASE`, and `ANNIHILATE` helpers inline. Try6 showed this preserves
final loop `II=1` and recovers a small amount of area: `10292 LUT` and `4385 FF`
compared with try5's `10591 LUT` and `4425 FF`.

The compact-completion configs are a state-width experiment on top of try6. In
no-statistics synthesis builds, completion accounting does not need the full
completed rule kind, so `ENABLE_COMPACT_COMPLETION_STATE` carries only the
metadata needed for control. Try7 showed this preserves final loop `II=1` and
reduces area to `9997 LUT` and `4354 FF`.

The narrow-buffer-cursor configs are a produced-buffer state-width experiment on
top of try7. The produced-pair buffer has a fixed capacity of `17`, so its count
and flush cursor use the narrow `ProducedPairCount` type instead of 32-bit
counters. Try8 showed this preserves final loop `II=1` and reduces area to
`9626 LUT` and `4246 FF`.

The derived-buffer-activity configs are a service-state experiment on top of
try8. They remove the separate `produced_pair_buffer_active` flag and derive
buffer service activity from the nonzero produced-pair count. Try9 preserved
final loop `II=1` and produced `9615 LUT`, `4269 FF`, and `2 DSP`. The critical
path moved from `produced_pair_buffer_active` to `pending_completion_active`,
showing that the next control target is the broader service-phase encoding.

The compact-service-phase configs are a reducer-controller experiment on top of
try9. They encode completion, produced-pair flush, staged `CALL` service, and
normal dispatch as one compact service phase. Try10 preserved final loop `II=1`
and produced `9646 LUT`, `4356 FF`, and `2 DSP`. The estimated clock improved
from `8.510 ns` to `7.287 ns`, and the top-level timing estimate improved from
`-1.21` slack to `+0.01`.

The full compact-service-phase configs are milestone checkpoints, not the normal
per-try synthesis target. They restore the full numeric subsystem while keeping
the current staged `CALL` and compact reducer service controller enabled. This
answers a different question from the FPGA-small runs: whether the improved
runtime controller still helps when complete numeric semantics are present, or
whether the synthesis result is again dominated by `F24`, division, remainder,
`fmod`, and full numeric comparison hardware.

Try12 ran this checkpoint. Full-profile CSIM passed, and full-profile
no-statistics CSYNTH completed at `18061 LUT`, `9785 FF`, `12 DSP`, and main
reducer-loop `II=1`, with estimated Fmax `137.04 MHz`. This is materially
smaller and cleaner than the early try1 full staged/no-statistics reference
(`48457 LUT`, `32215 FF`, `49 DSP`, `II=67`). The report still shows the numeric
subsystem as the largest area source: `apply_numeric_operation` alone accounts
for `9175 LUT`, `5306 FF`, and `10 DSP`.

Try13 isolates one part of that numeric cost. The
`ENABLE_NUMERIC_F24_REMAINDER` gate defaults to enabled, so the normal full
profile still supports F24 remainder. With
`ENABLE_NUMERIC_F24_REMAINDER=0`, full compact-service synthesis completed at
`14017 LUT`, `7902 FF`, `7 DSP`, and main-loop `II=1`. The complete reduction
from try12 (`-4044 LUT`, `-1883 FF`, `-5 DSP`) is explained by removing the
floating-point `fmod` hardware from `apply_numeric_operation`; integer
division/remainder and other F24 operations remain enabled.

For Chapter 4, this split should be reported explicitly. FPGA-small CSYNTH is
the clean measurement for queue/control/runtime architecture changes. Full
CSYNTH is a milestone reference for the complete semantic profile.

Try14 continues the full-profile numeric-resource investigation. With
`ENABLE_SHARED_INTEGER_DIV_REM=1` and `ENABLE_NUMERIC_F24_REMAINDER=0`, the
full compact-service synthesis completed at `13333 LUT`, `6965 FF`, `9 DSP`,
and main-loop `II=1`. HLS generated one signed divider, one signed remainder
unit, one unsigned divider, and one unsigned remainder unit, instead of
duplicating those modules for flipped and non-flipped operations. Compared with
try13, this saved `684 LUT` and `937 FF`, but increased DSP use by `2` because
HLS bound F24 add/sub differently in this variant. This makes try14 an optional
LUT/FF-saving policy, not a default replacement for the try13 resource profile.

Try15 resolves that DSP tradeoff with an explicit HLS binding policy. With
`ENABLE_F24_ADD_SUB_FABRIC_BIND=1` added to the try14 policy set, synthesis
completed at `13484 LUT`, `6900 FF`, `7 DSP`, and main-loop `II=1`. The report
shows `faddfsub_32ns_32ns_32_6_no_dsp_1` inside
`apply_numeric_operation`, so F24 add/sub stays enabled but no longer consumes
the extra two DSPs. Relative to try13, try15 saves `533 LUT` and `1002 FF`
without increasing DSP. Relative to try14, it spends `151 LUT` to recover
`2 DSP` and save `65 FF`.

## Suggested Diploma Wording

The reducer is evaluated in two profiles. The full simulation profile preserves
the complete prototype semantics, including floating-point and high-cost numeric
operations. The FPGA-small profile preserves the interaction-combinator runtime
architecture but disables selected expensive numeric datapaths so that synthesis
results on a small Artix-7 device reflect the runtime structure rather than
being dominated by floating-point, division, and remainder units.

The FPGA-small profile should not be described as a different calculus or a
different reducer. It is a synthesis-oriented subset of the numeric subsystem
used to study hardware feasibility.

## Parallel Scheduler Try34 Note

Try34 adds a focused split-refill synthesis profile:

```text
hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_nostats.cfg
```

This profile keeps the try31/try32 selected-command staging and disables the
try33 pending-status path. It enables `ENABLE_PARALLEL_SPLIT_REFILL`, which
splits local-lane refill detection from global queue pop/count mutation. The
profile is a synthesis probe for the experimental parallel reducer; it is not a
replacement semantic profile.

Full-profile CSIM and FPGA-small CSIM passed. Focused CSYNTH completed at
`4680 LUT`, `2118 FF`, `0 DSP`, `0 BRAM`, scheduler `II=1`, and `-4.11` slack.
This is a negative timing result: refill splitting reduced FF usage but
worsened timing versus try32 and try33. The evidence suggests the next queue
experiment must make the global pop datapath a true single syntactic service
instead of priority-coding two pop call sites.

Try35 adds the follow-up focused profile:

```text
hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_nostats.cfg
```

This enables `ENABLE_PARALLEL_SINGLE_REFILL_POP` on top of the try34 refill
split. The profile captures one `refill_target_lane`, performs exactly one
global queue pop in REFILL_POP, then routes the result to the selected lane.
Focused CSYNTH completed at `4003 LUT`, `2114 FF`, `0 DSP`, `0 BRAM`,
scheduler `II=1`, and `-2.05` slack. It is still not timing-clean, but it is
the best focused parallel scheduler evidence so far and shows that HLS-visible
single-owner queue services matter.

Try36 adds a selected-command completion-service probe:

```text
hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_completion_nostats.cfg
```

This enables `ENABLE_PARALLEL_SELECTED_COMMAND_COMPLETION` on top of the try35
profile. Successful selected-command execution records completion metadata, then
a separate service increments `executed_steps` and lane commit counters.
Focused CSYNTH completed at `4091 LUT`, `2117 FF`, `0 DSP`, `0 BRAM`,
scheduler `II=1`, and `-2.86` slack. This is worse than try35. The report shows
that `executed_steps` is no longer the visible limiter, but the critical path
moves into selected-command produced-pair/status routing and
`parallel_service_phase` selection. Therefore this completion-service gate is
kept as documented evidence, not as the next preferred focused profile.

Try37 adds a compact produced-pair-buffer profile:

```text
hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_nostats.cfg
```

This enables `ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER` on top of the try35
profile and leaves the try36 completion service disabled. Because the focused
parallel profile disables `COMMUTE`, `OPERATE`, `SWITCH`, and `CALL`, each
measured interaction emits at most two produced pairs. The profile therefore
narrows `MAXIMUM_PRODUCED_ACTIVE_PAIRS_PER_INTERACTION` to `2` and the
produced-pair counter to `2` bits only for that synthesis probe. Focused CSYNTH
completed at `3998 LUT`, `1148 FF`, `0 DSP`, `0 BRAM`, scheduler `II=1`, and
`-2.00` slack. This is not timing-clean, but it is the best focused parallel
scheduler result so far and shows that profile-specific state-width reduction
is as important as profile-specific operation gating.
