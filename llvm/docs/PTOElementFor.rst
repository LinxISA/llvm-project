PTO Element ``for`` Regions
===========================

``#pragma pto element for`` marks an ordinary C or C++ ``for`` loop for the
required PTO element-region compiler.  Clang emits the normal LLVM CFG and SSA
for the loop body.  The pragma does not select a source-expression pattern and
does not expose a hardware lane, layout, predicate carrier, or temporary Tile
to the programmer.

Ordering contract
-----------------

Different logical element iterations are unordered and may execute in any
order or concurrently.  Operations within one logical element retain the
source-language order.  Programs must use atomics or another explicit
synchronization mechanism when different elements access conflicting storage.
This contract does not allow the compiler to combine effectful operations or
change their multiplicity merely because their operands are uniform.

Clang records the contract on the marked loop with these queryable properties:

* ``llvm.loop.linx.pto.element.inter_element_order = "unordered"``
* ``llvm.loop.linx.pto.element.intra_element_order = "source"``

The loop also carries ``llvm.loop.linx.pto.element.region`` and shares its
identity token with ``llvm.linx.experimental.element.region``.

Control-flow boundary
---------------------

The first frontend slice accepts ordinary ``if``, nested loops, ``switch``,
outer-loop ``continue``, and ``break`` or ``continue`` that target a nested
loop or switch.  It rejects ``break`` that exits the marked loop, function
``return``, and ``goto`` or labels that can cross the region boundary.  Nested
lambda bodies are separate function bodies and are not inspected as region
control flow.

These checks define the source-region boundary.  They do not imply that every
body operation already has PTO lowering.  Later LLVM legality passes diagnose
unsupported effects, types, memory dependencies, and Tile representations.
In particular, complete ``-O0`` lowering still has typed Tile spill/reload
limitations; raw Clang CFG and ordering metadata are available independently
of that backend limitation.

Generic predication IR stage
---------------------------

The explicit ``linx-v5-element-predication`` function pass widens a marked
ordinary LLVM loop into logical vector SSA and standard VP intrinsics.  It is
an independently testable stage under development, not yet part of the default
PTO instruction-selection pipeline.  For raw Clang CFG, use the existing LLVM
normalization passes before it::

  opt -mtriple=linx64v5 \
    -passes='mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication,verify' \
    -verify-each -S input.ll

The current stage accepts a SCEV-proven constant iteration count from 1 through
4096, a zero-based unit induction, and an iteration CFG that becomes acyclic
after removing the outer latch backedge.  The count is not fixed to a hardware
lane count.  The 4096 limit is an implementation bound for this development
stage, not an ISA limit.

Conditional edges use poison-safe ``select(parent_mask, condition, false)``.
Any number of incoming PHI edges are blended by their edge masks.  Integer
arithmetic, comparisons, selects, integer casts and scalar integer-element GEPs
are widened.  Loads and stores become masked ``llvm.vp.gather/scatter``; in
particular, an inactive bad address or divisor must not be evaluated through an
unmasked memory or division instruction.

Memory streams must currently have a proven unit-element stride.  Streams with
a store must either reference disjoint underlying objects or have the same
address SCEV and access size.  A scalar ``NoAlias`` result for ``a[i]`` versus
``a[i+1]`` is insufficient, because these streams overlap across iterations.
Nested loops, non-induction recurrences, unplanned live-outs, atomics, volatile
accesses, unknown calls and unsupported types receive diagnostics in this
stage.  These restrictions are compiler coverage limits, not new source syntax.

The pass retains the mandatory region sentinel.  The final region verifier
therefore rejects its output until target legalization has consumed the VP IR
and the sentinel.  This prevents partial generic IR support from being confused
with complete masked Tile code generation.

The ``element-predication-*.ll`` tests cover CFG conversion and diagnostics.
``pto-element-predication.cpp`` additionally starts from the actual pragma and
ordinary C++ expressions.  A separate host semantic fixture can be run with::

  python3 llvm/test/CodeGen/LinxV5/Inputs/run-element-predication-host.py \
    --opt build-tlea/bin/opt --clang /path/to/host/clang --output-dir /tmp/pto-p2

