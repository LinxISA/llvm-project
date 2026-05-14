// RUN: %clang --target=linx64v5 -O2 -mlxbc -S -o - %s

// clang-format off
int corrects() {
  asm volatile (
    "BSTART\n"
    "addi a0, 1, ->a1"
    ::
  );
  return 0;
}

void __vec__ simt_corrects1() {
  asm volatile (
    "l.addi t#1.sd, 1, ->t.d\n"
    "l.addi t#1.sd, 1, ->t.d\n"
    "l.bstop\n"
  );
}

void __vec__ simt_corrects2() {
  asm volatile (
    "l.addi t#1.sd, 1, ->t.d"
  );
}
