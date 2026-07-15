# LinxISA Tile Block Encoding Contract (PR1 + PR2 core CFG/RelRef)

Status: PR1 implemented; PR2 core CFG balancing + relative tile binding is implemented; PR3 extends edge balancing with cycle handling, strict tile-size policy, and TMOV mode expansion (`V2V` + `A2V`).

## 1. Scope

This document freezes the PR1 tile-block assembly contract for LLVM + MC:

- Canonical typed tile headers: `BSTART.TMA`, `BSTART.CUBE`, `BSTART.TEPL`, `BSTART.VPAR`, `BSTART.VSEQ`.
- `TMOV_FP` is removed from the minimal feature set.
- `TVEC` is not a standalone block mnemonic.
- PTO vector template ops (`TADD`, `TSUB`, `TROWMAX`, etc.) are encoded through `BSTART.TEPL`.
- `VPAR`/`VSEQ` are the TVEC-equivalent execution path and use SIMT block bodies.

## 2. Block Family Split

### 2.1 Header-only tile blocks

These block families are encoded as typed headers plus descriptors, with no mandatory out-of-line body for the operation itself:

- `BSTART.TMA` (tile memory/move)
- `BSTART.CUBE` (tile matmul/accumulator)
- `BSTART.TEPL` (template PTO vec ops)

### 2.2 SIMT-body tile vector blocks

`BSTART.VPAR` and `BSTART.VSEQ` correspond to TVEC-style execution:

- They are the only TVEC-equivalent block headers.
- They carry a SIMT body path (`B.TEXT <label>` to a linear body terminated by `BSTOP/C.BSTOP`).
- Vector operation sequencing lives in the body, not as a `TileOp10` selector in the header.

## 3. Encoding Assignment (v0.3 codec)

From `llvm/lib/Target/LinxISA/MCTargetDesc/linxisa_opcodes.c`:

| Header | asm_fmt | mask | match | fields |
|---|---|---|---|---|
| `BSTART.TMA` | `BSTART.TMA Function, DataType` | `0x060fffff` | `0x00011181` | `DataType[4:0]`, `Function[4:0]` |
| `BSTART.CUBE` | `BSTART.CUBE Function, DataType` | `0x060fffff` | `0x00031181` | `DataType[4:0]`, `Function[4:0]` |
| `BSTART.TEPL` | `BSTART.TEPL TileOp10, DataType` | `0x06007fff` | `0x02001181` | `DataType[4:0]`, `TileOp10[9:0]` |
| `BSTART.VPAR` | `BSTART.VPAR <VS8, VS16>` | `0xf9ffffff` | `0x00021181` | `Mode` (current LLVM emits `0`) |
| `BSTART.VSEQ` | `BSTART.VSEQ <VS8, VS16>` | `0xf9ffffff` | `0x00029181` | `Mode` (current LLVM emits `0`) |

## 4. Selector Assignment

### 4.1 `BSTART.TMA` Function (minimal)

- `0`: `TLOAD`
- `1`: `TSTORE`
- `2`: `TMOV`

No `TMOV_FP` function value in minimal mode.

### 4.2 `BSTART.CUBE` Function (current)

- `0`: `TMATMUL` (`MAMULB` alias)
- `2`: `TMATMUL.ACC` (`MAMULB.ACC` alias)
- `8`: `ACCCVT`

### 4.3 `BSTART.TEPL` TileOp10

Assigned PTO template-op values:

- Elementwise/base: `TADD=0x000`, `TSUB=0x001`, `TMUL=0x002`, `TDIV=0x003`, `TMAX=0x004`, `TMIN=0x005`, `TAND=0x006`, `TOR=0x007`, `TXOR=0x008`, `TSHL=0x009`, `TSHR=0x00A`, `TRELU=0x00D`, `TPRELU=0x00E`, `TCVT=0x00F`
- Row/col reductions: `TROWMAX=0x020`, `TROWMIN=0x021`, `TROWSUM=0x022`, `TCOLMAX=0x024`, `TCOLMIN=0x025`, `TCOLSUM=0x026`
- Math transforms: `TEXP=0x040`, `TLOG=0x041`, `TSQRT=0x042`, `TRSQRT=0x043`, `TRECIP=0x044`
- Data movement/shape: `TGATHER=0x060`, `TSCATTER=0x061`, `TRESHAPE=0x062`, `TTRANSPOSE=0x063`

