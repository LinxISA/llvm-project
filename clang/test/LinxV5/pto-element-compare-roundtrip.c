// RUN: %clang --target=linx64v5 -mlxbc -O2 -x ir -mllvm -enable-all-vector-as-tilereg=true -mllvm -linxv5-enable-clock-hand-opt=false -S %S/../../../llvm/test/CodeGen/LinxV5/elementwise-f32-compare-roundtrip.ll -o %t.s
// RUN: FileCheck %S/../../../llvm/test/CodeGen/LinxV5/elementwise-f32-compare-roundtrip.ll --check-prefix=ASM < %t.s
// RUN: %clang --target=linx64v5 -mlxbc -x assembler -c %t.s -o %t.roundtrip.o
// RUN: %clang --target=linx64v5 -mlxbc -O2 -x ir -mllvm -enable-all-vector-as-tilereg=true -mllvm -linxv5-enable-clock-hand-opt=false -c %S/../../../llvm/test/CodeGen/LinxV5/elementwise-f32-compare-roundtrip.ll -o %t.direct.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.roundtrip.o | FileCheck %S/../../../llvm/test/CodeGen/LinxV5/elementwise-f32-compare-roundtrip.ll --check-prefix=OBJ
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.direct.o | FileCheck %S/../../../llvm/test/CodeGen/LinxV5/elementwise-f32-compare-roundtrip.ll --check-prefix=OBJ
// RUN: llvm-objcopy --dump-section=.text=%t.direct.text %t.direct.o
// RUN: llvm-objcopy --dump-section=.text=%t.roundtrip.text %t.roundtrip.o
// RUN: cmp %t.direct.text %t.roundtrip.text
// Driver -S -> integrated assembler shares the twelve TCMP/TCMPS predicate
// vectors with llc/MC; exact .text parity also locks dense +0/mask/result slots.
