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
- Treat full-profile CSIM as the required semantic correctness guard for normal
  single-lane reducer changes.
- Treat FPGA-small CSYNTH as the primary architecture measurement until full
  numerics are deliberately targeted again. Full-profile CSYNTH is a reference
  measurement to run when the change touches full-only behavior or when a new
  milestone needs comparison against the complete semantic profile.

## CSIM And CSYNTH Policy

The architecture tries normally run full-profile CSIM, FPGA-small CSIM, and
FPGA-small CSYNTH. They do not normally repeat full-profile CSYNTH for every
queue, scheduler, or control-path change.

Reason: the full profile is intentionally semantic-complete, so synthesis is
dominated by known expensive numeric and runtime paths such as F24, division,
remainder, `fmod`, and full numeric datapaths. The recorded full staged/no-stats
reference from try1 completed at about `48457 LUT`, `32215 FF`, `49 DSP`, and
main-loop `II=67`. Repeating that synthesis for each small runtime restructuring
mostly re-measures the full numeric subsystem instead of the architecture change
being tested.

Use this validation split:

- Full CSIM: proves complete-profile semantics did not break.
- FPGA-small CSIM: proves the gated FPGA architecture path works.
- FPGA-small CSYNTH: measures the actual architecture experiment.
- Full CSYNTH: run only at milestones, or when a change touches full-only
  behavior such as full numeric operations, inline reference behavior, or a
  complete-profile top-level interface.

For the current try sequence, most changes target queue ownership, reducer
control, staged `CALL`, produced-pair buffering, and completion accounting. In
that context, FPGA-small CSYNTH is the clean synthesis signal, while full CSYNTH
is slow, noisy, and useful mainly as a milestone comparison.

## Diploma Use

The try logs are source material for architecture and evaluation chapters. They
should clearly distinguish:

- implemented behavior,
- partially implemented architecture,
- failed synthesis approaches,
- planned future work,
- measured FPGA evidence.
