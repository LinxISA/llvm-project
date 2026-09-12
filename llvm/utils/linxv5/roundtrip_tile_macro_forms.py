#!/usr/bin/env python3
"""Prove objdump macro output reassembles to identical .text bytes."""

from pathlib import Path
import re
import subprocess
import sys
import tempfile

if len(sys.argv) != 4:
    raise SystemExit("usage: roundtrip_tile_macro_forms.py LLVM_MC LLVM_OBJDUMP SOURCE")
mc, objdump, source = sys.argv[1:]

def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout

def section_bytes(path):
    dump = run(objdump, "-s", "-j", ".text", path)
    result = bytearray()
    for line in dump.splitlines():
        match = re.match(r"^\s*[0-9a-f]+\s+((?:[0-9a-f]{8}(?:\s+|$))+)", line)
        if not match:
            continue
        for word in match.group(1).split():
            result.extend(bytes.fromhex(word))
    return bytes(result)

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    original = root / "original.o"
    replay_source = root / "replay.s"
    replay = root / "replay.o"
    subprocess.run(
        [mc, "-triple=linx64v5", "-filetype=obj", source, "-o", original],
        check=True,
    )
    disassembly = run(objdump, "-d", "--no-show-raw-insn", original)
    lines = [".text"]
    for line in disassembly.splitlines():
        match = re.match(r"^\s*[0-9a-f]+:\s+(\S.*)$", line)
        if match:
            lines.append(match.group(1))
    replay_source.write_text("\n".join(lines) + "\n")
    subprocess.run(
        [mc, "-triple=linx64v5", "-filetype=obj", replay_source, "-o", replay],
        check=True,
    )
    before = section_bytes(original)
    after = section_bytes(replay)
    if before != after:
        mismatch = next(
            (index for index, pair in enumerate(zip(before, after)) if pair[0] != pair[1]),
            min(len(before), len(after)),
        )
        raise SystemExit(
            f"TileOp macro round trip differs at .text byte {mismatch}: "
            f"original={len(before)} replay={len(after)}"
        )
    print(f"TileOp macro round trip preserved {len(before)} .text bytes")
