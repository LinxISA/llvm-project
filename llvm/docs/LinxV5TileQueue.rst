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
