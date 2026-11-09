// RUN: %clang++ --target=linx64v5 -mcpu=janus -O2 -emit-llvm -S %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mcpu=janus -O3 -c %s -o %t.o
// RUN: llvm-objdump -dr --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

extern "C" void shared_initialize();
extern "C" void invocation_work(unsigned);

extern "C" void common_runtime_entry() {
  unsigned thread = __builtin_linx_get_thread_idx();
  if (thread == 0)
    shared_initialize();
  __builtin_linx_sync_core();
  invocation_work(thread);
}

// IR-LABEL: define{{.*}}@common_runtime_entry
// IR: call{{.*}}@llvm.linx.get.thread.id
// IR: icmp eq
// IR: br i1
// IR: call void @shared_initialize
// IR: call void @llvm.linx.sync.core
// IR: call void @invocation_work
// OBJ-LABEL: <common_runtime_entry>:
// OBJ: ssrget
// OBJ: CALL
// OBJ: sync.core
// OBJ: invocation_work
