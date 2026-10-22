LinxV5 Matrix FIXPIPE attributes
===============================

``B.FPATR`` controls the FIXPIPE post-processing of matrix CUBE operations,
including ``TMATMULMX``. It has **ten**, not nine, positional operands::

  B.FPATR PreQuantMode, ReluMode, GroupNCode, RowMaxEn, GroupMaxEn, RowMaxInit, MaxAbsEn, TransA, TransB, CScaleEn

The instruction printer directly replaces the positional numbers with named
operands, without an explanatory comment. For example::

  B.FPATR PreQuantMode=F322BF16, ReluMode=ReLU, GroupN=32, RowMaxEn=1, GroupMaxEn=1, RowMaxInit=0, MaxAbsEn=0, TransA=0, TransB=0, CScaleEn=0

This is assembler syntax, not an annotation. Named fields may appear in any
order, but all ten must be specified exactly once. Field and mode names are
case-insensitive. Modes accept either a symbolic name or their numeric code.
``GroupN`` takes the actual column count; ``GroupNCode`` is accepted instead
when specifying the encoded group-width code. They name the same field and
cannot both appear. Flags remain 0 or 1.

The assembler also continues to accept the original ten-number positional
syntax. Named and positional operands cannot be mixed in one instruction.
Machine encoding and existing cross-field legality checks are unchanged.

Existing TileOP inline assembly using ten numeric ``%c[...]`` operands does
not need to change. Text consumers matching positional disassembly must adapt
to the named output. Older assemblers that lack named operand support cannot
necessarily assemble the new textual output, even though object encoding is
unchanged. Folded TileOP macros retain their existing configuration syntax,
including numeric ``PreMode`` and ``PostMode`` values.

Operand meanings
----------------

* ``PreQuantMode`` selects accumulator conversion/quantization and output type.
  Zero (``NoQuant``) preserves the accumulator type.
* ``ReluMode`` selects no activation (0), ReLU (1), scalar LReLU/PReLU (2),
  or per-column vector PReLU (3).
* ``GroupNCode`` is an encoding, not the group width itself. Codes 0 through 9
  map to 0, 8, 16, 32, 48, 64, 80, 96, 112, and 128 columns respectively.
* ``RowMaxEn`` enables row-maximum output and the applicable row-maximum input.
* ``GroupMaxEn`` enables group-maximum output; it requires a nonzero
  ``GroupNCode``, and a nonzero ``GroupNCode`` requires ``GroupMaxEn``.
* ``RowMaxInit`` initializes the row maximum from ``RowMaxIn``; it requires
  ``RowMaxEn``.
* ``MaxAbsEn`` selects maximum absolute value instead of signed maximum for
  enabled reductions. It requires ``RowMaxEn`` or ``GroupMaxEn``.
* ``TransA`` and ``TransB`` enable logical transpose of a Shared A or B primary.
  They do not request transpose of an arbitrary Local Tile.
* ``CScaleEn`` enables per-row scaling of the explicit FP32 accumulator C.
  It is supported only by FP32 ``TMATMUL.ACC`` and ``TMATMULMX.ACC`` forms.

All seven flags are encoded as 0 (disabled) or 1 (enabled). An all-zero
``B.FPATR`` is still required in a matrix bundle; it is not equivalent to
omitting the instruction. Vector parameters here are Local Tile parameters,
not SIMT vector instructions or registers.

Quantization mode names
-----------------------

The printer uses the closed mode table from PTO-SPEC revision
``b0165320a4b575b18d2f4d93faf40178f661ab49``,
``asl/block/attributes/B.FPATR.asl`` and
``docs/status/decisions/ADR-NUM-0012-numeric-postprocess-and-format-operations.md``
(Decision 179). Other six-bit codes are reserved.

==== ============== ============ ===========
Code Name           Accumulator  Destination
==== ============== ============ ===========
0    NoQuant        FP32/S32/U32 unchanged
1    F322F16        FP32         FP16
2    VREQ8          S32          S8
3    REQ8           S32          S8
4    VDEQF16        S32          FP16
5    DEQF16         S32          FP16
12   VSHIFTS322S16  S32          S16
13   SHIFTS322S16   S32          S16
16   F322BF16       FP32         BF16
17   REQ4           S32          S4X2
18   VREQ4          S32          S4X2
19   DEQS16         S32          S16
20   VDEQS16        S32          S16
23   VQF322B8_PRE   FP32         S8
24   QF322B8_PRE    FP32         S8
25   QF322HIF8_PRE  FP32         HiF8
26   QF322FP8_PRE   FP32         E4M3
27   QF322F32_PRE   FP32         FP32
28   VQF322HIF8_PRE FP32         HiF8
32   QF322F16_PRE   FP32         FP16
33   VQF322F16_PRE  FP32         FP16
34   QF322BF16_PRE  FP32         BF16
35   QS322BF16_PRE  S32          BF16
36   VQF322BF16_PRE FP32         BF16
37   VQF322FP8_PRE  FP32         E4M3
38   VQF322F32_PRE  FP32         FP32
39   VQS322BF16_PRE S32          BF16
==== ============== ============ ===========

For nonzero modes, a ``V`` prefix denotes per-column Local Tile parameters;
the corresponding non-``V`` parameterized modes use scalar parameters.
``F322F16`` and ``F322BF16`` need no quantization parameter.

Physical versus folded disassembly
----------------------------------

When a complete bundle can be represented as a TileOP macro, the disassembler
may fold its physical instructions into a named macro with configuration fields.
To inspect each physical ``B.FPATR`` and its named operands, use::

  llvm-objdump -d --disassembler-options=no-tile-macros input.o