Unassigned values remain reserved for future PTO template expansion.

## 5. Canonical Assembly/Disassembly Rules

### 5.1 Accepted/printed aliases

The assembler accepts and disassembler prints typed aliases:

- TMA aliases: `BSTART.TLOAD`, `BSTART.TSTORE`, `BSTART.TMOV`
- CUBE aliases: `BSTART.TMATMUL`, `BSTART.TMATMUL.ACC`, `BSTART.ACCCVT`
- TEPL aliases: `BSTART.TADD`, `BSTART.TSUB`, `BSTART.TROWMAX`, etc.

### 5.2 Fallback forms

- Known TEPL selectors disassemble to alias mnemonic (`BSTART.TADD FP16`, etc.).
- Unknown TEPL selector disassembles as `BSTART.TEPL <TileOp10>, <DataType>`.
- `TMOV_FP` alias/function is not emitted in minimal mode.

### 5.3 Alias disambiguation rules

- `BSTART.TLOAD`/`BSTART.TSTORE`/`BSTART.TMOV` are exact aliases of `BSTART.TMA Function={0,1,2}`.
- `BSTART.TMATMUL`/`BSTART.TMATMUL.ACC`/`BSTART.ACCCVT` are exact aliases of `BSTART.CUBE Function={0,2,8}`.
- `BSTART.T*` PTO template aliases (for example `BSTART.TADD`, `BSTART.TROWMAX`) are exact aliases of `BSTART.TEPL TileOp10=<value>`.
- `BSTART.PAR` is not accepted in v0.56.5; use the typed block headers above.

## 6. Examples

### 6.1 TEPL template op (header-only)

```asm
BSTART.TADD FP16
B.IOT t#1, t#2, last, ->t<4KB>
C.BSTOP
```

Equivalent canonical form:

```asm
BSTART.TEPL TADD, FP16
```

### 6.2 TVEC-equivalent via VPAR/VSEQ + SIMT body

```asm
BSTART.VPAR 0
B.TEXT simt_body
C.BSTOP

simt_body:
  ; SIMT body uops/instructions
  C.BSTOP
```

No `BSTART.TVEC` mnemonic is defined.

### 6.3 Block-split and descriptor constraints

- `BSTART.TLOAD` (`Function=0`) and `BSTART.TSTORE` (`Function=1`) are header-only tile memory blocks.
- TLOAD/TSTORE descriptor contract:
  - layout/pad descriptor via `B.ARG` (for example ND2ZN/DN2NZ flavor + element format);
  - address/stride descriptor via `B.IOR` (`[base,stride,...]`);
  - tile binding/allocation via `B.IOT` (including relative tile destination and size class).
- `BSTART.TMOV` (`Function=2`) is tile-state-only movement (no GM memory transfer). It uses:
  - `B.ARG` for TMOV mode (`V2V=0`, `A2V=1`);
  - tile binding descriptors (`B.IOT`) for source/destination relrefs and `SizeCode`;
  - no `B.IOR` memory base/stride descriptor in minimal PR2 TMOV lowering.
  - canonical `SizeCode` policy: the immediate `imm4` selects the v0.56.5 size table; register-sized descriptors are rejected.
- `BSTART.TMATMUL` (`Function=0`) and `BSTART.TMATMUL.ACC` (`Function=2`) are CUBE blocks and require matrix shape descriptors:
  - `m,n,k` via `B.DIM` or repeated `C.B.DIMI` into `LB0/LB1/LB2`;
  - tile source/destination bindings via `B.IOT` (A/B/(optional)ACC).
