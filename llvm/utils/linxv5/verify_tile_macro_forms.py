#!/usr/bin/env python3
"""Verify exact folding coverage for generated PTO TileOp forms."""

from __future__ import annotations

from pathlib import Path
import re
import sys


if len(sys.argv) != 3:
    raise SystemExit("usage: verify_tile_macro_forms.py SOURCE DISASSEMBLY")

source = Path(sys.argv[1]).read_text().splitlines()
disassembly = Path(sys.argv[2]).read_text().splitlines()
forms = []
for line in source:
    match = re.match(
        r"# FORM: fold=([01]) canonical=([01]) spelling=(\S+) operation=(\S+)$", line
    )
    if match:
        forms.append(
            {
                "fold": match.group(1) == "1",
                "canonical": match.group(2) == "1",
                "spelling": match.group(3),
                "operation": match.group(4),
            }
        )

results = []
for line in disassembly:
    macro = re.match(r"^\s*[0-9a-f]+:\s+([A-Z][A-Z0-9_.]+)\s+<", line)
    if macro:
        results.append(("macro", macro.group(1)))
        continue
    physical = re.match(
        r"^\s*[0-9a-f]+:\s+BSTART\.(?:TEPL|TLSU|CUBE|GMOV)\s+([A-Z0-9_.]+)",
        line,
    )
    if physical:
        results.append(("physical", physical.group(1)))

if len(forms) != 142:
    raise SystemExit(f"expected 142 generated forms, found {len(forms)}")
operations = {form["operation"] for form in forms}
if len(operations) != 117:
    raise SystemExit(f"expected 117 TileOp operations, found {len(operations)}")
if len(results) != len(forms):
    raise SystemExit(
        f"expected {len(forms)} disassembly results, found {len(results)}"
    )

for index, (form, result) in enumerate(zip(forms, results, strict=True)):
    expected = ("macro", form["spelling"])
    matches = result == expected
    if not matches:
        raise SystemExit(
            f"form {index} {form['spelling']}: expected {expected}, found {result}"
        )

folds = sum(form["fold"] for form in forms)
if folds != 142:
    raise SystemExit(f"expected 142 macro-foldable source forms, found {folds}")
canonical = sum(form["canonical"] for form in forms)
if canonical != 138:
    raise SystemExit(f"expected 138 canonical physical schemas, found {canonical}")
print(
    f"verified 142 PTO TileOp forms: {folds} macro folds, "
    f"{canonical} canonical physical schemas, 0 physical fallbacks"
)
