# Architecture Try Log

This directory is the permanent experiment log for FPGA/runtime architecture
attempts in the interaction-combinator reducer.

Every substantial architecture attempt must have one `tryN.md` file. A failed
try is still useful research evidence if it records what was attempted, how it
was validated, and where Vitis HLS stopped or failed.

## Required Workflow

Before starting any new try:

1. Work from the root git repository at `C:\diploma`.
2. Read this file.
3. Read `GOAL.md`.
4. Read `TRY_TEMPLATE.md`.
5. Read every existing `try*.md`.
6. Identify the latest tried architecture and its result.
7. Choose a new approach only if it is meaningfully different from prior failed
   tries.
8. Create or switch to a branch named `tryN`, where `N` is the next unused try
   number.
9. Create `tryN.md` from the template before editing runtime code.

## Status Values

Use exactly one current status at the top of every `tryN.md`:

- `planned`: design chosen but not implemented.
- `implemented`: code written, not fully validated.
- `csim_passed`: host/CSIM validation passed.
- `csynth_passed`: synthesis completed and report exists.
- `failed_csim`: implementation failed correctness validation.
- `failed_csynth_freeze`: Vitis froze or stalled during synthesis.
- `failed_csynth_resources`: synthesis completed but resources/timing are unacceptable.
- `abandoned`: superseded by a better approach, with reason documented.

Each try must also include a short status history table so the experiment can be
reconstructed later.

## Evidence Rules

- Commit failed synthesis attempts if they contain useful evidence.
- Record the exact command used for CSIM or CSYNTH.
- Record the last visible Vitis log line if synthesis freezes.
- Record generated report paths and resource numbers when reports exist.
- Explain why the next try must be different.

## Diploma Use

The try logs are source material for architecture and evaluation chapters. They
should clearly distinguish:

- implemented behavior,
- partially implemented architecture,
- failed synthesis approaches,
- planned future work,
- measured FPGA evidence.

