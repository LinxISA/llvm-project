#!/usr/bin/env python3
"""Verify wrapped LinxV5 macro bytes and opcode-column alignment."""

import re
import sys


lines = sys.stdin.read().splitlines()
records = []
section = None
for index, line in enumerate(lines):
    if re.match(r"^Disassembly of section .+:$", line):
        section = index
        continue
    match = re.match(
        r"^\s*([0-9a-f]+):\s+"
        r"((?:[0-9a-f]{4}|[0-9a-f]{8})"
        r"(?:\s+(?:[0-9a-f]{4}|[0-9a-f]{8}))*)\s+(\S+)",
        line,
    )
    if match:
        encoded_words = match.group(2).split()
        records.append(
            {
                "line": index,
                "section": section,
                "address": int(match.group(1), 16),
                "bytes": sum(len(word) // 2 for word in encoded_words),
                "opcode": match.group(3),
                "column": match.start(3),
            }
        )

tile_macro_opcode = re.compile(
    r"^(?:T[A-Z][A-Z0-9_.]*|GMOV|MGATHER[A-Z0-9_.]*|MSCATTER[A-Z0-9_.]*)$"
)
macros = [record for record in records if tile_macro_opcode.fullmatch(record["opcode"])]
if not macros:
    raise SystemExit("no TileOp macro lines found")

for record in macros:
    line = record["line"] + 1
    while line < len(lines):
        words = lines[line].split()
        if not (1 <= len(words) <= 2) or not all(
            re.fullmatch(r"[0-9a-f]{4}|[0-9a-f]{8}", word) for word in words
        ):
            break
        record["bytes"] += sum(len(word) // 2 for word in words)
        line += 1
    next_record = next(
        (
            candidate
            for candidate in records
            if candidate["line"] > record["line"]
            and candidate["section"] == record["section"]
        ),
        None,
    )
    if next_record and record["bytes"] != next_record["address"] - record["address"]:
        raise SystemExit(
            f"{record['opcode']} byte width {record['bytes']} does not match "
            f"address delta {next_record['address'] - record['address']}"
        )

columns = {record["column"] for record in records}
if len(columns) != 1:
    raise SystemExit(f"LinxV5 opcodes do not share one column: {sorted(columns)}")
opcode_column = columns.pop()

print(
    f"verified {len(macros)} wrapped TileOp encodings at opcode column "
    f"{opcode_column + 1}; all {len(records)} LinxV5 opcodes aligned"
)
