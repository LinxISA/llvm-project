# Scoped Scalar GM instructions (PTO #416)

The covered normative forms are pinned to the pto-spec commit recorded in
`scoped-scalar-forms.json`. No network or adjacent repository is required when
building LLVM. Update the pin deliberately when the owner changes.

Generate LLVM TableGen records, then check or regenerate the derived files:

```sh
llvm-tblgen -I llvm/include -I llvm/lib/Target/LinxV5 -dump-json \
  llvm/lib/Target/LinxV5/LinxV5.td -o /tmp/linx-records.json
python3 llvm/utils/linx/generate-scoped-scalar.py --records /tmp/linx-records.json --check
python3 llvm/utils/linx/verify-scoped-scalar.py --bin-dir /path/to/build/bin
```

The generated variants reuse the existing instruction's operand definitions,
implicit register uses/defs, constraints, scheduling and request flags. Code
generation variants constrain the first allocation stage to global registers;
the second allocation stage restores the full architectural operand classes. The
manifest enumerates 235 accepted mnemonic families; LLVM has multiple records
for some operand-modifier alternatives. Only covered records receive scoped
forms, including their permitted aq/rl/far modifiers. DMA/Prefetch and SIMT `.brg` are excluded.

Explicit `.private/.intra/.inter` forms are always eight bytes, with a single
retirement boundary. Scope is per request, not a prefix or persistent register.
The MC emitter encodes original operands using their original encoder and moves
fixups into the payload. Direct PC-relative fixups compensate the moved place
by +2, preserving the outer TPC base. Paired low relocations keep their HI
anchor. No RELAX marker is emitted for the carrier.

Clang exposes `__builtin_linx_scalar_load/store/atomic`. See IntrinsicsLinx.td
for selector values. These conservative intrinsics cannot be speculated or
replaced with unscoped memory accesses. Their effects include normal GM
aliasing; scopes are not separate address spaces. Raw builtin access widths
must be naturally aligned; TileOp byte helpers cover arbitrary alignment.

Native Scalar code now uses automatic conservative alias handling. Ordinary
accesses to parameters (including `noalias`), globals, integer-derived addresses,
and escaping stack objects become IntraCore requests. Only complete stack
objects proved nonescaping and unused by Tile memory operations retain Private
accesses. GEP/PHI/select do not establish privacy. This object-based policy also
covers buffers reused by other functions; it deliberately trades code size and
optimization opportunities for safety rather than relying on byte-range AA.

The pass runs before generic optimization and atomic expansion, then again
before instruction selection. It expands memset/memcpy/memmove and supported
atomic fallbacks while retaining scope through DSE, vectorization and libc
formation. Native vector carriers of at least 128 bytes and marked element
loops retain their existing fixed-IntraCore Tile/IndexedGenerated lowering.
SIMT function bodies and DMA/bridge target requests keep their own semantics.
Inline assembly owns its encoded requests; passing a stack address to it makes
that object potentially shared.

`__attribute__((linx_scalar_shareability("intra")))` sets an explicit function
default (also `private`/`inter`), kept out of line. Explicit scoped builtins keep
their own selectors; a Private default is a programmer contract. Memory callees
with visible native definitions receive automatic lowering. External memory
callees need a matching shareability contract, and indirect memory calls are
rejected. Such a declaration asserts that the separately compiled implementation
honors the contract; it does not rewrite a prebuilt library. Unknown generic
memory intrinsics, aggregate/scalable accesses, non-GM address spaces, and
unaligned or wider-than-64-bit shared atomics fail with a diagnostic rather than
silently emitting Private requests or an unknown-scope runtime call.

Shareability does not imply memory order or a cache policy. Scalar/Tile data
exchange requires matching IntraCore in one Core; InterCore cannot substitute.
The target model must implement scoped decoding, request provenance and
reservation rules before these binaries can be executed there.

Validation includes 2,958 normative encoding witnesses (zero/nonzero operands,
all permitted ordering/far modifiers and three scopes), reserved carriers,
relax/no-relax links and paired PC-relative lows, Clang diagnostics, native
atomic/load/store codegen, stack addresses and TileOp mixed TLOAD/TSTORE objects.
The generated witness manifest includes the spec's ordering-field positions.

Loop tests verify machine instructions before the final Local-register rewrite,
then check final assembly separately: the existing block-header representation
places control-flow headers before their bodies and fails the generic final
successor verifier. Straight-line scoped code has full machine verification.

The broader MC/LinxV5 run currently has eight failures in Tile
macro fixtures (TCMP/TSEL form ambiguity, matrix macro folding and legacy
TLOAD/TSTORE syntax). These are outside the scoped Scalar regression set;
the run reports 110 passed and one expected failure. See the workspace log
`build/issue416-mc-regression.log` for the exact diagnostics.

Automatic-alias validation additionally covers unannotated O0/O2/O3 C code,
escaping and merged stack objects, integer addresses, globals, memory copies,
small vectors, native Tile carriers, opaque-call diagnostics, and Scalar /
IndexedGenerated interoperation. The focused run passes 26 LLVM/Clang/MC/LLD
fixtures and four TileOp tests. Two additional element fixtures currently fail
expectations outside that focused set: `element-predication-atomic-uniform.ll`
expects the old `BSTART.TLSU MSCATTER` spelling, and
`pto-element-generic-f32-memory.cpp` expects a zero TCI stride where generated IR
contains 4294967296. These failures remain visible; this does not establish a
clean full-target suite or runtime model correctness.
