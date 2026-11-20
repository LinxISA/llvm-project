#ifndef LINX_PTO_RUNTIME_H
#define LINX_PTO_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

/* Platform ABI, not an ISA launch instruction. Keep layout in sync with musl,
 * Linux and TileOP. The launcher publishes this immutable descriptor before
 * entering the same PC on PE0..3, with separate valid stacks and TLS. */
#ifndef PTO_RUNTIME_INTRA
#if defined(__linx) && defined(__clang__)
#define PTO_RUNTIME_INTRA __attribute__((linx_scalar_shareability("intra")))
#else
#define PTO_RUNTIME_INTRA
#endif
#endif

#define PTO_RUNTIME_GROUP_CONTEXT_MAGIC 0x50544f47u
#define PTO_RUNTIME_GROUP_CONTEXT_VERSION 1u
#define PTO_RUNTIME_GROUP_PARTICIPANTS 4u
#define PTO_RUNTIME_GROUP_STATE_READY 0u
#define PTO_RUNTIME_GROUP_STATE_RUNNING 1u
#define PTO_RUNTIME_GROUP_STATE_DONE 2u
#define PTO_RUNTIME_GROUP_STATE_ABORT 3u

struct pto_runtime_group_context {
  uint32_t magic;
  uint16_t version;
  uint16_t participant_count;
  volatile uint32_t *state;
  volatile int32_t *result;
  volatile uint32_t *error;
};

#if defined(__cplusplus)
#define PTO_RUNTIME_ASSERT static_assert
#else
#define PTO_RUNTIME_ASSERT _Static_assert
#endif
PTO_RUNTIME_ASSERT(sizeof(struct pto_runtime_group_context) == 32,
                   "PTO context ABI drift");
PTO_RUNTIME_ASSERT(offsetof(struct pto_runtime_group_context, state) == 8,
                   "PTO state pointer ABI drift");
PTO_RUNTIME_ASSERT(offsetof(struct pto_runtime_group_context, result) == 16,
                   "PTO result pointer ABI drift");
PTO_RUNTIME_ASSERT(offsetof(struct pto_runtime_group_context, error) == 24,
                   "PTO error pointer ABI drift");
#undef PTO_RUNTIME_ASSERT
#ifdef __cplusplus
extern "C" {
#endif

PTO_RUNTIME_INTRA const struct pto_runtime_group_context *
__pto_runtime_group_context_get(void);
PTO_RUNTIME_INTRA unsigned __pto_runtime_pe_context_count(void);
PTO_RUNTIME_INTRA void __linx_group_entry(
    const struct pto_runtime_group_context *) __attribute__((noreturn));
PTO_RUNTIME_INTRA void __linx_group_worker_start(void) __attribute__((noreturn));
PTO_RUNTIME_INTRA int __linx_group_pe_main(
    unsigned, const struct pto_runtime_group_context *);
PTO_RUNTIME_INTRA void __pto_runtime_group_complete(
    const struct pto_runtime_group_context *, int) __attribute__((noreturn));

/* Optional PE0-only hooks. Return zero on success; failure aborts before the
 * generation-zero rendezvous. Neither hook may execute a four-PE collective.
 * Process initialization runs once per loaded image; invocation preparation
 * runs on every launch, including after an aborted invocation. A failed
 * process initializer poisons the image until the process is reloaded.
 * Constructors/libc initialization belong to the platform CRT before entry;
 * freestanding adapters may perform them in the process hook instead. */
PTO_RUNTIME_INTRA int __pto_runtime_process_initialize(void);
PTO_RUNTIME_INTRA int __pto_runtime_invocation_initialize(
    const struct pto_runtime_group_context *);

#ifdef __cplusplus
}
#endif
#endif
