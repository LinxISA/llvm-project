PTO ISA 0.59 alignment
=====================

Specification baseline
----------------------

The integration baseline is PTO-ISA/pto-spec main at
``42067d0f2890b8c013a868bd1247ebc6437b3179``. Normative ASL/NDF owns
instruction behavior. This is a development snapshot, not a claim that the
compiler implements every instruction or that a published release is certified.

Implementation and validation status
------------------------------------

L1 is a compatibility migration, not strict grammar replacement. The source
projection, header mapping and 24 atomic TSTORE forms are checked; historical
TEPL Layout, TCVT and MX emitter alternatives remain intentionally visible.
Removing them requires a separate compatibility decision and migration tests.

L2 has targeted encoding, physical replay, macro replay and rejection evidence
for #398/#399, Shared ND/CUBE TMOV, indexed bounds and atomic TSTORE. It is not
an exhaustive audit of every changed normative ASL clause or runtime behavior.

L3 retains the existing #398/#399 implementation and #406 PredicateCell logical
descriptor tracking (``1ff282e03713``), adds Shared ND/CUBE TMOV and the separately
pinned indexed-range extension. Logical descriptor MIR tests pass. Follow-up
triage found that ``element-predication-typed-view.ll`` and its invalid companion
failed because of stale constant-materialization ordering and diagnostic
expectations, not a demonstrated lowering defect. Both pass after updating the
checks while retaining normalization dataflow and invalid-path rejection
coverage. This does not establish that all element typed-view paths work.

L4 adds fail-closed PredicateCell atomic coverage without broadening lowering.
L5 preserves both Local Tile ABI regression files and descriptor-preserving
Tile values live across an ordinary opaque call. It introduces no Shared
parameter/return ABI or generic noinline typed-view helper ABI.

L6 implements opt-in ELF note production and linker admission using the pinned
development identity below. Platform acceptance and publication remain external
decisions; no default compatibility or certification guarantee is changed.

Validation against LLVM ``684074b245ea`` uses MC/LinxV5, CodeGen/LinxV5,
Clang/LinxV5 and the new LLD identity test. Baseline: 464 PASS, 104 FAIL,
2 XFAIL, 1 UNRESOLVED. After this patch: 485 PASS, 102 FAIL, 2 XFAIL,
1 UNRESOLVED. All 19 new tests pass; no previously passing test regresses.
Two existing tests now pass after building llvm-readobj. Existing failures are
not claimed as fixed or evidence of conformance. Machine-readable logs are in
``/home/zhuwei/codex_tmp/llvm-059-isa-alignment/{baseline,final2}.json``.

After rebasing onto development mainline ``b19e8c0e184c``, the suite reported
490 PASS, 102 FAIL, 2 XFAIL and 1 UNRESOLVED. The typed-view expectation updates
bring this to 492 PASS, 100 FAIL, 2 XFAIL and 1 UNRESOLVED, with no previously
passing test regressing. Follow-up logs are in
``/home/zhuwei/codex_tmp/llvm-059-followup/final.{json,log}``. These two recovered
tests are test maintenance, not new ISA implementation coverage.

The development identity is ``pto-isa-0.59.0-mode-function-v1``, release
``0.59.0``, with encoding projection
``4effea27acd51de4f637bddbee7d80e135c2d53775fa90e7d47c49041da8f693``.
The historical release manifest has a different projection; its fingerprint
must not be substituted for the current development fingerprint.

Catalog and interfaces
----------------------

``llvm/utils/linxv5/pto_0590_catalog.json`` is a compact, reproducible
projection of the pinned normative macro catalog. It includes all 119 direct
operations, including TLEA. LLVM retains existing emitter forms and Encoding
IDs; the 24 atomic TSTORE aliases are appended as explicit Encoding alternatives.
These select Canonicalize=1 and the exact reduction operator, validate dtype,
and reject N8 atomic CUBE storage. The expanded compatibility model records
0.59.0 and the normative source digest; its historical seed remains unchanged.
It is not a strict normative grammar migration: historical TEPL Layout
attributes, TCVT alternatives and MX emitter constraints remain, and require
separate removal/migration decisions. Header and atomic-binder checks do not
establish complete 0.59 legality coverage.

Shared ND to Local CUBE TMOV supports the M16/M32 conversion layouts while
preserving Shared backing dtype. Indexed TLSU byte bounds are signed i64
relative byte bounds, not an implicit row stride. An explicit zero bound is
preserved with the nonzero-register ``R`` inline-assembly constraint. Range
records and execution-mask records have distinct roles.

Indexed ranges are a separately pinned experimental extension from PTO-SPEC
PR #424 at ``1981bceffa820e6dc0e56d61b2bb2cfd580e0759``. That proposal is
not merged in the normative baseline above. Its presence is not evidence that
the baseline assigns these semantics. Consumers must opt into the matching
platform/model extension; the baseline development identity does not certify it.

The accepted common-entry startup contract from PTO-SPEC #415 requires all
four contexts to enter the same runtime entry, PE0-only shared initialization,
and a single startup SYNC.CORE site before work. Compiler regressions preserve
that role branch and ordering. Stack/TLS bootstrap, constructors, four actual
contexts, invocation lifetime and loader admission remain runtime/platform
responsibilities; compilation does not establish their correctness.

Predicate and calling-ABI boundaries
-----------------------------------

The GPR-predicate element path and experimental PredicateCell element path
are distinct. PredicateCell atomic-add, unordered scatter, VP scatter and
floating negation remain rejected by the element legalizer. Direct native
PredicateTile indexed TLSU intrinsics are available separately; they do not
implicitly make arbitrary scalar element effects legal. Rejection must not
be replaced with unmasked memory accesses or speculative side effects.

The Local Tile ``linx-local-tile-abi`` is restricted to its explicit U32 NORM
profiles and markers. It is not a general SharedTile ABI. Shared handles must
remain in Shared-aware SSA lifetimes; ordinary stack spills, GPR copies and
ordinary C++ parameter/return transport are not supported. Noinline typed-view
helpers require a separate storage/alias/lifetime contract. See
``PTOElementHelpers.rst`` for the current inlining requirements.

ELF identity admission
----------------------

``llvm-mc -linxv5-emit-pto-isa-identity`` and
``llc -linxv5-emit-pto-isa-identity -filetype=obj`` emit one allocated,
four-byte-aligned ``.note.pto.isa`` note. Clang accepts the same LLVM option
through ``-mllvm``. The descriptor is canonical JSON without a trailing NUL;
the owner is ``PTO`` and the type is 1. A preexisting identity section is
rejected instead of silently overriding its contents. Assembly-text output
does not embed the note; enable the option again when assembling that text.

``ld.lld --pto-isa-identity`` requires exactly one matching identity on every
participating object, rejects missing/malformed/mismatched identities, and
retains one note in the output, including relocatable links. This explicit
mode currently accepts only little-endian ELF64 LinxV5 object inputs, not
shared-library inputs. Unselected archive members are not participating inputs.
Default legacy object emission/linking remains unchanged.

These opt-in mechanisms identify the intended encoding contract; they are
not hardware or runtime conformance certification. The loader/platform must
independently validate admission and execution behavior. Updating the pinned
fingerprint requires reviewing the normative and encoding deltas, rebuilding
the compiler and regenerating its regression evidence.
