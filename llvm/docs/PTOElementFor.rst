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
