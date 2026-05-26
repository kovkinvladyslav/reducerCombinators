# Try 13: F24 Remainder Resource Gate

Current status: `csynth_passed`

Branch: `try13`

Architecture: full-profile numeric-resource policy probe that isolates floating-point remainder (`fmod`)

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Numeric-resource policy try recorded before editing runtime code. |
| 2026-05-26 | `csim_passed` | Default full CSIM and no-F24-rem full CSIM both passed. |
| 2026-05-26 | `csynth_passed` | No-F24-rem full no-statistics CSYNTH completed and produced `csynth.rpt`. |

## Goal

Reduce full-profile synthesis cost by isolating one expensive numeric path:
F24 remainder, which maps to `std::fmod` and generated `generic_fmod_mult_float`
hardware in try12.

## Hypothesis

Try12 showed the runtime controller is now clean (`II=1`) even in the full
profile, but `apply_numeric_operation` still accounts for `9175 LUT`,
`5306 FF`, and `10 DSP`. The report explicitly generated
`generic_fmod_mult_float_s` and a fmod pipeline. A narrow compile-time gate for
only F24 remainder should remove the floating-point fmod hardware while keeping
integer division/remainder and other F24 operations available.

## Difference From Prior Tries

This is not a queue/control try and does not repeat the FPGA-small profile.
FPGA-small disables F24, division, and remainder together. Try13 keeps the full
numeric profile as the default and adds one narrower synthesis policy:
`ENABLE_NUMERIC_F24_REMAINDER=0`.

This separates a specific floating-point resource question from the broader
runtime architecture work.

## Implementation Plan

- Add `ENABLE_NUMERIC_F24_REMAINDER`.
- Default it to `ENABLE_NUMERIC_F24 && ENABLE_NUMERIC_REMAINDER`, so existing
  full-profile behavior is unchanged.
- In `apply_numeric_operation`, reject F24 remainder operations with
  `REDUCER_STATUS_INVALID_NUMERIC_OPERATION` only when this new gate is disabled.
- Keep U24/I24 remainder behavior controlled by `ENABLE_NUMERIC_REMAINDER`.
- Add a targeted F24 remainder test whose expectation follows the new gate.
- Add full compact-service CSIM and CSYNTH configs with:
  - full profile,
  - compact-service architecture gates from try12,
  - statistics disabled for synthesis,
  - `-DENABLE_NUMERIC_F24_REMAINDER=0`.

## Validation Commands

Syntax check:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -DENABLE_COMPACT_SERVICE_PHASE=1 -DENABLE_NUMERIC_F24_REMAINDER=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full-profile CSIM with F24 remainder disabled:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_full_compact_service_phase_no_f24_rem.cfg --work_dir interactReduce_csim_full_no_f24_rem_try13
```

Full-profile no-statistics CSYNTH with F24 remainder disabled:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_full_compact_service_phase_no_f24_rem_nostats.cfg --work_dir interactReduce_csynth_full_no_f24_rem_try13
```

## Results

- Host syntax: passed for default full compact-service flags, no-F24-rem full flags, and FPGA-small compact-service flags.
- Host execution: not run separately; Vitis CSIM is the executable validation.
- Full CSIM:
  - default full compact-service CSIM passed, including `F24 REM profile` as a supported operation.
  - no-F24-rem full compact-service CSIM passed, with `F24 REM profile` returning `REDUCER_STATUS_INVALID_NUMERIC_OPERATION`.
- FPGA-small CSIM: not the focus of this try.
- CSYNTH: passed for `hls_config_synth_full_compact_service_phase_no_f24_rem_nostats.cfg`.

## HLS Evidence

- Work directory: `C:\diploma\interactReduce\interactReduce_csynth_full_no_f24_rem_try13`
- Report paths:
  - `C:\diploma\interactReduce\interactReduce_csynth_full_no_f24_rem_try13\hls\syn\report\csynth.rpt`
  - `C:\diploma\interactReduce\interactReduce_csynth_full_no_f24_rem_try13\hls\syn\report\apply_numeric_operation_csynth.rpt`
- LUT: `14017`
- FF: `7902`
- DSP: `7`
- BRAM: reported as `-` at top level because top memories are external `ap_memory` interfaces.
- II: main reducer pipeline `VITIS_LOOP_169_1` final `II=1`; all loop constraints satisfied.
- Timing/slack: Vitis reported estimated Fmax `137.23 MHz`; top module summary slack `0.01`, main reducer pipeline slack `2.01`.
- Numeric evidence:
  - `apply_numeric_operation` is reduced to `5131 LUT`, `3423 FF`, and `5 DSP`.
  - The report still includes F24 add/mul/div/compare and integer div/rem modules.
  - No report entry matched `generic_fmod`, confirming the floating-point fmod module was removed.
- Comparison with try12:
  - top-level delta: `-4044 LUT`, `-1883 FF`, `-5 DSP`;
  - `apply_numeric_operation` delta: `-4044 LUT`, `-1883 FF`, `-5 DSP`;
  - main-loop `II` remains `1`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try13 succeeded as a numeric-resource policy experiment. It shows that F24
remainder is a separable synthesis cost, not an unavoidable part of supporting
all F24 arithmetic. Disabling only F24 remainder removes the `generic_fmod`
hardware and accounts for the full top-level resource delta versus try12.

This is useful diploma evidence because it separates three ideas:

- the interaction-combinator runtime controller is stable and remains `II=1`;
- the default full semantic profile is preserved and still supports F24
  remainder;
- a synthesis-oriented numeric policy can remove one expensive operation with a
  clear status behavior.

## Next Recommendation

The next numeric try should decide whether to keep this as an optional
synthesis profile or go one level deeper into numeric service staging. Good
next candidates are:

- a named `FULL_NO_F24_REMAINDER_PROFILE`/policy config for documentation
  clarity;
- a full-profile integer div/rem sharing experiment, since the report still
  instantiates duplicated `sdiv/srem/udiv/urem` modules;
- a staged numeric service for divide/remainder if preserving every operation
  in one profile becomes more important than single-cycle operation dispatch.
