// Common-entry carrier for the hosted group ABI. This file intentionally has
// no libc dependency and never enters musl CRT. The launcher supplies one PC,
// one PE-local stack/TLS context per PE, and a shared group descriptor.

#if !defined(__linx)
#error "the Linx group worker runtime is target-only"
#endif

struct pto_runtime_group_context {
  unsigned magic;
  unsigned short version;
  unsigned short participant_count;
  // Launcher-owned four-element arrays indexed by architectural PE id.
  volatile unsigned *state;
  volatile int *result;
  volatile unsigned *error;
};

enum {
  PTO_RUNTIME_GROUP_CONTEXT_MAGIC = 0x50544f47u,
  PTO_RUNTIME_GROUP_CONTEXT_VERSION = 1u,
  PTO_RUNTIME_GROUP_STATE_READY = 0u,
  PTO_RUNTIME_GROUP_STATE_RUNNING = 1u,
  PTO_RUNTIME_GROUP_STATE_DONE = 2u,
  PTO_RUNTIME_GROUP_STATE_ABORT = 3u,
};

extern const struct pto_runtime_group_context *
__pto_runtime_group_context_get(void) __attribute__((weak));
extern int __linx_group_pe_main(
    unsigned, const struct pto_runtime_group_context *) __attribute__((weak));
extern void __pto_runtime_group_complete(
    const struct pto_runtime_group_context *, int)
    __attribute__((weak, noreturn));

static __attribute__((noreturn)) void group_abort(void) {
  __builtin_trap();
  for (;;) __asm__ volatile("" ::: "memory");
}

static __attribute__((noreturn)) void group_park(void) {
  for (;;) __asm__ volatile("" ::: "memory");
}

static inline void group_sync_core(void) {
  // Keep this runtime buildable with the bootstrap assembler, which predates
  // the mnemonic. PTO ISA encodes SYNC.CORE as the fixed L32 word 0x0330a02b.
  __asm__ volatile(".word 0x0330a02b" ::: "memory");
}

__attribute__((noreturn, used, visibility("default"))) void
__linx_group_entry(const struct pto_runtime_group_context *ctx) {
  const unsigned pe_id = __builtin_linx_get_thread_idx();
  if (!ctx || !__linx_group_pe_main || pe_id >= 4)
    group_abort();

  ctx->state[pe_id] = PTO_RUNTIME_GROUP_STATE_RUNNING;
  group_sync_core();

  const int status = __linx_group_pe_main(pe_id, ctx);
  ctx->result[pe_id] = status;
  ctx->state[pe_id] = status ? PTO_RUNTIME_GROUP_STATE_ABORT
                             : PTO_RUNTIME_GROUP_STATE_DONE;
  group_sync_core();

  if (pe_id == 0) {
    int group_status = 0;
    for (unsigned i = 0; i != 4; ++i) {
      if (ctx->state[i] != PTO_RUNTIME_GROUP_STATE_DONE && !group_status)
        group_status = ctx->result[i] ? ctx->result[i] : -1;
    }
    *ctx->error = group_status != 0;
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
      ctx->participant_count != 4 || !ctx->state || !ctx->result ||
      !ctx->error)
    group_abort();
  __linx_group_entry(ctx);
  group_abort();
}
