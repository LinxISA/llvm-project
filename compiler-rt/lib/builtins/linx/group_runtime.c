//===-- linx/group_runtime.c - Hosted four-PE group runtime --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the static, single-shot hosted SMT4 runtime. PE0 owns libc and
// process exit. PE1 through PE3 enter __linx_group_worker_start directly and
// call the application-provided __linx_group_worker_main.
//
//===----------------------------------------------------------------------===//

#include "../int_lib.h"
#include <stdint.h>

#define LINX_GROUP_PE_COUNT 4U
#define LINX_GROUP_EXPORT COMPILER_RT_ABI __attribute__((visibility("default")))

struct __attribute__((aligned(64))) LinxGroupControl {
  volatile uint32_t ready;
  volatile uint32_t done0;
  volatile uint32_t done1;
  volatile uint32_t done2;
  volatile uint32_t done3;
  volatile int32_t status0;
  volatile int32_t status1;
  volatile int32_t status2;
  volatile int32_t status3;
  void *context;
};

static struct LinxGroupControl control;

extern int __linx_group_worker_main(uint32_t pe_id, void *context);

static inline uint32_t current_pe(void) {
  uint32_t pe_id;
  __asm__ volatile("ssrget 0x802, ->%0" : "=r"(pe_id));
  return pe_id;
}

static inline void compiler_barrier(void) {
  __asm__ volatile("" : : : "memory");
}

static inline uint32_t load_published(const volatile uint32_t *value) {
  uint32_t data = *value;
  compiler_barrier();
  return data;
}

static inline void store_published(volatile uint32_t *value, uint32_t data) {
  compiler_barrier();
  *value = data;
}

LINX_GROUP_EXPORT int linx_group_run(void *context) {
  if (current_pe() != 0)
    return -1;

  control.status0 = 0;
  control.status1 = 0;
  control.status2 = 0;
  control.status3 = 0;
  control.done0 = 0;
  control.done1 = 0;
  control.done2 = 0;
  control.done3 = 0;
  control.context = context;
  store_published(&control.ready, 1);

  control.status0 = __linx_group_worker_main(0, context);
  store_published(&control.done0, 1);

  while (load_published(&control.done1) == 0) {
  }
  while (load_published(&control.done2) == 0) {
  }
  while (load_published(&control.done3) == 0) {
  }

  if (control.status0 != 0)
    return control.status0;
  if (control.status1 != 0)
    return control.status1;
  if (control.status2 != 0)
    return control.status2;
  if (control.status3 != 0)
    return control.status3;
  return 0;
}

LINX_GROUP_EXPORT NORETURN __attribute__((used, retain)) void
__linx_group_worker_start(void) {
  uint32_t pe_id = current_pe();
  if (pe_id == 0 || pe_id >= LINX_GROUP_PE_COUNT) {
    for (;;) {
    }
  }
  while (load_published(&control.ready) == 0) {
  }

  int status = __linx_group_worker_main(pe_id, control.context);
  if (pe_id == 1) {
    control.status1 = status;
    store_published(&control.done1, 1);
  } else if (pe_id == 2) {
    control.status2 = status;
    store_published(&control.done2, 1);
  } else {
    control.status3 = status;
    store_published(&control.done3, 1);
  }

  for (;;) {
  }
}
