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
for line in macro_lines:
    parts = split_top_level(line)
    if len(parts) >= 2:
        mutations.append(", ".join(parts[:-1]))
        continue
    begin = line.find("<")
    end = line.find(">", begin + 1)
    config = line[begin + 1 : end]
    fields = [field.strip() for field in config.split(",")]
    if len(fields) < 2:
        raise SystemExit(f"form has no removable required field: {line}")
    mutations.append(line[: begin + 1] + ", ".join(fields[:-1]) + line[end:])

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
rejections = result.stderr.count(message)
if result.returncode == 0 or rejections != len(mutations):
    raise SystemExit(
        f"expected {len(mutations)} missing-binding rejections, "
        f"found {rejections}\n{result.stderr}"
    )

print(f"verified required-binding rejection for {len(mutations)} TileOp forms")
