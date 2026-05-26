# Try 15: Fabric-Bound F24 Add/Sub With Shared Integer Div/Rem

Current status: `csynth_passed`

Branch: `try15`

Architecture: full-profile numeric binding policy probe for F24 add/sub after
try14 shared integer div/rem

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try14 saved LUT/FF but increased DSP because HLS bound F24 add/sub to a full-DSP implementation. |
| 2026-05-26 | `implemented` | Added `ENABLE_F24_ADD_SUB_FABRIC_BIND` and full-profile shared-int/no-F24-rem configs. |
| 2026-05-26 | `csim_passed` | Host executable built with try15 flags and reported `Overall Result: PASS`; Vitis generated CSIM executable but Windows Application Control blocked direct execution. |
| 2026-05-26 | `csynth_passed` | Full-profile no-statistics CSYNTH completed with `csyn_t15`. |

## Goal

Test whether try14's integer div/rem sharing can keep its LUT/FF reduction
without the `+2 DSP` tradeoff by explicitly steering F24 add/sub away from
DSP-based binding.

## Hypothesis

Try14 proved that selecting integer divide/remainder operands before applying
`/` or `%` removes duplicated signed and unsigned div/rem modules. However, the
same report increased top-level DSP from `7` to `9` because HLS selected a
`faddfsub_32ns_32ns_32_7_full_dsp_1` implementation for F24 add/sub.

If F24 add/sub is explicitly bound to fabric while keeping
`ENABLE_SHARED_INTEGER_DIV_REM=1` and `ENABLE_NUMERIC_F24_REMAINDER=0`, the
design may preserve try14's LUT/FF savings and return DSP usage closer to
try13.

## Difference From Prior Tries

Try13 disabled only F24 remainder. Try14 shared integer div/rem operand
selection and accepted the resulting DSP tradeoff. Try15 does not add another
numeric semantic gate. It tests an HLS binding policy for an already-supported
F24 operation, so the semantic behavior should remain the same while the
operator implementation changes.

## Implementation Plan

- Add `ENABLE_F24_ADD_SUB_FABRIC_BIND`, default `0`.
- When enabled in the F24 branch of `apply_numeric_operation`, apply HLS
  `BIND_OP` pragmas to the F24 result variable for `fadd` and `fsub` with
  fabric implementation.
- Add full compact-service configs that enable:
  - `ENABLE_NUMERIC_F24_REMAINDER=0`;
  - `ENABLE_SHARED_INTEGER_DIV_REM=1`;
  - `ENABLE_F24_ADD_SUB_FABRIC_BIND=1`.
- Preserve default full-profile source behavior when the gate is disabled.
- Compare against try13 and try14:
  - try13: lower DSP, higher LUT/FF;
  - try14: lower LUT/FF, higher DSP.

## Validation Commands

Syntax check:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -DENABLE_COMPACT_SERVICE_PHASE=1 -DENABLE_NUMERIC_F24_REMAINDER=0 -DENABLE_SHARED_INTEGER_DIV_REM=1 -DENABLE_F24_ADD_SUB_FABRIC_BIND=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full-profile CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_full_compact_service_phase_f24_addsub_fabric_shared_int_no_f24_rem.cfg --work_dir csim_t15
```

Full-profile no-statistics CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_full_compact_service_phase_f24_addsub_fabric_shared_int_no_f24_rem_nostats.cfg --work_dir csyn_t15
```

## Results

- Host syntax: passed with the try15 full-profile flags.
- Host execution: passed. The `g++`-built `try15_host_test.exe` reported
  `Overall Result: PASS`.
- Full CSIM: Vitis compiled `csim.exe`, then failed to launch it with
  `couldn't execute ".\csim.exe": invalid argument`; direct execution was
  blocked by Windows Application Control. This is recorded as a local tool
  launch/policy issue. The same source and flags passed through the host
  executable.
- FPGA-small CSIM: not the focus of this try.
- CSYNTH: passed with `csyn_t15`.

## HLS Evidence

- Work directory: `C:\diploma\interactReduce\csyn_t15`
- Top report: `C:\diploma\interactReduce\csyn_t15\hls\syn\report\csynth.rpt`
- Numeric report: `C:\diploma\interactReduce\csyn_t15\hls\syn\report\apply_numeric_operation_csynth.rpt`
- LUT: `13484`
- FF: `6900`
- DSP: `7`
- BRAM: `0`
- Main reducer loop II: `1`
- Top slack: `0.01`
- Main reducer pipeline slack: `2.01`
- `apply_numeric_operation`: `4713 LUT`, `2393 FF`, `5 DSP`, latency max
  `27`, interval max `27`
- F24 add/sub binding: `faddfsub_32ns_32ns_32_6_no_dsp_1`, with the report
  marking the `BIND_OP` pragmas as applied.
- Integer div/rem modules: one each of
  `sdiv_24ns_24ns_24_28_seq_1`, `srem_24ns_24ns_24_28_seq_1`,
  `udiv_24ns_24ns_24_28_seq_1`, and `urem_24ns_24ns_24_28_seq_1`
- `generic_fmod`: absent

Top-level comparison:

| Build | LUT | FF | DSP | Main loop II |
| --- | ---: | ---: | ---: | ---: |
| Try13 full no F24 remainder | 14017 | 7902 | 7 | 1 |
| Try14 shared integer div/rem | 13333 | 6965 | 9 | 1 |
| Try15 shared int + F24 add/sub fabric | 13484 | 6900 | 7 | 1 |

Compared with try13, try15 saves `533 LUT` and `1002 FF` with no DSP increase.
Compared with try14, try15 spends `151 LUT`, saves `65 FF`, and saves `2 DSP`.

Numeric-helper comparison:

| Build | Numeric LUT | Numeric FF | Numeric DSP |
| --- | ---: | ---: | ---: |
| Try13 full no F24 remainder | 5131 | 3423 | 5 |
| Try14 shared integer div/rem | 4562 | 2458 | 7 |
| Try15 shared int + F24 add/sub fabric | 4713 | 2393 | 5 |

## Conclusion

Try15 succeeded as the best current full-profile numeric-resource policy among
tries 13-15. It keeps the try14 architectural benefit of sharing integer
flipped/non-flipped divide and remainder datapaths, but removes the accidental
DSP increase by binding F24 add/sub to fabric.

The tradeoff is small and explicit: compared with try14, fabric F24 add/sub
costs `151 LUT` but saves `2 DSP` and `65 FF`. Compared with try13, try15 keeps
the same top-level DSP count while reducing both LUT and FF.

This is useful for the diploma because it shows that numeric resource policy is
not only about disabling operations. Some complete-profile operations can stay
semantically enabled while HLS binding choices decide whether the cost appears
as LUT/FF or DSP.

## Next Recommendation

Keep `ENABLE_F24_ADD_SUB_FABRIC_BIND` optional and off by default, but use the
try15 config as the preferred full-profile no-F24-rem synthesis reference when
DSP must not increase over try13.

The next architecture try should return to runtime structure rather than
continue piling numeric binding policies. A useful direction is the memory
strategy called out in the original plan: document and then prototype a bounded
node-memory banking or redex-queue service experiment that can be measured
against the current compact-service `II=1` baseline without changing
interaction semantics.
