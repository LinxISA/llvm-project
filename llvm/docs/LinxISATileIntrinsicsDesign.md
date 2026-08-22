# LinxISA PTO 0.58 Tile Intrinsics

This document records the LLVM and Clang lowering boundary for the released PTO
0.58 common subset. Architectural behavior and encodings come from LinxISA
v0.58 and its pinned PTO manifest; these intrinsics are compiler interfaces,
not an independent specification.

## First-class tile intrinsics

The target extension type is:

```llvm
%linx.tile = type target("linx.tile")
```

TLSU operations:

```llvm
declare %linx.tile @llvm.linx.tile.tload(
    ptr, i32 tsize, i32 dtype, i64 layout,
    i64 valid_col, i64 valid_row, i64 stride_bytes)
declare void @llvm.linx.tile.tstore(
    ptr, %linx.tile, i32 tsize, i32 dtype, i64 layout,
    i64 valid_col, i64 valid_row, i64 stride_bytes)
declare %linx.tile @llvm.linx.tile.tmov(
    %linx.tile, i32 tsize, i32 dtype, i64 layout, i1 has_layout)
```

Tile-operation dispatch:

```llvm
declare %linx.tile @llvm.linx.tileop.unary(
    %linx.tile, i32 selector, i32 tsize, i32 dtype)
declare %linx.tile @llvm.linx.tileop.binary(
    %linx.tile, %linx.tile, i32 selector, i32 tsize, i32 dtype)
```

The selector is the unchanged packed TEPL carrier value
`(Mode << 5) | Function`. The semantic operation is classified as `VEC` or
`SFU`. Assigned selectors lower to canonical `BSTART.VEC` or `BSTART.SFU`
assembly; reserved selectors reject.

CUBE operations:

```llvm
declare %linx.tile @llvm.linx.cube.tmatmul(
    %linx.tile a, %linx.tile b, i32 m, i32 n, i32 k)
declare %linx.tile @llvm.linx.cube.tmatmul.acc(
    %linx.tile acc, %linx.tile a, %linx.tile b,
    i32 m, i32 n, i32 k)
```

The accumulator is an explicit ordinary Local tile. There is no `ACCCVT`
intrinsic and no implicit accumulator state.

## Frontend vector bridges

Clang builtins use fixed vectors at the frontend boundary. Their LLVM bridge
intrinsics use `.vec` or `.shape` suffixes and lower immediately to the same
PTO 0.58 machine pseudos as first-class tile intrinsics. These are current
frontend adapters, not legacy ISA compatibility paths.

Examples include:

- `llvm.linx.tile.tmov.vec`
- `llvm.linx.cube.tmatmul.vec`
- `llvm.linx.cube.tmatmul.acc.vec`
- `llvm.linx.tlsu.tload.shape`
- `llvm.linx.tlsu.tstore.shape`
- `llvm.linx.tileop.unary.shape`
- `llvm.linx.tileop.binary.shape`

## Legality

The backend enforces before instruction selection:

- SizeCode is an immediate in `[1, 10]` (128 B through 64 KiB per PE).
- TLOAD/TSTORE and tile operations require a concrete DataType in
  `0..14`, `16..20`, or `24..28`; TMOV additionally accepts
  `DTYPE_NONE=31` for source-descriptor inference.
- Layout is an assigned B.DATR code. TMOV requires `layout=0` when
  `has_layout=0` and validates the encoded layout when `has_layout=1`.
- Tile-operation selectors are assigned PTO 0.58 selectors.
- VEC/SFU operand modes match the intrinsic form.
- CUBE M, N, and K are arbitrary positive integers in `[1, 65535]`. The
  compiler bridge uses CUBE_M32 for A/C/D and CUBE_N8 for B, requires
  `M <= 32`, validates each operand capacity independently, and rounds the
  output requirement up to a legal Local SizeCode.
- TLOAD/TSTORE strides are nonnegative, element-aligned, and large enough for
  the selected row span when explicitly nonzero.
- Dynamic values are accepted only for descriptor fields whose bridge contract
  explicitly permits them.

Failure is explicit. The backend does not normalize deleted names, reserved
selectors, illegal SizeCode values, or incompatible descriptors.

## Machine pseudos

The principal internal operations are:

- `PSEUDO_TLSU_TLOAD_DESC`
- `PSEUDO_TLSU_TSTORE_DESC`
- `PSEUDO_TLSU_TMOV`
- `PSEUDO_TILEOP_UNARY`
- `PSEUDO_TILEOP_BINARY`
- `PSEUDO_CUBE_TMATMUL`
- `PSEUDO_CUBE_TMATMUL_ACC`

Machine pseudos use semantic engine names. Only final MC emission refers to the
TEPL encoding carrier.

## Hard-break policy

PTO 0.58 is a hard break. The compiler exposes no TMA intrinsic namespace, no
TEPL semantic intrinsic namespace, no MAMULB or ACCCVT intrinsic, and no old
B.IOT compatibility form. Deleted assembly spellings remain errors.