- `BSTART.ACCCVT` (`Function=8`) is CUBE accumulator conversion and requires quantization descriptors:
  - scale/zero-point and conversion policy carried by `B.ARG` + `B.IOR` profile-defined arguments;
  - accumulator/source/destination tile bindings via `B.IOT`.
- `BSTART.TEPL` is header-only template dispatch:
  - `TileOp10` chooses the PTO template op;
  - descriptor requirements are op-family specific (shape/reduction axes/stride policy) and are carried via `B.ARG`, `B.IOR`, `B.DIM`/`C.B.DIMI`, and `B.IOT` tile bindings as required by that template.
- `BSTART.VPAR`/`BSTART.VSEQ` are SIMT-body blocks:
  - header encodes vector execution mode only;
  - operation sequence resides in the `B.TEXT` body and must terminate at `BSTOP/C.BSTOP`.

## 7. Validation Targets

- MC alias/canonical tests:
  - `llvm/test/MC/LinxISA/v03-tma-function-canonical.s`
  - `llvm/test/MC/LinxISA/v03-tma-function-range-error.s`
  - `llvm/test/MC/LinxISA/v03-cube-function-canonical.s`
  - `llvm/test/MC/LinxISA/v03-cube-function-fallback.s`
  - `llvm/test/MC/LinxISA/v03-tepl-template-ops.s`
  - `llvm/test/MC/LinxISA/v03-tepl-tileop-range-error.s`
  - `llvm/test/MC/LinxISA/v03-tmov-descriptor-roundtrip.s`
  - `llvm/test/MC/LinxISA/v03-vpar-vseq-encoding.s`
- CodeGen PR2 tests:
  - `llvm/test/CodeGen/LinxISA/v03-tile-phi-edge-tmov.mir`
  - `llvm/test/CodeGen/LinxISA/v03-tile-phi-loop-tmov.mir`
  - `llvm/test/CodeGen/LinxISA/v03-tile-phi-metadata-mismatch.mir`
  - `llvm/test/CodeGen/LinxISA/v03-tile-phi-edge-tmov.ll`
- PR3/strict profile additions:
  - `llvm/test/CodeGen/LinxISA/v03-tile-phi-cycle-temp.mir`
  - `llvm/test/CodeGen/LinxISA/v03-tile-phi-cycle-spill.mir`
  - `llvm/test/CodeGen/LinxISA/v03-tmov-a2v-mode.mir`
  - `llvm/test/CodeGen/LinxISA/v03-tile-size-policy-reject.mir`
  - `llvm/test/MC/LinxISA/v03-ioti-size-range-error.s`
- Existing disasm snippet updated to canonical TEPL fallback for packed template values.

## 8. PR2 Backend Design (implemented core slice)

### 8.1 Pass placement and purpose

- New machine pass: `LinxISATileSSABalance` (legacy PM id: `linx-tile-ssa-balance`).
- Run points in Linx target pipeline:
  1. post-regalloc (before generic `ExpandPostRAPseudos`, so tile `COPY` is converted before generic COPY expansion)
  2. pre-emit guard immediately before `LinxISABlockify`

This pass converts tile `COPY` traffic (including PHI-edge copies after PHI elimination) into canonical compiler-inserted `PSEUDO_TMA_TMOV`.

### 8.2 Canonical internal relref model

The backend now uses canonical relative tile references before final descriptor encoding:

- `TileHand = {T, U, M, N, ACC}`
- `TileRelRef = {Hand, Depth, Reuse}`
  - `Depth` is constrained to `1..8` (strict validation).
  - `Reuse` is derived from source kill/liveness (`kill=false => reuse=1`).

Canonical mapping:

- `t#k -> (k-1)`
- `u#k -> 8 + (k-1)`
- `m#k -> 16 + (k-1)`
- `n#k -> 24 + (k-1)`

Relrefs lower into canonical `B.IOT` fields: source tile identifiers use the 6-bit hand/depth namespace and destinations use the encoded destination-hand field.