Only the disposable host-test copy removes the checked target sentinel and
expands VP using LLVM's existing expansion pass.  Its independent scalar
reference covers three-way PHI, 33/129 elements, inactive null addresses and
zero divisors, and two successive regions.  The runner also compiles the real
C++ pragma fixture through Clang, normalizes its CFG with standard LLVM passes,
and executes the widened conditional kernel against an independent reference.
``--target-clang`` can override the ``clang++`` alongside the selected ``opt``.
This is generic IR semantic
evidence, not a PTO ELF, TileOp API, gfrun or gfsim validation claim.

Explicit masked Tile legalization
---------------------------------

The first physical legalization profile is enabled with
``-mllvm -linxv5-enable-generic-element=true``.  It uses the generic CFG/VP
pipeline followed by ``linx-v5-element-tile-legalize`` and the mandatory region
verifier.  It does not fall back to the older publication/gather/atomic profile
recognizer when an operation is unsupported.  This option remains off by
default while broader shape and control-flow support is developed.

Current machine-code scope is 32 logical elements of S32/U32 data, public typed
Tile views, canonical independent i32 memory streams, and the acyclic iteration CFG
already accepted by the predicator.  The backend chooses M32 internally;
source programs use ordinary element indices without naming layout or lanes.
The usual Linx Tile register configuration is still required when generating
objects, for example::

  clang++ --target=linx64v5 -mlxbc -O2 \
    -mllvm -linxv5-enable-generic-element=true \
    -mllvm -enable-all-vector-as-tilereg=true -c kernel.cpp

The legalizer maps logical masks to low 32 GPR bits.  Arithmetic and gathers
produce fully defined Tiles using ZERO, and scatter suppresses inactive memory
effects without setting the destination-only Zero control.  Multiway PHIs and
selects use fully defined numeric inputs and GPR-predicate TSEL.  Signed divide
and arithmetic shift select S32 operations; signed remainder expands through
truncating division, multiply and subtract rather than PTO floor remainder.
Stores select the exact physical source dtype, including direct S32 results.

Vector GEPs retain one uniform base and an explicit element index.  TLEA scales
the index to byte offsets exactly once.  Raw narrow GEP indices sign-extend;
an explicit zext selects unsigned extension.  When required, a typed add-zero
publishes the matching index backing dtype before TLEA.  Proven representable
i64 constant indices may use an equivalent U32 index Tile, and identical byte
offset streams are shared.  The source-level semantics do not change.

The public ``TPARTELEMENT`` annotation is prepared before ordinary SROA and
promotion. Typed carriers retain their vector SSA type: an exact-IV extract
maps to the current carrier, an exact-IV insert becomes a masked value update,
and carrier PHIs use the same edge masks as scalar PHIs. Accumulator seeds keep
their original values on unwritten elements. Only final backedge values and
their compatible identity views may leave an accumulator region; intermediate
iteration snapshots remain unsupported. Read-only views of a dominating Tile
producer can be shared by later regions through ordinary LCSSA.

Tile inputs and outputs use the existing ``Tr`` whole-carrier operand ABI.
Consumer validation parses the operand constraints, including indirect output
argument positions; it does not match assembly mnemonics. Imports require
proven Tile producers or an already validated region publication. Final S32
publications restore their signed dtype before subsequent TileOp consumers.
No GM roundtrip or private Tile API wrapper is introduced.

LLVM may narrow scalar control arithmetic to i8 or i16. The generic predicator
represents those integer bits in i32 vectors, explicitly normalizes arithmetic
and truncation modulo the original width, and sign-extends signed operations
and casts before widening. This does not admit sub-i32 GM memory accesses.

The predicator attaches the original region token and domain to the IR it
emits.  The legalizer preflights all token-owned regions before mutation,
preserves memory-root order, and consumes each sentinel only after removing
the complete owned VP/vector-pointer graph.  Missing ownership, escaping
values, unsupported domains, partial EVL, effects or types receive diagnostics.
Unmarked functions do not run the extra generic CFG canonicalization pipeline.

The executable profile covers both ordinary GM arrays and the official S32/U32
TileOp -> element-for -> TileOp bridge. Varying inner loops, general atomics,
wider Tile shapes/dtypes, and typed Tile spills at O0 remain unsupported.
The GM-array, public typed-view and whole-application migration tests remain
separate coverage lanes.
