// RUN: %clang --target=linx64v5 -O2 -c %s -o %t.o
// RUN: %clang --target=linx64v5 -O2 -mlinx-runtime-intra-abi -c %s -o %t.runtime.o
// RUN: env MALLOC_PERTURB_=85 %clang --target=linx64v5 -O2 -mlinx-runtime-intra-abi -c %s -o %t.perturbed.o
// RUN: not %clang --target=linx64v5 -mcpu=simt -O2 -c %s -o %t.simt.o 2>&1 | FileCheck %s --check-prefix=SIMT

// File-scope assembly uses MCSubtargetInfo, rather than a codegen subtarget.
// These scalar CRT block headers must not be misclassified as SIMT assembly.
__asm__(".text\n"
        ".globl _start\n"
        "_start:\n"
        "C.BSTART\n"
        "addi sp, 0, ->t\n"
        "addi t#1, 0, ->a0\n"
        "BSTART DIRECT _start_c\n");

// Keep this translation unit free of C functions, so the SIMT check only
// exercises the module-level assembly parser.
// SIMT: error: simt inline-asm only accept micro instruction.
