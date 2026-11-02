import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--clang', required=True)
parser.add_argument('--source', required=True)
parser.add_argument('--output', required=True)
args = parser.parse_args()
output = Path(args.output)
output.mkdir(parents=True, exist_ok=True)
for grain in (128, 256, 512, 1024, 2048, 4096):
    for number, dtype in enumerate(('unsigned', 'unsigned short',
                                    'unsigned long', 'float', '_Float16')):
        subprocess.run([args.clang, '--target=linx64v5', '-mlxbc', '-O2',
                        '-mllvm', '-linxv5-enable-generic-element=true',
                        '-DGRAIN=' + str(grain), '-DTYPE=' + dtype,
                        '-c', args.source, '-o',
                        str(output / ('grain%d-type%d.o' % (grain, number)))],
                       check=True)
for count in (15, 16, 17, 31, 32, 33, 63, 64, 65):
    subprocess.run([args.clang, '--target=linx64v5', '-mlxbc', '-O2',
                    '-mllvm', '-linxv5-enable-generic-element=true',
                    '-DGRAIN=128', '-DCOUNT=' + str(count),
                    '-c', args.source, '-o',
                    str(output / ('tail%d.o' % count))], check=True)
