Element-region helper and predicate materialization limits
========================================================

An unsigned 32-bit result of a scalar comparison in an admitted 32- or
64-element region is materialized with GPR-predicate ``TSEL`` selecting
integer one or zero. This is a boolean-to-integer conversion, not an
integer address-index extension. The predicate is evaluated only in the
region's active domain; inactive values do not become unmasked memory
accesses.

Factoring a typed element loop into a helper currently requires inlining
that helper into the function owning its local Tile storage. Use
``__attribute__((always_inline)) inline`` for such helpers, and keep the
typed view and element pragma together inside the helper. Region ownership
is specialized per inlined loop instance. Tile producers must dominate
the region, and final typed publications must precede their consumers.

A ``noinline`` helper accepting Tile references does not establish distinct
local backing storage: references may alias, and separate calls do not
prove a callee's parameter storage, alignment, size, or publication lifetime.
The current compiler rejects accessed reference-backed views rather than
assuming those properties. Supporting this requires a separate interprocedural
Tile storage/ABI contract; removing the local-allocation check is not safe.

Ordinary Tile loads and element loops are not categorically incompatible.
Raw vector loads must be promoted to typed Tile publications before region
legalization; a remaining vector memory operation inside a region is not an
admitted scalar predicated access. Loop bounds, single-latch structure and
supported recurrence rules still apply after inlining.

Experimental PredicateCell options and 128/256/512-element typed integer
views from PR #146 are separate features, not implied by this conversion
support. They are integrated on the ``0.59`` branch; their absence on the
stable mainline does not establish an ISA violation. On ``0.59``, predicate
materialization uses Tile-predicate ``TSEL`` when the PredicateCell path is
selected, and GPR-predicate ``TSEL`` otherwise.
The full hash fixture and simulator timing require their pinned compiler,
API, model, and data inputs; compile-only regressions are not runtime evidence.
