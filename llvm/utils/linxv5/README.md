# LinxV5 TileOp macro catalog

Regenerate the checked-in MC tables from the LLVM-owned model:

```sh
python3 llvm/utils/linxv5/generate_tile_macro_catalog.py \
  --output llvm/lib/Target/LinxV5/MCTargetDesc/LinxV5TileMacroCatalog.inc
```

Add `--check` to verify that the generated file is current. The default input
is `tile_macro_catalog.json`, the expanded 0.58.7 catalog imported from LLVM
commit `9fb347b6f`, followed by the emitter corrections in `tile_macro_model.py`.
The JSON `fields` object documents its positional record schema. The generated
digest covers the complete expanded model, including those corrections.
`--catalog PATH` retains the original external PTO catalog import workflow.

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