### 8.3 Strict metadata rules for COPY->TMOV conversion

`LinxISATileSSABalance` tracks tile metadata from tile-defining pseudos and enforces strict compatibility when multiple edges feed a merged tile destination:

- `SizeCode` is mandatory and must match.
- `DataType` must match when available on both values.
- `Layout` must match when available on both values.
- Missing required size metadata is rejected.
- Any relref depth outside `1..8` is rejected.

On mismatch, the backend emits a precise fatal diagnostic with function + MBB + instruction context.

### 8.4 TMOV lowering in blockify

`LinxISABlockify` now lowers compiler-inserted `PSEUDO_TMA_TMOV` into:

1. `BSTART.TMA` with `Function=2` (`TMOV`)
2. `B.ARG` carrying TMOV mode (`V2V=0`, `A2V=1`)
3. `B.IOT` descriptor with relref-derived source/destination and propagated `SizeCode`

No new opcode encoding was added; this reuses the canonical TMA/B.IOT encoding space.

## 9. PR3 Relay-Minimization (implemented)

PR3 adds a relay-minimization peephole in `LinxISATileSSABalance` after COPY->TMOV conversion:

- Remove identity relays:
  - `dst = TMOV dst` is deleted.
- Fold one-hop relay chains when descriptor metadata is compatible:
  - `t1 = TMOV t0`
  - `t2 = TMOV t1`
  - becomes `t2 = TMOV t0`

Fold preconditions:

- both instructions are `PSEUDO_TMA_TMOV`
- same `{SizeCode, DataType, Layout, HasLayout, Mode}`
- intermediate tile has exactly one non-debug use (the second TMOV)

Current PR3 is local (same basic block), conservative, and does not alter ISA encodings.

Validation additions:

- `llvm/test/CodeGen/LinxISA/v03-tile-relay-min-chain.mir`
- `llvm/test/CodeGen/LinxISA/v03-tile-relay-min-identity.mir`

## 10. PR3 CFG hardening (implemented)

`LinxISATileSSABalance` now treats tile COPY bundles on CFG edges (block-entry or block-tail bundles) as parallel-copy normalization points:

- full tile live-in edge normalization at block entry/tail COPY bundles;
- PHI/COPY cycle break priority:
  - first choice: reserved temp tile (`TILE31`);
  - fallback: spill source tile (`PSEUDO_TMA_TSTORE`) and reload into destination (`PSEUDO_TMA_TLOAD_ANY`).

Interior COPY chains that are not edge bundles keep sequential semantics.

## 11. Strict profile constraints (implemented)

- strict tile size policy: `SizeCode=5..8` (`512B..4KB`) is enforced in:
  - `LinxISATileSSABalance` metadata validation;
  - `LinxISABlockify` TLOAD/TSTORE/TMOV lowering;
  - asm parser angle-size parsing for `B.IOT` (`-><kind><size>`).
- queue-push destination encoding for TMOV lowering:
  - destination hand is encoded in `DstTile`;
  - destination tile-id payload in `SrcTile1` is normalized to hand head (push slot), not explicit depth.
- TMOV mode support:
  - `V2V=0`: explicit source tile binding;
  - `A2V=1`: source tile absent in descriptor, accumulator is implicit source domain.

## 12. Future Extension Fit

- `BSTART.TMA Function[4:0]`: `0..2` assigned in minimal strict-v0.3 (`TLOAD/TSTORE/TMOV`), `3..31` reserved for future typed memory/tile control ops.
- `BSTART.CUBE Function[4:0]`: `0,2,8` assigned (`TMATMUL`, `TMATMUL.ACC`, `ACCCVT`), all other values reserved for future CUBE families.
- `BSTART.TEPL TileOp10[9:0]`: sparse assigned PTO template space; unassigned values are reserved and must disassemble as numeric `BSTART.TEPL <TileOp10>, <DataType>`.
- New operations should prefer alias mnemonics that map to these selector spaces instead of adding new base block families unless a new block execution model is required.
