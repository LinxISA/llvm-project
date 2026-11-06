#!/usr/bin/env python3
"""Exercise range/mask stream rejection using real bytes and macro folding."""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

mc, objdump = sys.argv[1:3]


def assemble(source, path):
    subprocess.run([mc, '-triple=linx64v5', '-filetype=obj', '-o', str(path)],
                   input=source, text=True, capture_output=True, check=True)


def listing(path, physical=False):
    args = [objdump, '-d', '--no-show-raw-insn']
    if physical:
        args += ['--disassembler-options=no-tile-macros']
    return subprocess.run(args + [str(path)], text=True, capture_output=True)


with tempfile.TemporaryDirectory() as temp:
    path = Path(temp) / 'range.o'
    if len(sys.argv) == 4:
        # Isolate the compiler's indexed bundles from unrelated transports so
        # default macro folding and replay are checked for the real emitter.
        physical = listing(sys.argv[3], True)
        assert physical.returncode == 0, physical.stderr
        indexed = []
        active = False
        for line in physical.stdout.splitlines():
            match = re.match(r'^\s*[0-9a-f]+:\s+(.*)$', line)
            if not match:
                continue
            command = match[1]
            if 'BSTART' in command:
                active = bool(re.search(r'BSTART.TLSU\s+M(?:GATHER|SCATTER)', command))
            if active:
                indexed.append(command)
        assert indexed, physical.stdout
        assemble('\n'.join(indexed) + '\n', path)
        folded = listing(path)
        assert folded.returncode == 0, folded.stderr
        assert 'PredInv=0' in folded.stdout and 'PredInv=1' in folded.stdout
        replay = [match[1] for line in folded.stdout.splitlines()
                  if (match := re.match(r'^\s*[0-9a-f]+:\s+(.*)$', line))]
        assemble('\n'.join(replay) + '\n', path)
        # Canonical macros may move IOR before IOT and elide unit dimensions.
        # Their operand/configuration text must remain identical after replay.
        again = listing(path)
        assert again.returncode == 0, again.stderr
        canonical = [match[1] for line in again.stdout.splitlines()
                     if (match := re.match(r'^\s*[0-9a-f]+:\s+(.*)$', line))]
        assert canonical == replay, 'compiler indexed macro operands changed'
        before = path.read_bytes()
        assemble('\n'.join(canonical) + '\n', path)
        assert path.read_bytes() == before, 'canonical indexed macros changed bytes'
        print('compiler indexed bundles fold and canonical replay is stable')
        sys.exit(0)
    assemble('MGATHER <Encoding=E5, Col=2, U32, CUBE_M32>, '
             '[a0, a1, a2], T#1, ->T<128B>, [a3]\n', path)
    original = listing(path)
    assert original.returncode == 0, original.stderr
    physical = listing(path, True)
    assert physical.returncode == 0, physical.stderr
    lines = [m[1] for line in physical.stdout.splitlines()
             if (m := re.match(r'^\s*[0-9a-f]+:\s+(.*)$', line))]
    range_pos = next(i for i, line in enumerate(lines)
                     if 'B.IOR' in line and 'ExecMaskPresent' not in line)
    assert 'ExecMaskPresent' in lines[range_pos + 1], lines
    cases = {}
    # Move an existing legal dimension between the pair: no duplicate fields.
    noncontiguous = lines.copy()
    dimension = next(i for i, line in enumerate(noncontiguous) if 'B.DIM' in line)
    command = noncontiguous.pop(dimension)
    pos = next(i for i, line in enumerate(noncontiguous) if 'ExecMaskPresent' in line)
    noncontiguous.insert(pos, command)
    cases['noncontiguous'] = noncontiguous
    cases['third-record'] = lines + ['B.IOR [a4], []']
    absent = lines.copy()
    absent[range_pos] = 'B.IOR [a0, zero, zero], []'
    cases['absent-range-with-second-mask'] = absent
    wrong_flags = lines.copy()
    wrong_flags[range_pos] = 'B.IOR [a0, a1, a2], ExecMaskPresent'
    cases['flagged-range'] = wrong_flags
    for name, invalid in cases.items():
        assemble('\n'.join(invalid) + '\n', path)
        rejected = listing(path)
        assert rejected.returncode != 0, (name, rejected.stdout)
        assert 'no complete macro instruction form' in rejected.stderr, (name, rejected.stderr)
    # Build malformed physical records to exercise the decoder independently
    # of the macro parser. Check the exact coverage edge in both layouts.
    shapes = [("U32", "CUBE_M32", 32, 2, 1),
              ("U32", "CUBE_M16", 16, 2, 1),
              ("U16", "CUBE_M16", 16, 4, 1),
              ("U8", "CUBE_M32", 32, 4, 2),
              ("U8", "CUBE_M16", 16, 8, 2)]
    for dtype, layout, rows, columns, words in shapes:
        for actual_rows, actual_columns, legal in [
                (rows, columns, True), (rows + 1, columns, False),
                (rows, columns + 1, False), (0, columns, False)]:
            source = (f'BSTART.TLSU MGATHER, {dtype}\n'
                      f'B.DATR {layout}, Zero\n'
                      f'B.DIM zero, {actual_columns}, ->lb0\n'
                      f'B.DIM zero, {actual_rows}, ->lb1\n'
                      'B.IOT t#1, mask=1111, last, ->t<256B>\n'
                      'B.IOR [a0, a1, a2], []\n'
                      f'B.IOR [a3, {"a4" if words == 2 else "zero"}, zero], ExecMaskPresent\n')
            assemble(source, path)
            result = listing(path)
            assert (result.returncode == 0) == legal, (
                dtype, layout, actual_rows, actual_columns, result.stderr)
    print('range/mask stream and shape boundaries verified')
