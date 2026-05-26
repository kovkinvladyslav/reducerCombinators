# Try 14: Shared Integer Div/Rem Operand Selection

Current status: `csynth_passed`

Branch: `try14`

Architecture: full-profile numeric-resource policy probe that shares flipped and non-flipped integer div/rem datapaths

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Integer div/rem sharing try recorded before editing runtime code. |
| 2026-05-26 | `implemented` | Added `ENABLE_SHARED_INTEGER_DIV_REM` and full-profile no-F24-rem configs. |
| 2026-05-26 | `csim_passed` | Short-workdir Vitis CSIM passed with `csim_t14`. |
| 2026-05-26 | `csynth_passed` | Full-profile no-statistics CSYNTH completed with `csyn_t14`. |

## Goal

Reduce the remaining full-profile numeric area after try13 by targeting duplicated
integer divide and remainder operators in `apply_numeric_operation`.

## Hypothesis

Try13 removed F24 `fmod`, but the HLS report still instantiated two modules for
each integer direction:

- `sdiv_24s_24s_24_28_seq_1`: 2 instances;
- `srem_24s_24s_24_28_seq_1`: 2 instances;
- `udiv_24s_24s_24_28_seq_1`: 2 instances;
- `urem_24s_24s_24_28_seq_1`: 2 instances.

The duplicated instances come from separate flipped and non-flipped switch
cases. If the reducer first selects numerator and denominator, then performs a
single division or remainder expression for each type, HLS may bind one module
per operation/signedness instead of two.

## Difference From Prior Tries

Try13 isolated floating-point remainder. Try14 keeps F24 remainder disabled to
avoid reintroducing `fmod`, then targets integer div/rem duplication. It does
not change the compact service controller, staged `CALL`, queue ownership, or
default full-profile semantics.

## Implementation Plan

- Add `ENABLE_SHARED_INTEGER_DIV_REM`, default `0`.
- When enabled, handle U24/I24 divide and remainder before the main operation
  switch:
  - choose numerator and denominator based on flipped/non-flipped operation tag;
  - check the selected denominator for zero;
  - perform one `/` or `%` expression per type/op.
- Keep the old switch cases when the gate is disabled, preserving default
  source behavior.
- Add full compact-service configs that enable:
  - `ENABLE_NUMERIC_F24_REMAINDER=0`;
  - `ENABLE_SHARED_INTEGER_DIV_REM=1`.
- Compare against try13 no-F24-rem synthesis, not try12, so the measured delta
  belongs to integer div/rem sharing.

## Validation Commands

Syntax check:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -DENABLE_COMPACT_SERVICE_PHASE=1 -DENABLE_NUMERIC_F24_REMAINDER=0 -DENABLE_SHARED_INTEGER_DIV_REM=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full-profile CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_full_compact_service_phase_shared_int_div_rem_no_f24_rem.cfg --work_dir csim_t14
```

Full-profile no-statistics CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_full_compact_service_phase_shared_int_div_rem_no_f24_rem_nostats.cfg --work_dir csyn_t14
```

## Results

- Host syntax: passed for the try14 full-profile flags.
- Default full syntax: passed with compact service phase and the gate left at
  its default value.
- Full CSIM: passed with `csim_t14`.
- FPGA-small CSIM: not the focus of this try.
- CSYNTH: passed with `csyn_t14`.
- Tooling note: Vitis compiled but failed to launch `.\csim.exe` with
  `invalid argument` for longer/default work directory names. The same config
  passed with the short `csim_t14` work directory, so this is recorded as a
  Windows/Vitis launch-path issue rather than a reducer mismatch.

## HLS Evidence

- Work directory: `C:\diploma\interactReduce\csyn_t14`
- Top report: `C:\diploma\interactReduce\csyn_t14\hls\syn\report\csynth.rpt`
- Numeric report: `C:\diploma\interactReduce\csyn_t14\hls\syn\report\apply_numeric_operation_csynth.rpt`
- LUT: `13333`
- FF: `6965`
- DSP: `9`
- BRAM: `0`
- Main reducer loop II: `1`
- Top slack: `0.01`
- Main reducer pipeline slack: `2.01`
- `apply_numeric_operation`: `4562 LUT`, `2458 FF`, `7 DSP`, latency max
  `27`, interval max `27`
- Integer div/rem modules: one each of
  `sdiv_24ns_24ns_24_28_seq_1`, `srem_24ns_24ns_24_28_seq_1`,
  `udiv_24ns_24ns_24_28_seq_1`, and `urem_24ns_24ns_24_28_seq_1`
- `generic_fmod`: absent

Compared with try13 full no-F24-rem synthesis:

| Build | LUT | FF | DSP | Main loop II |
| --- | ---: | ---: | ---: | ---: |
| Try13 full no F24 remainder | 14017 | 7902 | 7 | 1 |
| Try14 shared integer div/rem | 13333 | 6965 | 9 | 1 |

The top-level delta is `-684 LUT`, `-937 FF`, and `+2 DSP`.

For the numeric helper:

| Build | Numeric LUT | Numeric FF | Numeric DSP |
| --- | ---: | ---: | ---: |
| Try13 full no F24 remainder | 5131 | 3423 | 5 |
| Try14 shared integer div/rem | 4562 | 2458 | 7 |

The numeric-helper delta is `-569 LUT`, `-965 FF`, and `+2 DSP`.

## Conclusion

Try14 succeeded as a mixed resource-policy result. Selecting integer div/rem
operands before the operator expression gives HLS one signed divider, one
signed remainder unit, one unsigned divider, and one unsigned remainder unit
instead of duplicated flipped/non-flipped instances. This reduces LUT and FF
pressure while preserving `II=1`.

The tradeoff is DSP: the report increased from `7` to `9` DSP at top level.
The added DSPs are not from integer division itself; they come from HLS binding
F24 add/sub as a `faddfsub` full-DSP instance in this variant. Therefore try14
is not an unconditional replacement for try13. It is useful when LUT/FF pressure
is more important than DSP pressure.

## Next Recommendation

Keep `ENABLE_SHARED_INTEGER_DIV_REM` as an explicit optional policy rather than
turning it on by default. If the next goal is minimizing DSP, prefer try13's
full no-F24-rem profile. If the next goal is reducing LUT/FF under a fixed full
numeric subset, try14 is the better reference. A useful next try would force or
isolate F24 add/sub binding so the project can see whether try14's DSP increase
can be avoided without losing the integer div/rem sharing benefit.
