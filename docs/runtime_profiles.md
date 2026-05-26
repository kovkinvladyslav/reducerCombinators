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
| `hls_config_fpga_small.cfg` | Single-lane reducer with `FPGA_SMALL_PROFILE` enabled |
| `hls_config_synth_full.cfg` | Lean full-profile synthesis config |
| `hls_config_synth_fpga_small.cfg` | Lean FPGA-small synthesis config |
| `hls_config_synth_full_nostats.cfg` | Full-profile synthesis probe with statistics disabled |
| `hls_config_synth_full_staged_call_nostats.cfg` | Full-profile synthesis probe with statistics disabled and staged `CALL` enabled |
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
