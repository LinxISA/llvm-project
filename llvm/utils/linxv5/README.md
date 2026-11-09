# LinxV5 TileOp macro catalog

Regenerate the checked-in MC tables from the LLVM-owned model:

```sh
python3 llvm/utils/linxv5/generate_tile_macro_catalog.py \
  --output llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileMacroCatalog.inc
```

Add `--check` to verify that the generated file is current. The default input
is `tile_macro_catalog.json`, the retained 0.58.7 emitter seed imported from LLVM
commit `9fb347b6f`, followed by the emitter corrections in `tile_macro_model.py`.
The JSON `fields` object documents its positional record schema. The generated
digest covers the complete expanded model, including those corrections.
`--catalog PATH` retains the original external PTO catalog import workflow.

The expanded model records architecture 0.59.0 and the source digest from
PTO-SPEC `42067d0f2890`, projected into `pto_0590_catalog.json`. Its 24 atomic
TSTORE aliases are appended with explicit Encoding IDs, preserving existing
emitter spellings and IDs. This is a compatibility model, not an exact copy of
all normative grammar: historical TEPL Layout configurations and MX emitter
alternatives remain. The verifier checks headers and the migrated atomic
binders, not complete compiler conformance:

```sh
python3 llvm/utils/linxv5/import_pto_catalog.py \
  /path/to/pto-spec/spec/catalog/tile-macro-assembly.json \
  llvm/utils/linxv5/pto_0590_catalog.json
```

Use `--check` with the importer to verify the projection against the pinned
source. See `llvm/docs/PTO059Alignment.rst` for identity and support boundaries.

`Encoding=E<number>` selects an alternative physical binder grouping within an
operation. Existing source spellings retain their default grouping. Append new
forms rather than reordering existing alternatives. The disassembler requires
one complete matching form; unknown records, duplicate dimensions, inconsistent
PE masks and invalid `last` markers continue to produce coverage errors.

Multi-member tile/shared groups accept `[operand0, operand1]`, preserving each
physical binder independently. Repeated legacy operands remain valid shorthand.
New MX ACC shared/local forms name transport roles because the original matrix
storage provenance cannot be recovered from an otherwise identical byte stream.

The benchmark regressions exercise TRACE boundaries, post-binder dimensions,
combined parent binders, CCTRL, matrix output groups, execution masks, TLEA and
TIMG2COL. Physical replay checks may canonicalize dimension-one defaults,
compressed dimension encodings and independent command kinds. Range modifiers
remain attached to their original binders.

The indexed TLSU range extension is pinned separately to PTO-ISA/pto-spec
PR #424, head `1981bceffa820e6dc0e56d61b2bb2cfd580e0759`. It appends
forms for all 23 indexed operations; the historical seed remains 0.58.7.
Bounds are signed i64 byte displacements of access starts, not element
indices or GM strides. An explicit zero range must use at least one nonzero
bound selector; inline asm can use the `R` constraint for Min. The compiler
feature macro is `__linx_indexed_tlsu_range=1`. Range intrinsics append Min
and Max to their legacy signature and retain the legacy profile checks.
Range plus GPR mask uses adjacent unflagged range / flagged mask-only IORs;
PredicateTile needs only the range IOR. No range inference is performed.
