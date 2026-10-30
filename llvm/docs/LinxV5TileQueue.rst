LinxV5 Local Tile queue identity
==============================

Compiler-generated Local Tile keep-alive copies are identity transports.
They preserve capacity, backing type, layout, physical columns, completed
valid rows and columns, and predicate basis where applicable. A clock-hand
name or vector size alone is not a complete descriptor.

These requirements apply to ordinary TileOP code as well as generic element
code. They do not depend on ``-linxv5-enable-generic-element``. Before
rewriting allocated registers to relative queue offsets, the backend snapshots
descriptor-bearing identities and checks the source generation and its age
on incoming CFG edges. A queue-alignment filler publishes a real copy of a
closed value, not a zero-mask no-op.

When a live-out reordering attempt fails, its retry drains only the remaining
live values. Replaying the old queue's holes can reproduce the same failed
ordering indefinitely. The final successful sequence still publishes every
interior and trailing hole required by the successor contract, using real
descriptor-bearing fillers. This does not enable absolute Local references
or bypass the descriptor and incoming-edge checks.

An ``IMPLICIT_DEF`` has no architectural queue publication. For a numeric
loop-carried value with a statically proven descriptor, the backend can refine
that undefined initial value to a zero-filled Tile of the same descriptor.
It does not reuse a different live reduction or accumulator as that value.
OPEN assembly parents and predicate carriers are not initialized this way.
Unresolved or conflicting descriptors are rejected rather than guessed.

The zero fill uses TEXPANDS with an omitted scalar binding. Nonzero scalars
use B.IOR.RegSrc0. Both object emission and textual assembly preserve the
physical columns independently of the valid columns.

For inline-assembly TCVT, BSTART names the source operation type and B.DATR
names the destination type. The result descriptor and any subsequent copy
use the destination type and independently derived destination capacity.

The normative contracts are ``asl/block/execution/BSTART.TMOV.asl``,
``asl/tile/model/state/descriptors.asl``, and
``asl/tile/tile-scalar-and-immediate/initialization/TEXPANDS.asl`` in pto-spec.
No model assertion or ISA legality rule is relaxed by this lowering.

PredicateCell logical composition
--------------------------------

U8 CUBE_M16/M32 TAND, TOR and TXOR derive destination identity from both
source identities. Two PredicateCells produce a PredicateCell with U8 basis,
independently of their comparison bases. A mixed Numeric source produces
Numeric U8. These identities cannot join merely because capacity and vector
register types match. Descriptor-bearing TMOV and B.SUBVIEW preserve basis.
A closed two-writer B.ASSEMBLE result has completed parent geometry, not the
first writer's columns. TSEL/TSELS publish Numeric results using the operation
type, independently of predicate basis. Runtime definedness, canonical 0/1
values, ZERO/MERGE and pinned-Q recovery rules remain unchanged.
