// RUN: %clang --target=linx64v5-linux-musl -O2 -c %s -o %t.o
// RUN: %clang --target=linx64v5-linux-musl -mcpu=janus -O2 -c %s -o %t.janus.o
// RUN: not %clang --target=linx64v5-linux-musl -mcpu=simt -O2 -c %s -o %t.simt.o 2>&1 | FileCheck %s --check-prefix=SIMT

// musl's CRT uses file-scope inline asm for scalar process startup. Its MC
// subtarget must not be downcast to the code generator's LinxV5Subtarget.
// SIMT: error: simt inline-asm only accept micro instruction.
__asm__(".text\n"
        ".globl _start\n"
        ".type _start,@function\n"
        ".align 1\n"
        "_start:\n"
        "C.BSTART\n"
        "addi sp, 0, ->t\n"
        "andi t#1, -16, ->t\n"
        "addi t#1, 0, ->sp\n"
        "BSTART DIRECT _start_c\n");
