import sys


output, mode = sys.argv[1:]
parent_count = 512 if mode == "many" else 4 if mode == "overflow" else 2
block_count = 65537 if mode == "long" else 1
init = ('    INLINEASM &"BSTART.TEPL 27, 17\\0AB.DATR CUBE_M32, RNONE'
        '\\0AB.DIM zero, 1, ->lb0\\0AB.DIM zero, 32, ->lb1'
        '\\0AB.IOT $1, mask=1111, last, ->$0<${2:Z}>'
        '\\0AB.ASSEMBLE ${3:c}, ${4:c}, zero, 0, 1\\0A", '
        '9, 720906, def %PARENT, 720905, %0, 13, 2, 13, 1, 13, 0\n')
last = ('    INLINEASM &"BSTART.TEPL 27, 17\\0AB.DATR CUBE_M32, RNONE'
        '\\0AB.DIM zero, 1, ->lb0\\0AB.DIM zero, 32, ->lb1'
        '\\0AB.IOT $1, mask=1111, last\\0AB.IOT $0, mask=1111'
        '\\0AB.ASSEMBLE ${2:c}, ${3:c}, zero, 1, 1\\0A", '
        '9, 720905, %PARENT, 720905, %0, 13, 0, 13, 1\n')


def event(template, parent):
    return template.replace("PARENT", str(parent))


with open(output, "w") as stream:
    stream.write("--- |\n  define void @linear_parents() { ret void }\n...\n")
    stream.write("---\nname: linear_parents\ntracksRegLiveness: true\n")
    stream.write("body: |\n  bb.0:\n")
    if block_count > 1:
        stream.write("    successors: %bb.1\n")
    stream.write("    %0:tile_abs = IMPLICIT_DEF\n")
    for parent in range(1, parent_count + 1):
        stream.write("    %{}:tile_abs = IMPLICIT_DEF\n".format(parent))
    if mode == "invalid":
        stream.write(event(last, 1))
    if mode in ("overlap", "overflow"):
        for parent in range(1, parent_count + 1):
            stream.write(event(init, parent))
        for parent in range(1, parent_count + 1):
            stream.write(event(last, parent))
    else:
        for parent in range(1, parent_count + 1):
            stream.write(event(init, parent))
            if mode != "long":
                stream.write(event(last, parent))
    for block in range(1, block_count):
        stream.write("    PseudoBR %bb.{}\n  bb.{}:\n".format(block, block))
        if block + 1 < block_count:
            stream.write("    successors: %bb.{}\n".format(block + 1))
    if mode == "long":
        for parent in range(1, parent_count + 1):
            stream.write(event(last, parent))
    stream.write("    PseudoRET\n...\n")
