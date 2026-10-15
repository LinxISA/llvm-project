// Minimal secondary-PE entry carrier for launchers that provide a worker PC.
// This file intentionally has no libc dependency and never enters musl CRT.

#if !defined(__linx)
#error "the Linx group worker runtime is target-only"
#endif

extern int __linx_group_worker_main(unsigned pe_id) __attribute__((weak));
extern void __linx_group_worker_complete(int status)
    __attribute__((weak, noreturn));

__attribute__((noreturn, used, visibility("default"))) void
__linx_group_worker_start(void) {
  const unsigned pe_id = __builtin_linx_get_thread_idx();
  int status = -1;

  if (__linx_group_worker_main)
    status = __linx_group_worker_main(pe_id);

  // The platform owns guest-result -> host End mapping.  Until that ABI is
  // standardised, require an explicit hook and fail closed if it is absent.
  if (__linx_group_worker_complete)
    __linx_group_worker_complete(status);

  __builtin_trap();
  for (;;)
    ;
}
