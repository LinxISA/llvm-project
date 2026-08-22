# LinxISA PTO 0.58 Tile Block Contract

This document describes the compiler-facing block contract implemented by the
LinxISA backend. The released LinxISA v0.58 catalog and its pinned PTO 0.58
manifest are authoritative for encodings and architectural semantics. This
document does not define a second instruction set.

## Execution engines

PTO 0.58 has four tile execution engines:

| Engine | Responsibility | Canonical block header |
| --- | --- | --- |
| `VEC` | Element-wise tile operations | `BSTART.VEC Operation, DataType` |
| `SFU` | Reductions, transcendental operations, data rearrangement, and other complex operations | `BSTART.SFU Operation, DataType` |
| `TLSU` | Tile load, store, movement, and memory operations | operation-specific `BSTART.T*` header |
| `CUBE` | Matrix operations | operation-specific `BSTART.TMATMUL*` header |

`VEC` and `SFU` are semantic classifications and canonical assembly aliases.
They reuse the unchanged `BSTART.TEPL Mode, Function, DataType` binary carrier.
The backend must not allocate a new encoding merely to express the engine
classification. A raw TEPL carrier is accepted only for an assigned PTO 0.58
Mode/Function pair; reserved pairs fail closed.

The exact operation-to-engine table is centralized in
`LinxISATileEnginesV058.h` and must match the released PTO tile-operation
catalog. Disassembly prints the canonical `BSTART.VEC` or `BSTART.SFU` alias
for assigned operations.

## TLSU blocks

The compiler emits operation-specific TLSU headers:

- `BSTART.TLOAD DataType`
- `BSTART.TSTORE DataType`
- `BSTART.TMOV DataType`

`TLOAD` and `TSTORE` encode their stride through the bundle schema. An omitted
stride selects the architectural dense-stride default; an explicitly encoded
zero remains zero and is not treated as omission.

`B.IOT` supplies Local tile operands. PTO 0.58.3 encodes a three-bit PEMode
that expands to one of eight fixed four-PE masks and a SizeCode in `[1, 10]`,
representing 128 B through 64 KiB per participating PE.
A zero PE mask is a strict no-op. Source-only and destination forms retain their
distinct v0.58 encodings.

## CUBE blocks

The compiler models accumulator state as an explicit Local tile operand. It
does not use an implicit accumulator singleton and does not synthesize an
`ACCCVT` operation.

- `TMATMUL` consumes explicit Local A and B and writes explicit Local D.
- `TMATMUL.ACC` additionally consumes explicit Local C and writes explicit
  Local D. Aliasing C and D is legal read-old/write-new when descriptors match.

Matrix dimensions M, N, and K are arbitrary positive integers in `[1, 65535]`
and are independent of SizeCode. The current intrinsic bridge uses CUBE_M32
for A/C/D and CUBE_N8 for B, requires `M <= 32`, validates A, B, C, and D
capacity independently, and rounds the D requirement up to a legal Local
SizeCode.

## Operand and descriptor rules

- Scalar bundle inputs and outputs are absolute architectural GPR names from
  the 24-register bundle-visible set. Zero denotes an absent/default value
  where the selected BSTART schema permits omission.
- Local tile queue operands use `B.IOT`; shared tile bindings use `B.IOS` with
  absolute `S0` through `S255` identifiers.
- A bundle schema is selected after all bundle instructions are collected.
  Omitted fields use that instruction's architectural default. Nonzero surplus
  fields are rejected.
- Duplicate inputs and outputs are legal when the selected operation schema
  permits them.
- `B.IOD`, `C.B.IOS`, and `BSTART.PAR` are deleted spellings and are assembler
  errors. They are not compatibility aliases.

## Compiler lowering

The backend represents tile values in the `TILE_MN` register class and balances
tile SSA copies before block formation. Compiler-inserted movement uses
`PSEUDO_TLSU_TMOV` and lowers through the ordinary TLSU `TMOV` bundle. No
separate or legacy TMA path exists.

Queue ordering is architectural state. If a control-flow join does not prove a
unique queue order, block formation rejects instead of choosing an order.

## Validation surfaces

The corresponding regression suites are:

- `llvm/test/MC/LinxISA/v058-pto-common-roundtrip.s`
- `llvm/test/MC/LinxISA/v058-pto-common-errors.s`
- `llvm/test/CodeGen/LinxISA/v058-cube-explicit-accumulator.mir`
- TLSU, tile-operation, queue-order, and descriptor tests under
  `llvm/test/CodeGen/LinxISA/`
