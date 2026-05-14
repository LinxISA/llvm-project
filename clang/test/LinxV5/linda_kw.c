// RUN: %clang %s --target=linx64v5 -S -emit-llvm -O2 -o - | FileCheck %s --check-prefixes=DefaultNotTLS

// DefaultNotTLS: @g = external dso_local local_unnamed_addr global i32, align 8
// DefaultTLS: @g = external thread_local(localexec) local_unnamed_addr global i32, align 8

// DefaultNotTLS: @foo2.a = internal unnamed_addr global i32 0, align 8
// DefaultTLS: @foo2.a = internal thread_local(localexec) unnamed_addr global i32 0, align 8

// DefaultNotTLS: @a = dso_local local_unnamed_addr global i32 0, align 8
// DefaultTLS: @a = dso_local thread_local(localexec) local_unnamed_addr global i32 0, align 8

// DefaultNotTLS: @b = dso_local local_unnamed_addr global i32 0, section ".tmp", align 8
// DefaultTLS: @b = dso_local thread_local(localexec) local_unnamed_addr global i32 0, section ".tmp", align 8

// DefaultNotTLS: @c = dso_local thread_local(localexec) local_unnamed_addr global i32 0, align 8
// DefaultTLS: @c = dso_local thread_local(localexec) local_unnamed_addr global i32 0, align 8

// DefaultNotTLS: @d = dso_local thread_local(localexec) local_unnamed_addr global i32 0, section ".tmp", align 8
// DefaultTLS: @d = dso_local thread_local(localexec) local_unnamed_addr global i32 0, section ".tmp", align 8

// DefaultNotTLS: @h = dso_local local_unnamed_addr global i32 0, align 8
// DefaultTLS: @h = dso_local local_unnamed_addr global i32 0, align 8

// DefaultNotTLS: @j = dso_local local_unnamed_addr global i32 0, section ".tmp_s", align 8
// DefaultTLS: @j = dso_local local_unnamed_addr global i32 0, section ".tmp_s", align 8



int a;                                                   // tls
int __attribute__((section(".tmp"))) b;                  // tls
__linda_thread int c;                                    // tls
int __linda_thread __attribute__((section(".tmp"))) d;   // tls
static int e;                                            // tls
extern int g;                                            // tls
__linda_shared int h;                                    // shared
__linda_shared static int i;                             // shared
__linda_shared int __attribute__((section(".tmp_s"))) j; // shared
__linda_shared extern int k;                             // shared

extern int bar(int);

int foo1() { return g + 1; }
int foo2() {
  static int a = 0;
  a = bar(a);
  return a;
}
