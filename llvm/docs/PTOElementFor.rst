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
zero divisors, and two successive regions.  This is generic IR semantic
evidence, not a PTO ELF, TileOp API, gfrun or gfsim validation claim.
