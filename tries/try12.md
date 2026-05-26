# Try 12: Full-Profile Compact-Service Checkpoint

Current status: `csynth_passed`

Branch: `try12`

Architecture: full semantic profile using the current compact service phase and staged `CALL`

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Full-profile checkpoint recorded before adding configs or running validation. |
| 2026-05-26 | `csim_passed` | Full-profile compact-service CSIM passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | Full-profile compact-service no-statistics CSYNTH completed and produced `csynth.rpt`. |

## Goal

Measure the current best single-lane reducer architecture under the full semantic
profile, instead of only the FPGA-small profile. This try exists to answer why
most previous CSYNTH runs used the small profile and to provide a milestone
comparison for the diploma evaluation chapter.

## Hypothesis

The compact service phase, produced-pair buffer, deferred completion accounting,
and staged `CALL` should preserve full-profile correctness. However, full-profile
CSYNTH is expected to remain dominated by expensive numeric datapaths (`F24`,
division, remainder, `fmod`, and full numeric comparisons), so resource use and
possibly `II` may be much worse than the FPGA-small result even when the runtime
controller itself is improved.

## Difference From Prior Tries

Tries 2 through 11 primarily measured queue ownership, staged services, and
control-path structure using FPGA-small CSYNTH. Try1 included an early full
staged/no-statistics reference, but it did not include the later queue/control
architecture improvements from tries 3 through 11.

This try is different because it deliberately restores the full numeric profile
while keeping the current compact-service architecture gates enabled. It is a
milestone synthesis checkpoint, not another small-profile scheduler tweak.

## Implementation Plan

- Add a full-profile CSIM config that enables:
  - staged `CALL`
  - produced-pair buffer
  - deferred completion accounting
  - selective small-rule inlining
  - compact completion state
  - compact service phase
- Add a full-profile no-statistics CSYNTH config with the same architecture
  gates.
- Do not define `FPGA_SMALL_PROFILE` in either config.
- Do not change reducer semantics or source code in this try unless validation
  exposes a real full-profile integration defect.
- Record whether full-profile synthesis completes, freezes, or returns to the
  old full-profile resource/II pattern.

## Validation Commands

Syntax check:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -DENABLE_COMPACT_SERVICE_PHASE=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full-profile CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_full_compact_service_phase.cfg --work_dir interactReduce_csim_full_compact_service_phase_try12
```

Full-profile compact-service no-statistics CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_full_compact_service_phase_nostats.cfg --work_dir interactReduce_csynth_full_compact_service_phase_try12
```

Reference full-profile staged/no-statistics CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_full_staged_call_nostats.cfg --work_dir interactReduce_csynth_full_staged_call_checkpoint
```

## Results

- Host syntax: passed with compact-service full-profile flags.
- Host execution: not run separately; Vitis CSIM is the executable validation.
- Full CSIM: passed. Vitis reported `Overall Result: PASS` and `CSim done with 0 errors`.
- FPGA-small CSIM: not the focus of this try; covered by try10 and try11.
- CSYNTH: passed for `hls_config_synth_full_compact_service_phase_nostats.cfg`.

## HLS Evidence

- Work directory: `C:\diploma\interactReduce\interactReduce_csynth_full_compact_service_phase_try12`
- Report paths:
  - `C:\diploma\interactReduce\interactReduce_csynth_full_compact_service_phase_try12\hls\syn\report\csynth.rpt`
  - `C:\diploma\interactReduce\interactReduce_csynth_full_compact_service_phase_try12\hls\syn\report\reducer_core_csynth.rpt`
  - `C:\diploma\interactReduce\interactReduce_csynth_full_compact_service_phase_try12\hls\syn\report\apply_numeric_operation_csynth.rpt`
- LUT: `18061`
- FF: `9785`
- DSP: `12`
- BRAM: reported as `-` at top level because top memories are external `ap_memory` interfaces.
- II: main reducer pipeline `VITIS_LOOP_169_1` final `II=1`; all loop constraints satisfied.
- Timing/slack: Vitis reported estimated Fmax `137.04 MHz`; top module summary slack `0.00`, main reducer pipeline slack `2.01`.
- Numeric evidence: `apply_numeric_operation` uses `9175 LUT`, `5306 FF`, and `10 DSP`. Vitis generated full numeric modules including `fdiv_32ns_32ns_32_16_no_dsp_1`, `fmul_32ns_32ns_32_4_max_dsp_1`, integer `sdiv/srem/udiv/urem`, and `generic_fmod_mult_float_s`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try12 succeeded as a full-profile milestone checkpoint. It directly answers why
the previous queue/control tries mostly used FPGA-small CSYNTH: FPGA-small is
the clean measurement for the runtime controller, while full CSYNTH measures the
controller plus the complete numeric subsystem.

The result is much better than the early try1 full staged/no-statistics
reference (`48457 LUT`, `32215 FF`, `49 DSP`, `II=67`). With the current compact
service architecture, the full profile completed at `18061 LUT`, `9785 FF`,
`12 DSP`, and reducer main-loop `II=1`. The full numeric subsystem is still the
largest area source, but it no longer prevents the reducer controller from
synthesizing cleanly.

## Next Recommendation

Keep FPGA-small as the primary per-try synthesis target for runtime-controller
experiments, but run full-profile CSYNTH at milestones. The next architecture
try should not repeat the full checkpoint. A useful next direction is a targeted
numeric-resource policy try: preserve full-profile CSIM semantics, then explore
whether synthesis-time numeric sharing or optional expensive-operation services
can reduce the `apply_numeric_operation` area without changing default
functional behavior.
