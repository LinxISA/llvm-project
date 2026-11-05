# Check every assigned FENCE.D mask pair and C.SETRET immediate against
# the raw fields specified by PTO, then reassemble canonical disassembly.
import re
import subprocess
import sys

mc = sys.argv[1]

def run(args, text):
    return subprocess.run([mc, '-triple=linx64v5'] + args, input=text,
                          text=True, capture_output=True, check=True).stdout

source = '\n'.join('fence.d %d, %d' % (p, s)
                   for p in range(16) for s in range(16))
source += '\n' + '\n'.join('c.setret %d, ->ra' % i for i in range(32)) + '\n'
expected = [0x202b | (p << 24) | (s << 20)
            for p in range(16) for s in range(16)]
expected += [0x5016 | (i << 6) for i in range(32)]

def encodings(text):
    return [bytes(int(v.strip(), 16) for v in block.split(','))
            for block in re.findall(r'encoding: \[([^\]]+)\]', text)]

encoded = encodings(run(['-show-encoding'], source))
assert [int.from_bytes(b, 'little') for b in encoded] == expected
raw = '\n'.join(' '.join('0x%02x' % v for v in b) for b in encoded) + '\n'
decoded = run(['-disassemble'], raw)
assert 'unexpected' not in decoded
assert encodings(run(['-show-encoding'], decoded)) == encoded
print('256 FENCE.D pairs and 32 C.SETRET fields: encoding and roundtrip pass')
