# CALL Architecture Notes

## Current Evidence

`CALL` is implemented functionally through the packed book representation. The
full reducer can expand a `REF` by copying packed book nodes into live node
memory, remapping local variable and node references, pushing internal book
redexes, and finally linking the instantiated root to the call argument.

The synthesis evidence shows that this behavior is too large for the reducer hot
loop when implemented as one inlined rule body:

- full and FPGA-small inline-`CALL` synthesis attempts stalled after HLS
  performance transformations;
- the FPGA-small no-statistics/no-`CALL` control completed synthesis;
- that control reported about `4202 LUT`, `2283 FF`, `2 DSP`, and main-loop
  `II=4` on `xc7a35t-cpg236-1`.
- a full-profile inline/no-statistics synthesis attempt timed out before HW
  transforms after reaching `82891` compile/link instructions and `8851`
  post-Performance instructions.

The useful conclusion is not that `CALL` is semantically wrong. The useful
conclusion is that packed-book instantiation is a different hardware service
from constant-size interaction rewrites such as `LINK`, `ERASE`, or
`ANNIHILATE`.

## Why Inline CALL Is Expensive

The inline `apply_call_rule` path performs several bounded but multi-element
operations inside the same reducer loop body:

- reads the definition header;
- validates book node/redex ranges;
- initializes fresh substitution variables;
- copies up to `MAXIMUM_BOOK_NODE_COUNT_PER_DEFINITION` packed nodes;
- remaps local book variables and node addresses into runtime addresses;
- pushes up to `MAXIMUM_BOOK_REDEX_COUNT_PER_DEFINITION` internal redexes;
- pushes the adjusted root/application redex.

For CSIM this is ordinary control flow. For HLS this forces a graph
instantiation datapath, several memory services, and queue mutation into the
same pipelined scheduler region as the small interaction rules.

## Try1 Architecture

Try1 keeps the existing inline implementation as the full-profile reference and
adds a staged FPGA-small path controlled by `ENABLE_STAGED_CALL`.

The staged path treats `CALL` as a multi-cycle service:

```text
pop REF active pair
  |
  v
validate definition and capacity
  |
  v
initialize fresh variables, one per service tick
  |
  v
copy adjusted book nodes, one per service tick
  |
  v
push adjusted book redexes, one per service tick
  |
  v
push adjusted root/application redex
```

The reducer still counts the completed `CALL` as one interaction step. Internal
service ticks do not increment `performed_steps`.

## Full vs FPGA-Small Evidence

Try1 was first measured in the FPGA-small profile because that is the profile
intended for Artix-7 feasibility. The full profile remains useful as a semantic
reference, but it intentionally keeps expensive numeric datapaths.

Measured synthesis outcomes:

| Build | Result | LUT | FF | DSP | II |
| --- | --- | ---: | ---: | ---: | ---: |
| FPGA-small no-`CALL` control | completed | 4202 | 2283 | 2 | 4 |
| FPGA-small staged `CALL` | completed | 7804 | 3254 | 2 | 4 |
| Full inline `CALL`, no stats | timed out before HW transforms | n/a | n/a | n/a | n/a |
| Full staged `CALL`, no stats | completed but too large for xc7a35t | 48457 | 32215 | 49 | 67 |

This comparison is important: staging fixes the `CALL` synthesis structure, but
the full profile is still dominated by F24, division, remainder, and `fmod`.
That is why the FPGA-small profile remains the correct implementation target for
resource-feasible hardware experiments.

## HVM/HVM2 Comparison Angle

For diploma comparison, the important point is architectural rather than
syntactic. HVM/HVM2-style references are runtime graph expansion points. In
software, expansion can be a normal function call over dynamic memory. In this
HLS reducer, expansion must be represented as a bounded hardware service with
explicit memory traffic, queue effects, and allocation counters.

This distinction gives a clear Chapter 3/4 story:

- the interaction-combinator rules form the small reducer datapath;
- `CALL`/`REF` expansion is a graph-instantiation service;
- FPGA feasibility depends on separating these roles enough for HLS to schedule
  memories and control paths.

## Diagram Material

Useful diagrams to prepare later:

- reducer hot loop with direct rules and staged `CALL` request;
- `CALL` service finite-state machine;
- book memory to live graph memory remapping;
- queue effects: popped `REF`, internal redex pushes, root redex push;
- comparison table: inline `CALL` attempt vs no-`CALL` control vs staged `CALL`.
