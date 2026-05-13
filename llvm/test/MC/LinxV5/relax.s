// RUN: clang %s --target=linx64v5 -O2 -c -o %s.o
// RUN: ld.lld  %s.o -o %s.bin
// RUN: llvm-objdump -d %s.bin | FileCheck %s --dump-input always -vv
// RUN: rm %s.o %s.bin
// CHECK: C.BSTART.STD    DIRECT, 0x11132 <test>
// CHECK: c.setret        0x1112a, ->ra <.Ltmp0>
main:
        C.BSTART.STD
        FENTRY  [ra ~ ra], sp!, 16
        L.BSTART.STD    CALL, test
        setret  .Ltmp0, ->ra
.Ltmp0:
        C.BSTART.STD
        FRET.STK        [ra ~ ra], sp!, 16
        C.BSTOP

test:
        nop