// RUN: %clang --target=linx64v5 -x assembler -c %S/../../../llvm/test/MC/LinxV5/execution-mask-result-carrier.s -o %t.o
// RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t.o | FileCheck %S/../../../llvm/test/MC/LinxV5/execution-mask-result-carrier.s --check-prefix=DIS
// The integrated assembler consumes the same native encoding vectors as MC.
