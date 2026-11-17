#!/usr/bin/env python3
"""Check every pinned scoped mnemonic against llvm-mc and llvm-objdump."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile


def run(args, text=None):
    p = subprocess.run([str(x) for x in args], input=text, text=True,
                       capture_output=True)
    if p.returncode:
        raise RuntimeError(f'{args}:\n{p.stderr}')
    return p.stdout


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin-dir', type=Path)
    args = p.parse_args()
    if args.bin_dir is None:
        import shutil
        args.bin_dir = Path(shutil.which('llvm-mc')).resolve().parent
    manifest = json.loads(Path(__file__).with_name('scoped-scalar-forms.json').read_text())
    # No mutable source-tree or adjacent checkout is needed at test time.
    raw, forms = [], []
    for form in manifest['forms']:
        match = sum(int(e['match'], 16) << (e['index'] * 32)
                    for e in form['encoding'])
        mask = sum(int(e['mask'], 16) << (e['index'] * 32)
                   for e in form['encoding'])
        fields = form['ordering_fields']
        for flags in range(1 << len(fields)):
            ordering = 0
            suffix = ''
            selected = set()
            for index, field in enumerate(fields):
                if flags & (1 << index):
                    selected.add(field['name'])
                    for piece in field['pieces']:
                        ordering |= 1 << piece['instruction_lsb']
            if selected:
                suffix = '.' + ('aq' if 'aq' in selected else '') + ('rl' if 'rl' in selected else '') + ('f' if 'far' in selected else '')
            for pattern in [0, 0x000000008080]:
                payload = match | ordering | (pattern & ~mask & ((1 << form['length_bits']) - 1))
                for scope in range(3):
                    code = (payload << 16) | (({16: 0, 32: 1, 48: 2}[form['length_bits']]) << 10) | (scope << 8) | 31
                    raw.append('.quad 0x%016x' % code)
                    forms.append((form, scope, code, suffix))
    with tempfile.TemporaryDirectory() as temp:
        obj = Path(temp)/'all.o'
        run([args.bin_dir/'llvm-mc', '-triple=linx64v5', '-filetype=obj', '-o', obj], '\n'.join(raw))
        dis = run([args.bin_dir/'llvm-objdump', '-d', '--no-show-raw-insn', obj])
        lines = [m.group(1) for line in dis.splitlines()
                 if (m := re.match(r'^\s*[0-9a-f]+:\s+(.*)$', line))]
        assert len(lines) == len(forms), (len(lines),len(forms))
        assembly=[]
        for index,(line,(form,scope,code,suffix)) in enumerate(zip(lines,forms)):
            mnemonic=form['mnemonic'].lower()+suffix+['.private','.intra','.inter'][scope]
            assert line.split()[0].lower()==mnemonic,(form['form_id'],line)
            line = re.sub(r"\s+<[^>]+>", "", line)
            if '.pcr.' in mnemonic:
                # objdump renders outer-PC-relative immediates as addresses.
                line = re.sub(r"\[0x([0-9a-f]+)\]",
                              lambda m: '[' + str((int(m.group(1), 16) - index * 8 + (1 << 63)) % (1 << 64) - (1 << 63)) + ']', line)
            assembly.append(line)
        enc=run([args.bin_dir/'llvm-mc','-triple=linx64v5','-show-encoding'], '\n'.join(assembly))
        encodings=re.findall(r'encoding: \[([^]]+)\]',enc)
        assert len(encodings)==len(forms)
        for encoded,(_,_,expected,_) in zip(encodings,forms):
            actual=bytes(int(b.strip(),16) for b in encoded.split(','))
            assert actual==expected.to_bytes(8,'little'),(encoded,hex(expected))
    print(f'{len(forms)} scoped witnesses: normative mask/match, decode, assembly and byte roundtrip passed')


if __name__=='__main__': main()
