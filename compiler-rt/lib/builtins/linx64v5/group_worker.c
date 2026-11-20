// Common-entry carrier for the hosted group ABI. This file intentionally has
// no libc dependency and never enters musl CRT. The launcher supplies one PC,
// one PE-local stack/TLS context per PE, and a shared group descriptor.

#if !defined(__linx)
#error "the Linx group worker runtime is target-only"
#endif

#define PTO_RUNTIME_INTRA
#include "../../../include/linx/pto_runtime.h"
#define GROUP_INTRA __attribute__((linx_scalar_shareability("intra")))

/* Only PE0 reads/writes this process-lifetime state. Workers never poll it. */
static volatile unsigned process_state;

extern int __pto_runtime_process_initialize(void)
    __attribute__((weak)) GROUP_INTRA;
extern int __pto_runtime_invocation_initialize(
    const struct pto_runtime_group_context *) __attribute__((weak)) GROUP_INTRA;

extern const struct pto_runtime_group_context *
__pto_runtime_group_context_get(void) __attribute__((weak)) GROUP_INTRA;
extern int __linx_group_pe_main(
    unsigned, const struct pto_runtime_group_context *) __attribute__((weak)) GROUP_INTRA;
extern void __pto_runtime_group_complete(
    const struct pto_runtime_group_context *, int)
    __attribute__((weak, noreturn)) GROUP_INTRA;

static __attribute__((noreturn)) void group_abort(void) {
  /* LLVM's generic trap expands to libc abort on this target. This runtime
   * has no libc dependency: use the accepted SYS software-breakpoint fault.
   * The platform must treat it as unrecoverable group failure. */
  __asm__ volatile("BSTART.SYS FALL\nEBREAK 0\nBSTOP\n" ::: "memory");
  for (;;) __asm__ volatile("" ::: "memory");
}

static __attribute__((noreturn)) void group_park(void) {
  for (;;) __asm__ volatile("" ::: "memory");
}

static inline void group_sync_core(void) {
  // Use the compiler-owned lowering so the runtime follows the same encoding,
  // scheduling and block-isolation rules as public SYNC_CORE users.
  __builtin_linx_sync_core();
}

static inline void group_store_state(volatile uint32_t *p, uint32_t value) {
  *p = value;
  __asm__ volatile("" ::: "memory");
}

static inline uint32_t group_load_state(const volatile uint32_t *p) {
  const uint32_t value = *p;
  __asm__ volatile("" ::: "memory");
  return value;
}

static inline void group_store_result(volatile int32_t *p, int32_t value) {
  *p = value;
  __asm__ volatile("" ::: "memory");
}

static inline int32_t group_load_result(const volatile int32_t *p) {
  const int32_t value = *p;
  __asm__ volatile("" ::: "memory");
  return value;
}

__attribute__((noreturn, used, visibility("default"))) void
__linx_group_entry(const struct pto_runtime_group_context *ctx) {
  const unsigned pe_id = __builtin_linx_get_thread_idx();
  if (!ctx || ctx->magic != PTO_RUNTIME_GROUP_CONTEXT_MAGIC ||
      ctx->version != PTO_RUNTIME_GROUP_CONTEXT_VERSION ||
      ctx->participant_count != PTO_RUNTIME_GROUP_PARTICIPANTS ||
      pe_id >= PTO_RUNTIME_GROUP_PARTICIPANTS || !ctx->state ||
      !ctx->result || !ctx->error)
    group_abort();

  /* PR #415: workers can reach this site's generation zero first. Only PE0
   * touches process/shared initialization, and arrives after success. */
  if (pe_id == 0) {
    int status = 0;
    if (process_state != 2) {
      if (process_state != 0)
        group_abort(); /* recursive/failed process init requires image reload */
      process_state = 1;
      if (__pto_runtime_process_initialize)
        status = __pto_runtime_process_initialize();
      if (status) {
        group_store_result(&ctx->result[0], status);
        group_store_state(&ctx->state[0], PTO_RUNTIME_GROUP_STATE_ABORT);
        *ctx->error = 1;
        group_abort();
      }
      process_state = 2;
    }
    if (__pto_runtime_invocation_initialize)
      status = __pto_runtime_invocation_initialize(ctx);
    if (status) {
      group_store_result(&ctx->result[0], status);
      group_store_state(&ctx->state[0], PTO_RUNTIME_GROUP_STATE_ABORT);
      *ctx->error = 1;
      group_abort();
    }
  }

  // A missing callback is a group error, not a reason for one PE to leave the
  // rendezvous.  Every participating PE must execute both static SYNC.CORE
  // sites or the remaining PEs can deadlock indefinitely.
  group_store_state(&ctx->state[pe_id], PTO_RUNTIME_GROUP_STATE_RUNNING);
  group_sync_core();

  const int status = __linx_group_pe_main
                         ? __linx_group_pe_main(pe_id, ctx)
                         : -22; /* -EINVAL, without a libc dependency */
  group_store_result(&ctx->result[pe_id], status);
  group_store_state(&ctx->state[pe_id],
                    status ? PTO_RUNTIME_GROUP_STATE_ABORT
                           : PTO_RUNTIME_GROUP_STATE_DONE);
  group_sync_core();

  if (pe_id == 0) {
    int group_status = 0;
    for (unsigned i = 0; i != PTO_RUNTIME_GROUP_PARTICIPANTS; ++i) {
      if (group_load_state(&ctx->state[i]) != PTO_RUNTIME_GROUP_STATE_DONE &&
          !group_status)
        {
          const int pe_status = group_load_result(&ctx->result[i]);
          group_status = pe_status ? pe_status : -1;
        }
    }
    *ctx->error = group_status != 0;
    __asm__ volatile("" ::: "memory");
    if (__pto_runtime_group_complete)
      __pto_runtime_group_complete(ctx, group_status);
    group_abort();
  }
  group_park();
}

__attribute__((noreturn, used, visibility("default"))) void
__linx_group_worker_start(void) {
  const struct pto_runtime_group_context *ctx =
      __pto_runtime_group_context_get ? __pto_runtime_group_context_get() : 0;
  if (!ctx ||
      ctx->magic != PTO_RUNTIME_GROUP_CONTEXT_MAGIC ||
      ctx->version != PTO_RUNTIME_GROUP_CONTEXT_VERSION ||
      ctx->participant_count != PTO_RUNTIME_GROUP_PARTICIPANTS || !ctx->state ||
      !ctx->result ||
      !ctx->error)
    group_abort();
  __linx_group_entry(ctx);
  group_abort();
}
