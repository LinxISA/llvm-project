# LinxISA Tile Intrinsics Design (PR4+PR5, strict v0.3)

Status: implemented in backend as the PR4 intrinsic surface on top of PR1-PR3 tile block lowering.

## 1. Goal

Provide a strict LLVM IR surface for Linx tile SSA values and lower it to existing Linx tile block pseudos and descriptors:

- `llvm.linx.tile.tload`
- `llvm.linx.tile.tstore`
- `llvm.linx.tile.tmov`
- `llvm.linx.cube.mamulb`
- `llvm.linx.cube.mamulb.acc`
- `llvm.linx.cube.acccvt` (PR5 stub)
- `llvm.linx.tepl.unary` / `llvm.linx.tepl.binary` (PR5 core TEPL surface)
- `llvm.linx.tepl.tadd` / `llvm.linx.tepl.tsub` / `llvm.linx.tepl.trowmax` (PR5 typed wrappers)

This keeps PHI-edge TMOV balancing (`LinxISATileSSABalance`) and descriptor emission (`LinxISABlockify`) unchanged in pass order:

1. `LinxISATileSSABalance`
2. `LinxISABlockify`

## 2. Tile IR Type

PR4 introduces a dedicated tile type:

```llvm
%linx.tile = type target("linx.tile")
```

Backend mapping:

- `target("linx.tile")` <-> `MVT::linxtile`
- Linx tile register classes are `linxtile`-only in strict v0.3 mode.
- Legacy `llvm.linx.tma.*` shim intrinsics are removed.

## 3. Core Intrinsics

```llvm
declare %linx.tile @llvm.linx.tile.tload(
  ptr %base, i32 %size_code, i32 %dtype, i64 %layout,
  i64 %lb0, i64 %lb1, i64 %stride_bytes)

declare void @llvm.linx.tile.tstore(
  ptr %base, %linx.tile %src, i32 %size_code, i32 %dtype, i64 %layout,
  i64 %lb0, i64 %lb1, i64 %stride_bytes)

declare %linx.tile @llvm.linx.tile.tmov(
  %linx.tile %src, i32 %mode, i32 %size_code, i32 %dtype,
  i64 %layout, i1 %has_layout)

declare %linx.tile @llvm.linx.cube.mamulb(
  %linx.tile %a, %linx.tile %b, i32 %m, i32 %n, i32 %k)

declare %linx.tile @llvm.linx.cube.mamulb.acc(
  %linx.tile %acc, %linx.tile %a, %linx.tile %b, i32 %m, i32 %n, i32 %k)

declare %linx.tile @llvm.linx.cube.acccvt(
  %linx.tile %acc, i32 %size_code, i32 %dtype, i64 %qarg0, i64 %qarg1)

declare %linx.tile @llvm.linx.tepl.unary(
  %linx.tile %src, i32 %tileop10, i32 %size_code, i32 %dtype)

declare %linx.tile @llvm.linx.tepl.binary(
  %linx.tile %a, %linx.tile %b, i32 %tileop10, i32 %size_code, i32 %dtype)

declare %linx.tile @llvm.linx.tepl.tadd(
  %linx.tile %a, %linx.tile %b, i32 %size_code, i32 %dtype)

declare %linx.tile @llvm.linx.tepl.tsub(
  %linx.tile %a, %linx.tile %b, i32 %size_code, i32 %dtype)

declare %linx.tile @llvm.linx.tepl.trowmax(
  %linx.tile %src, i32 %size_code, i32 %dtype)
```

## 4. Strict Legality (v0.3)

Validated in Linx DAG isel for the `llvm.linx.tile.*` surface:

1. `size_code` must be an immediate in `[5,8]` (strict 512B..4KB policy).
2. `dtype` must fit `u5` (`0..31`).
3. `tmov mode` must be `0(V2V)` or `1(A2V)`.
4. `has_layout` must be `0/1`.
5. Descriptor operands `layout/lb0/lb1/stride_bytes` are immediate operands.
6. Current strict PR4 lowering requires `stride_bytes == 0`.
7. `tepl tileop10` must be an immediate in `[0,1023]`.
8. `cube m/n/k` must be immediates in `[0,131071]`.
9. `cube.acccvt qarg1` is reserved and must be `0` in current PR5 stub lowering.

Merge-edge metadata compatibility (size/dtype/layout) remains enforced by `LinxISATileSSABalance` with hard errors on mismatch.

## 5. Lowering Mapping

### 5.1 `llvm.linx.tile.tload`

Lowering target pseudo:

- `PSEUDO_TMA_TLOAD_DESC(dst, base, dtype, layout, lb0, lb1, size, stride)`

Blockify expansion:

- `BSTART.TLOAD` (`BSTART.TMA Function=0, DataType=dtype`)
- `B.DIM` / `C.B.DIMI` for `LB0/LB1`
- `B.ARG(layout)`
- `B.IOR` (base in `RegSrc1`, stride lane currently zero)
- `B.IOT` destination push descriptor with `SizeCode=size`

