import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--clang', required=True)
parser.add_argument('--opt', required=True)
parser.add_argument('--host-clang', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[5]
source = root / 'clang/test/LinxV5/pto-element-grain-codegen.cpp'
output = Path(args.output)
output.mkdir(parents=True, exist_ok=True)
harness = output / 'reference.cpp'
harness.write_text(r'''
extern "C" void typed(const unsigned *, const unsigned *, unsigned *);
extern "C" void integer_ops(const unsigned *, unsigned *);
extern "C" void indexed(const unsigned *, const unsigned *, unsigned *);
int main() {
  unsigned input[192], other[192], indices[192], result[192];
  for (unsigned index = 0; index < 192; ++index) {
    input[index] = (index * 13) % 113;
    other[index] = (index * 7) % 5;
    indices[index] = (index * 3) % (COUNT + 7);
    result[index] = 0x12345678;
  }
  typed(input, other, result + 16);
  for (unsigned index = 0; index < 192; ++index) {
    unsigned expected = 0x12345678;
    if (index >= 16 && index < COUNT + 16) {
      unsigned element = index - 16;
      expected = ((input[element] > 2) | (other[element] < 1))
                     ? input[element] + other[element]
                     : input[element] - other[element];
    }
    if (result[index] != expected) return 1;
    result[index] = 0x12345678;
  }
  integer_ops(input, result + 16);
  for (unsigned index = 0; index < 192; ++index) {
    unsigned expected = 0x12345678;
    if (index >= 16 && index < COUNT + 16) {
      unsigned element = index - 16, value = input[element];
      expected = ((value > 3) & (value < 100))
                     ? ((value << 1) | element) ^ 7
                     : (value & 15) + element;
    }
    if (result[index] != expected) return 2;
    result[index] = 0x12345678;
  }
  indexed(input, indices, result + 16);
  for (unsigned index = 0; index < 192; ++index) {
    unsigned expected = 0x12345678;
    if (index >= 16 && index < COUNT + 16) {
      unsigned element = index - 16;
      if (indices[element] < COUNT)
        expected = input[indices[element]] + element;
    }
    if (result[index] != expected) return 3;
  }
}
''')

def run(command):
    subprocess.run([str(argument) for argument in command], check=True)

for grain in (128, 256, 512, 1024, 2048, 4096):
    for count in (1, 31, 33, 65, 129):
        directory = output / ('grain%d-count%d' % (grain, count))
        directory.mkdir(exist_ok=True)
        raw = directory / 'raw.ll'
        wide = directory / 'predicated.ll'
        portable = directory / 'portable.ll'
        expanded = directory / 'expanded.ll'
        binary = directory / 'reference'
        run([args.clang, '--target=linx64v5', '-mlxbc', '-O1', '-Xclang',
             '-disable-llvm-passes', '-DHOST_TEST', '-DGRAIN=' + str(grain),
             '-DCOUNT=' + str(count), '-S', '-emit-llvm', source, '-o', raw])
        run([args.opt, '-mtriple=linx64v5',
             '-passes=mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication,verify',
             '-verify-each', '-S', raw, '-o', wide])
        text = wide.read_text()
        text = re.sub(r'^\s*call void @llvm\.linx\.experimental\.element\.region\(metadata !\d+\).*\n',
                      '', text, flags=re.MULTILINE)
        text = re.sub(r'^declare void @llvm\.linx\.experimental\.element\.region\(metadata\).*\n',
                      '', text, flags=re.MULTILINE)
        assert '@llvm.linx.' not in text
        text = re.sub(r'^target (triple|datalayout) = .*\n', '', text,
                      flags=re.MULTILINE)
        text = re.sub(r'"target-features"="[^"]*"', '', text)
        portable.write_text(text)
        run([args.opt, '-enable-new-pm=0', '-expandvp', '-verify',
             '-expandvp-override-evl-transform=Discard',
             '-expandvp-override-mask-transform=Convert',
             '-S', portable, '-o', expanded])
        run([args.host_clang, '-O0', '-DCOUNT=' + str(count), expanded,
             harness, '-o', binary])
        run([binary])
print('PASS: 30 generic VP host executions; not Tile model execution')
