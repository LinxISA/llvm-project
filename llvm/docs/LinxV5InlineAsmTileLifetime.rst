Tile source lifetimes in MIR
============================

Local sources use the existing ``TRegToOffset`` lifecycle and relative-register
conversion. Shared sources use ``linxv5-shared-reuse-finalize`` immediately after
``TRegToOffset`` and before ``EmitHeader``. The latter uses LLVM
``LivePhysRegs`` and CFG live-ins to distinguish uses of physical Shared slots,
including loops, successor uses and redefinitions. It handles generated Shared
consumers and inline assembly through the same final register encoding.

A retained source becomes ``Shared_Sn_RU``; a safe final use remains
``Shared_Sn`` with a kill flag. These source-only encoding registers are reserved
and never allocation candidates. Implicit uses of the original Shared register
preserve the data dependence after the final alias conversion. Header emission
preserves these decisions and the output paths print the selected registers.
The ordinary ``%S`` modifier prints that MIR encoding; explicit ``%R`` retains,
and ``%K`` requires a proven consuming input. Tied operands remain identical
and retain their input. Destinations remain ordinary Shared registers.

The pass inspects PE masks and input roles in a restricted Tile assembly
vocabulary, without rewriting the assembly string. Constant equality ``.if`` /
``.elseif`` conditions with decimal or immediate ``${N:c}`` operands are
interpreted only for safety classification, with a maximum nesting of sixteen.
Repeated inputs, tied inputs, partial PE reads and unsupported consumer forms
retain. Recognized full-PE TLSU TMOV, TMOV.S2L.BROADCAST,
TMOV.S2L.EXTRACT and TSTORE sources may consume at their final use.

Calls, opaque assembly, literal Shared references, escaped Shared identities,
conditional or partial Shared definitions, partial function PE domains and
generic-element mode conservatively disable inferred consumption for the
function. ``INLINEASM_BR`` is also conservative. These limits protect whole-slot
MIR liveness from architectural uses or definitions it cannot represent.

The former ``InlineAsmTileLifetime`` pass, its enable/disable flag and internal
``P/Q`` modifiers have been removed. Public TileOP source needs no new modifier.
Compiler regression tests cover MIR decisions, final assembly and object
encoding. They do not establish model or hardware execution correctness.
