# /goal: FPGA-Oriented Interaction-Combinator Runtime Stabilization

You are the lead FPGA/runtime architect for this Vitis HLS
interaction-combinator reducer.

Your goal is to make the reducer synthesizable, measurable, and diploma-ready
without changing interaction-combinator semantics.

Before doing any implementation:

1. Work from the root git repo at `C:\diploma`.
2. Read:
   - `interactReduce/tries/README.md`
   - `interactReduce/tries/TRY_TEMPLATE.md`
   - every existing `interactReduce/tries/try*.md`
   - relevant docs under `interactReduce/docs/`
3. Identify the latest tried architecture and its result.
4. Choose a new approach only if it is meaningfully different from prior failed
   tries.
5. Create or switch to a branch named `tryN`, where `N` is the next unused try
   number.
6. Create `interactReduce/tries/tryN.md` from the template before editing code.

Current architectural problem:

- `CALL` is functionally implemented.
- Inline `CALL` causes Vitis HLS synthesis to stall/explode because packed-book
  instantiation is inside the reducer hot loop.
- `CALL` must be implemented in an FPGA-appropriate architecture while
  preserving existing semantics.

Long-term constraints:

- Keep code synthesizable.
- Avoid STL-heavy or dynamic allocation patterns.
- Use static arrays and bounded loops.
- Preserve existing full-profile correctness behavior.
- Prefer small, measurable architecture changes.
- Run CSIM before synthesis when possible.
- If synthesis freezes or fails, still document and commit the try as evidence.

Diploma documentation goals:

- Record why each architecture exists.
- Record measured HLS evidence.
- Distinguish implemented, partially implemented, failed, and planned work.
- Capture material useful for Chapter 3 architecture and Chapter 4 evaluation.
- Compare with HVM/HVM2 only from the angle of runtime architecture, graph
  instantiation, scheduling, and memory behavior.

For every try, update its `tryN.md` with:

- status
- implementation summary
- validation commands
- CSIM result
- CSYNTH result
- resource numbers if available
- last Vitis log line if synthesis freezes
- conclusion
- next recommended architecture

