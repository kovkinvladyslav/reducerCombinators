# Try 16: Parallel Bank-Conflict Instrumentation

Current status: `csynth_passed`

Branch: `try16`

Architecture: non-stalling memory-bank conflict counters for the experimental
two-lane reducer

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try15 completed the numeric-resource line; next useful step is memory instrumentation before implementing real banking. |
| 2026-05-26 | `implemented` | Added substitution/node bank constants, non-stalling bank conflict counters, reporting columns, and targeted parallel tests. |
| 2026-05-26 | `csim_passed` | Host executable passed with `Overall Result: PASS`; Vitis CSIM generated `csim.exe` but failed to launch it in this Windows environment. |
| 2026-05-26 | `csynth_passed` | Focused FPGA-small/no-`CALL` parallel CSYNTH completed with `csyn_t16_parallel_small`; default full parallel CSYNTH timed out after Performance transforms. |

## Goal

Prepare the experimental parallel reducer for future memory banking by
measuring bank-level conflicts separately from true semantic conflicts.

## Hypothesis

The current parallel scheduler already detects exact substitution-variable
conflicts, exact binary-node-address conflicts, and allocation contention. That
is sufficient for correctness, but it does not show whether future memory banks
would actually help two-lane execution.

If the scheduler records same-bank/different-address conflicts without stalling
the lanes, then tests and benchmarks can distinguish:

- true semantic conflicts, which must serialize;
- future memory-bank conflicts, which depend on the banking strategy;
- independent work, which can dual-commit today.

## Difference From Prior Tries

Tries 2-11 changed queue ownership and single-lane control structure. Tries
12-15 measured full-profile numeric resource policy. Try16 does not change
single-lane execution, numeric behavior, staged `CALL`, or conflict decisions.
It adds measurement for a planned memory architecture: substitution banks and
node banks.

## Implementation Plan

- Add explicit bank-count constants:
  - `SUBSTITUTION_MEMORY_BANK_COUNT = 4`, matching the existing cyclic HLS
    partition factor;
  - `NODE_MEMORY_BANK_COUNT = 2`, matching the planned low-address-bit node
    banking probe.
- Add two parallel statistics counters:
  - `substitution_bank_conflict_count`;
  - `node_bank_conflict_count`.
- Add helper functions in `lane_interaction.hpp` that detect:
  - two independent `LINK` rules targeting different substitution variables in
    the same substitution bank;
  - two binary-node active pairs touching different node addresses in the same
    node bank.
- Record these counters in `lane_conflicts_with_committed_work`, but do not
  return conflict for bank-only matches.
- Add tests that prove:
  - same-bank/different-variable `LINK` pairs can still dual-commit while
    incrementing `substitution_bank_conflict_count`;
  - same-bank/different-node `ANNIHILATE` pairs can still dual-commit while
    incrementing `node_bank_conflict_count`.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Host execution:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src test\reducer_test.cpp src\reducer_core.cpp src\reducer_core_parallel.cpp -o try16_host_test.exe
.\try16_host_test.exe
```

Parallel CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_parallel.cfg --work_dir csim_t16_parallel
```

Optional parallel CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_parallel.cfg --work_dir csyn_t16_parallel
```

Focused parallel-bank-metric CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_bank_metrics.cfg --work_dir csyn_t16_parallel_small
```

## Results

- Host syntax: passed.
- Host execution: passed. `try16_host_test.exe` reported `Overall Result:
  PASS`.
- Full CSIM: not the focus of this try.
- Parallel CSIM: Vitis compiled `csim.exe`, then failed to launch it with
  `couldn't execute ".\csim.exe": invalid argument`; this matches earlier local
  Vitis launcher failures. Host execution covers correctness for the same
  testbench.
- CSYNTH:
  - default full `hls_config_parallel.cfg` timed out after 20 minutes; the last
    log line was truncated after `Array/Struct (step 4)`/Performance-size
    reporting, with only `csynth_design_size.rpt` generated;
  - focused FPGA-small no-`CALL` parallel-bank-metric CSYNTH completed with
    `csyn_t16_parallel_small`.

## HLS Evidence

- Focused work directory: `C:\diploma\interactReduce\csyn_t16_parallel_small`
- Top report:
  `C:\diploma\interactReduce\csyn_t16_parallel_small\hls\syn\report\csynth.rpt`
- LUT: `39085`
- FF: `51611`
- DSP: `2`
- BRAM: `4`
- Main scheduler loop: `VITIS_LOOP_300_2`
- Main scheduler loop II: `12`
- Violation type: `Memory Dependency`
- Top slack: `-9.93`
- Estimated Fmax: `58.05 MHz`
- Statistics port width: `1152` bits
- Inlined helpers include `link_rules_touch_same_substitution_bank` and
  `active_pairs_touch_same_node_bank`, proving the new counters are in the
  synthesized scheduler path.
- Full parallel timeout work directory:
  `C:\diploma\interactReduce\csyn_t16_parallel`
- Full parallel timeout evidence:
  `C:\diploma\interactReduce\csyn_t16_parallel\hls\syn\report\csynth_design_size.rpt`
- Last full-parallel log line before timeout:
  `INFO: [HLS 200-1995] There were 22,579 instructions in the design after the 'Array/Struct (step 4)' phase ...`

## Conclusion

Try16 succeeded as instrumentation and as negative synthesis evidence for the
current experimental parallel top.

The implementation adds memory-bank observability without changing
interaction-combinator semantics or the current conservative conflict gate.
The new tests prove that same-bank/different-address work still dual-commits:

- same-bank `LINK` variables increment `substitution_bank_conflict_count` while
  `substitution_conflict_count` remains zero;
- same-bank binary-node active pairs increment `node_bank_conflict_count` while
  `node_access_conflict_count` remains zero.

The focused parallel-small synthesis completes, but the result is not
resource-feasible on `xc7a35t`: `39085 LUT`, `51611 FF`, `2 DSP`, `4 BRAM`,
`II=12`, and `-9.93` slack. This confirms that the current parallel top is a
measurement platform, not a finished FPGA runtime. The useful result is that
future banking experiments now have counters to show whether bank conflicts are
actually present in workloads.

## Next Recommendation

Do not implement full node-memory banking immediately. First reduce the
parallel top's control/statistics footprint or add a narrow no-statistics
parallel synthesis mode that still preserves only the counters needed for bank
analysis. After that, use the new bank counters to choose between:

- substitution bank arbitration for same-bank `LINK` writes;
- two-bank node memory with explicit same-bank lane arbitration;
- keeping the current single-lane reducer as the diploma reference and
  presenting the parallel top as measured future work.
