# Try N: Title

Current status: `planned`

Branch: `tryN`

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| YYYY-MM-DD | `planned` | Initial architecture plan recorded. |

## Goal

Describe the runtime/HLS architecture problem this try is meant to solve.

## Hypothesis

State the concrete hardware hypothesis. Example: separating a multi-cycle
service from the hot reducer loop will reduce HLS scheduling pressure compared
with inlining the same behavior into the pipelined loop.

## Difference From Prior Tries

Explain why this is not a repeat of any earlier `try*.md`.

## Implementation Plan

Describe the intended architecture, compile-time gates, interfaces, and any
semantic constraints that must be preserved.

## Validation Commands

List exact commands for syntax checks, host tests, CSIM, and CSYNTH.

## Results

Fill this after implementation.

- Host syntax:
- Host execution:
- Full CSIM:
- FPGA-small CSIM:
- CSYNTH:

## HLS Evidence

Fill this when reports or logs exist.

- Work directory:
- Report paths:
- LUT:
- FF:
- DSP:
- BRAM:
- II:
- Timing/slack:
- Last Vitis log line if frozen:

## Conclusion

State whether the try succeeded, failed, or should be superseded.

## Next Recommendation

Describe what the next try should do differently.

