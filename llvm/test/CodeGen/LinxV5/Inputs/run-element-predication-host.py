#!/usr/bin/env python3
"""Execute the generic P2 IR stage on the host; this is not PTO E2E evidence.

Only this disposable test copy drops the retained target-legalization marker.
The production final verifier is separately tested to reject that marker.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--opt', required=True)
parser.add_argument('--clang', required=True)
parser.add_argument('--output-dir')
args = parser.parse_args()
here = Path(__file__).resolve().parent
fixture = here.parent / 'element-predication-cfg.ll'
out = Path(args.output_dir or tempfile.mkdtemp(prefix='pto-predication-host-'))
out.mkdir(parents=True, exist_ok=True)
commands = []


def run(command):
    commands.append([str(arg) for arg in command])
    subprocess.run(commands[-1], check=True)


wide = out / 'predicated.ll'
run([args.opt, '-mtriple=linx64v5',
     '-passes=loop-simplify,lcssa,linx-v5-element-predication,verify',
     '-verify-each', '-S', fixture, '-o', wide])
text = wide.read_text()
pattern = r'^\s*call void @llvm\.linx\.experimental\.element\.region\(metadata !\d+\).*\n'
text, count = re.subn(pattern, '', text, flags=re.MULTILINE)
assert count == 5, ('unexpected region fixture count', count)
text = re.sub(r'^declare void @llvm\.linx\.experimental\.element\.region\(metadata\).*\n',
              '', text, flags=re.MULTILINE)
assert '@llvm.linx.' not in text, 'test copy still contains target intrinsics'
text = re.sub(r'^target (triple|datalayout) = .*\n', '', text, flags=re.MULTILINE)
portable = out / 'portable-test-copy.ll'
portable.write_text(text)
expanded = out / 'expanded.ll'
run([args.opt, '-enable-new-pm=0', '-expandvp', '-verify',
     '-expandvp-override-evl-transform=Discard',
     '-expandvp-override-mask-transform=Convert',
     '-S', portable, '-o', expanded])
assert 'call ' not in ''.join(line for line in expanded.read_text().splitlines()
                              if '@llvm.vp.' in line), 'VP expansion incomplete'
exe = out / 'host-check'
run([args.clang, '-O2', expanded, here / 'element-predication-host.c', '-o', exe])
run([exe])
manifest = {'stage': 'P2 generic IR host semantics; not PTO codegen/gfrun/gfsim',
            'commands': commands,
            'hashes': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in (fixture, wide, portable, expanded, exe)}}
(out / 'evidence.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(out / 'evidence.json')
