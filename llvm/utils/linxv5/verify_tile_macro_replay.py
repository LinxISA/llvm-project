#!/usr/bin/env python3
"""Check macro replay and physical binder/attribute preservation.

Dimension-one defaults, compressed dimensions and cross-kind command ordering
are canonicalized. Operand order within a command kind remains significant.
"""

from pathlib import Path
import re
import subprocess
import sys
import tempfile


def run(*args, input=None):
    return subprocess.run(
        args, input=input, text=True, capture_output=True, check=True
    ).stdout


def records(text):
    result = {}
    current = None
    for line in text.splitlines():
        match = re.search(r"<MCInst #\d+ (\w+)", line)
        if match:
            current = []
            result.setdefault(match[1], []).append(current)
        match = re.search(r"<MCOperand (Imm|Reg):(-?\d+)", line)
        if match and current is not None:
            current.append((match[1], int(match[2])))
    # Physical compression and dimension-one omission are equivalent.
    dims = result.pop("C_B_DIMI", []) + result.pop("B_DIM", [])
    # Multiple bundles require order within each slot, including default ones.
    for dim in dims:
        slot = dim[0][1]
        value = [("Reg", 38), dim[1]] if len(dim) == 2 else dim[1:]
        if value != [("Reg", 38), ("Imm", 1)]:
            result.setdefault("DIM" + str(slot), []).append(value)
    # A completely default attribute command can be omitted by the parser.
    default_datr = [("Imm", x) for x in [0, 0, 31, 3, 0, 0, 0, 0]]
    for name, default in [("BDATR", default_datr), ("B_FPATR", [("Imm", 0)] * 10)]:
        values = [value for value in result.pop(name, []) if value != default]
        if values:
            result[name] = values
    return result


def main():
    if len(sys.argv) != 5:
        raise SystemExit("usage: verify_tile_macro_replay.py MC OBJDUMP OBJCOPY SOURCE")
    mc, objdump, objcopy, source = sys.argv[1:]
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        original, replay = root / "original.o", root / "replay.o"
        run(mc, "-triple=linx64v5", "-filetype=obj", source, "-o", str(original))
        listing = run(objdump, "-d", "--no-show-raw-insn", str(original))
        lines = [".text"]
        for line in listing.splitlines():
            match = re.match(r"^\s*[0-9a-f]+:\s+(\S.*)$", line)
            if match:
                lines.append(match[1])
        run(
            mc,
            "-triple=linx64v5",
            "-filetype=obj",
            "-o",
            str(replay),
            input="\n".join(lines) + "\n",
        )
        decoded = []
        for obj in [original, replay]:
            binary = obj.with_suffix(".bin")
            run(objcopy, "-O", "binary", "--only-section=.text", str(obj), str(binary))
            data = "[" + " ".join(hex(x) for x in binary.read_bytes()) + "]"
            decoded.append(
                run(
                    mc,
                    "--disassemble",
                    "--show-inst",
                    "--triple=linx64v5",
                    "--linxv5-no-aliases",
                    input=data,
                )
            )
        before, after = map(records, decoded)
        if before != after:
            differences = {
                key: (before.get(key), after.get(key))
                for key in before.keys() | after.keys()
                if before.get(key) != after.get(key)
            }
            raise SystemExit(f"macro replay changed physical records: {differences}")
        print("macro replay preserved physical operands and attributes")


if __name__ == "__main__":
    main()
