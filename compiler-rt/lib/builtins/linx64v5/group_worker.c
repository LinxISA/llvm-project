// Secondary-PE entry carrier for the hosted group ABI.  This file intentionally
// has no libc dependency and never enters musl CRT.  gfrun supplies the PC,
// stack and PE id; the carrier calls main and parks when it returns.  The
// image's main owns any shared-memory completion protocol.

#if !defined(__linx)
#error "the Linx group worker runtime is target-only"
#endif

// The hosted image's ordinary main is the worker body as well.  It must not
// use process-global libc/TLS facilities when entered on PE1..PE3.
extern int main();

static __attribute__((noreturn)) void worker_park(void) {
  for (;;) {
    __asm__ volatile("" ::: "memory");
  }
}

__attribute__((noreturn, used, visibility("default"))) void
__linx_group_worker_start(void) {
  (void)main();
  worker_park();
}
