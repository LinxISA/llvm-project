// RUN: not %clang --target=linx64 -O2 -mlxbc -S -o - %s 2>&1 \
// RUN:   | FileCheck %s --dump-input always -vv

// clang-format off
int errors() {
  asm volatile (
    "addi a0, 1, ->a1"
    ::
  );
  return 0;
}

// ASM-LABEL: _Z11simt_errorsv:
void __vec__ simt_errors() {
// CHECK-NOT: error: inline-asm should start from BSTART or Tile Call
// CHECK: error: simt inline-asm only accept micro instruction
  asm volatile (
    "BSTART\n"
    "addi a0, 1, ->t"
    ::
  );
// CHECK-DAG: error: simt inline-asm in multi-instructions should ends at l.bstop.
// CHECK-DAG: warning: The l.bstop in simt inline-asm returns the simt function. Please write whole simt function in inline-asm or embed single-instruction inline-asm to represent a specific operation.
  asm volatile (
    "l.addi t#1.sd, 1, ->t.d\n"
    "l.addi t#1.sd, 1, ->t.d\n"
    ::
  );
}
