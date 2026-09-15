#!/usr/bin/env python3
"""Require every generated TileOp form to reject a missing required binding."""

from __future__ import annotations

from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile


if len(sys.argv) != 3:
    raise SystemExit(
        "usage: verify_tile_macro_required_operands.py LLVM_MC SOURCE"
    )

mc, source_path = sys.argv[1:]
source_lines = Path(source_path).read_text().splitlines()
macro_lines = [
    line
    for line in source_lines
    if re.match(r"^[A-Z][A-Z0-9_.]*\s+<", line)
]
if len(macro_lines) != 142:
    raise SystemExit(f"expected 142 macro lines, found {len(macro_lines)}")


def split_top_level(text: str) -> list[str]:
    parts: list[str] = []
    start = 0
    angle = square = paren = 0
    for index, char in enumerate(text):
        if char == "<":
            angle += 1
        elif char == ">":
            angle -= 1
        elif char == "[":
            square += 1
        elif char == "]":
            square -= 1
        elif char == "(":
            paren += 1
        elif char == ")":
            paren -= 1
        elif char == "," and angle == square == paren == 0:
            parts.append(text[start:index].strip())
            start = index + 1
    parts.append(text[start:].strip())
    return parts


mutations = []
mutation_forms = []
for form_index, line in enumerate(macro_lines):
    parts = split_top_level(line)
    if len(parts) >= 2:
        for operand_index in range(1, len(parts)):
            mutations.append(
                ", ".join(
                    part for index, part in enumerate(parts)
                    if index != operand_index
                )
            )
            mutation_forms.append(form_index)
    begin = line.find("<")
    end = line.find(">", begin + 1)
    config = line[begin + 1 : end]
    fields = [field.strip() for field in config.split(",")]
    for field_index in range(len(fields)):
        mutations.append(
            line[: begin + 1]
            + ", ".join(
                field for index, field in enumerate(fields)
                if index != field_index
            )
            + line[end:]
        )
        mutation_forms.append(form_index)

with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "missing-required.s"
    path.write_text(".text\n" + "\n".join(mutations) + "\n")
    result = subprocess.run(
        [mc, "-triple=linx64v5", "-filetype=obj", path, "-o", os.devnull],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )

message = (
    "error: TileOp operands/configuration do not match any exact PTO 0.58.6 form"
)
rejected_forms = set()
for match in re.finditer(
    r"missing-required\.s:(\d+):\d+: " + re.escape(message), result.stderr
):
    line_number = int(match.group(1))
    mutation_index = line_number - 2
    if 0 <= mutation_index < len(mutation_forms):
        rejected_forms.add(mutation_forms[mutation_index])
if result.returncode == 0 or len(rejected_forms) != len(macro_lines):
    raise SystemExit(
        f"expected a missing-required-binding rejection for all "
        f"{len(macro_lines)} forms, found {len(rejected_forms)}\n{result.stderr}"
    )

print(
    f"verified required-binding rejection for {len(macro_lines)} TileOp forms "
    f"across {len(mutations)} single-field mutations"
)
