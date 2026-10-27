import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text()
body = source.split("    INLINEASM", 1)[1].split("    PseudoRET", 1)[0]
lines = ("    INLINEASM" + body).splitlines()
print("--- |\n  define void @many_epochs() { ret void }\n...\n---")
print("name: many_epochs\ntracksRegLiveness: true\nbody: |\n  bb.0:\n    liveins: $r2")
print(lines[0])
for epoch in range(512):
    print("\n".join(lines[1:4]))
print("    PseudoRET\n...")