### 5.2 `llvm.linx.tile.tstore`

Lowering target pseudo:

- `PSEUDO_TMA_TSTORE_DESC(base, src, dtype, layout, lb0, lb1, size, stride)`

Blockify expansion:

- `BSTART.TSTORE` (`BSTART.TMA Function=1, DataType=dtype`)
- `B.DIM` / `C.B.DIMI` for `LB0/LB1`
- `B.ARG(layout)`
- `B.IOR` (base in `RegSrc1`, stride lane currently zero)
- `B.IOT` source descriptor with `SizeCode=size`

### 5.3 `llvm.linx.tile.tmov`

Lowering target pseudo:

- `PSEUDO_TMA_TMOV(dst, src, size, dtype, layout, has_layout, mode, src_reuse)`
- PR4 intrinsic lowering sets `src_reuse=0`; CFG/PHI normalization still computes reuse from liveness when compiler-inserted TMOVs are created.

Blockify expansion:

- `BSTART.TMOV` (`BSTART.TMA Function=2, DataType=dtype`)
- `B.ARG(mode)` where mode is `0(V2V)` or `1(A2V)`
- `B.IOT` relref bindings
  - V2V: explicit source tile
  - A2V: accumulator-source semantics (no explicit source tile binding)

TMOV destination keeps queue-push-only semantics.

### 5.4 `llvm.linx.cube.mamulb`

Lowering target pseudo:

- `PSEUDO_CUBE_MAMULB(dst, a, b, m, n, k)`

Blockify expansion:

- `BSTART.TMATMUL` (`BSTART.CUBE Function=0`)
- `B.DIM` / `C.B.DIMI` for `m/n/k`
- output tile binding is emitted as queue-push destination

### 5.5 `llvm.linx.cube.mamulb.acc`

Lowering target pseudo:

- `PSEUDO_CUBE_MAMULB_ACC(dst, acc, a, b, m, n, k)`

Blockify expansion:

- `BSTART.TMATMUL.ACC` (`BSTART.CUBE Function=2`)
- `B.DIM` / `C.B.DIMI` for `m/n/k`
- input accumulator tile is explicitly bound as descriptor input

### 5.6 `llvm.linx.cube.acccvt`

Lowering target pseudo:

- `PSEUDO_CUBE_ACCCVT(dst, acc, size, dtype, qarg0, qarg1)`

Blockify expansion:

- `BSTART.ACCCVT` (`BSTART.CUBE Function=8, DataType=dtype`)
- `B.ARG(qarg0)`
- `B.IOT` destination descriptor (current strict stub requires `qarg1=0`)

Current PR5 behavior keeps quant descriptor wiring minimal; `qarg1` is reserved until full quantization descriptor plumbing lands.

### 5.7 `llvm.linx.tepl.unary` / `llvm.linx.tepl.binary`

Lowering target pseudos:

- `PSEUDO_TEPL_UNARY(dst, src, tileop10, size, dtype)`
- `PSEUDO_TEPL_BINARY(dst, a, b, tileop10, size, dtype)`

Blockify expansion:

- `BSTART.TEPL(tileop10, dtype)`
- input `B.IOT` descriptor (unary: one source, binary: two sources)
- output `B.IOT` descriptor (queue-push destination bind with `SizeCode=size`)

### 5.8 Typed TEPL wrappers

Typed wrappers lower through the same pseudos as generic TEPL dispatch:

- `llvm.linx.tepl.tadd(a, b, size, dtype)` -> `PSEUDO_TEPL_BINARY(..., TileOp10=0x000)`
- `llvm.linx.tepl.tsub(a, b, size, dtype)` -> `PSEUDO_TEPL_BINARY(..., TileOp10=0x001)`
- `llvm.linx.tepl.trowmax(src, size, dtype)` -> `PSEUDO_TEPL_UNARY(..., TileOp10=0x020)`

## 6. Alias and Assembly Contract

Canonical assembly aliases remain:

- `BSTART.TLOAD`
- `BSTART.TSTORE`
- `BSTART.TMOV`

These are aliases of `BSTART.TMA` with `Function={0,1,2}`.

## 7. Diagnostics

PR4 adds strict diagnostics for the new intrinsic surface:

- invalid `size_code`
- invalid `dtype`
- invalid `mode`
- invalid `has_layout`
- non-zero `stride_bytes` (current strict profile)
- non-constant descriptor operands

PHI-edge and metadata mismatch diagnostics remain precise (function + MBB + instruction context) in `LinxISATileSSABalance`.

## 8. Roadmap Hooks

1. Extend TEPL typed-wrapper coverage beyond initial `tadd/tsub/trowmax` for additional common `TileOp10` values.
2. Complete ACCCVT quantization descriptor wiring (`B.ARG` + `B.IOR`) beyond current `qarg1=0` stub policy.
3. Enable non-zero stride lowering by carrying explicit stride registers into `B.IOR`.
4. Keep global relay minimization and advanced ClockHands-style optimization out of PR4/PR5.
