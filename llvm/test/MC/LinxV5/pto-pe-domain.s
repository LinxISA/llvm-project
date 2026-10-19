# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64v5 -filetype=asm %t/good.s | \
# RUN:   FileCheck %s --check-prefix=ASM
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/good.s -o %t/good.o
# RUN: llvm-objdump -d --disassembler-options=no-tile-macros %t/good.o | \
# RUN:   FileCheck %s --check-prefix=DIS
# RUN: not --crash llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-subset.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=SUBSET
# RUN: not --crash llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-shared.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=SHARED
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-raw.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=RAW
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-domain.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=DOMAIN
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-raw-alias.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=RAW
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-raw-inst.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=RAW
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-raw-incbin.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=RAW
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-raw-fill.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=RAW
# RUN: not llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-icall.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ICALL
# RUN: not --crash llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-gmov.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=GMOV
# RUN: not --crash llvm-mc -triple=linx64v5 -filetype=obj \
# RUN:   %t/bad-timg2col.s -o /dev/null 2>&1 | FileCheck %s --check-prefix=TIMG2COL

# ASM: .pto_pe_domain 8
# ASM: B.IOT{{.*}}mask=1111, last, ->t<128B>
# ASM: B.IOT{{.*}}mask=1000, last, ->t<128B>
# ASM: .pto_pe_domain 15
# ASM: B.IOT{{.*}}mask=1111, last, ->t<128B>

# DIS: B.IOT{{.*}}mask=1000, last, ->t<128B>
# DIS-NEXT: B.IOT{{.*}}mask=1000, last, ->t<128B>
# DIS-NEXT: B.IOT{{.*}}mask=1111, last, ->t<128B>
# DIS: BSTART.VPAR
# DIS: B.IOT{{.*}}mask=1000, last
# DIS: L.BSTART.STD DIRECT{{.*}}<.text+0x18>

# SUBSET: explicit PE mask is not a subset of the function Tile PE domain
# SHARED: nondefault Tile PE domains do not support Shared, GMOV, or cooperative PE binders
# RAW: raw data directives are forbidden in a nondefault Tile PE domain
# DOMAIN: Tile PE domain must be one of 1,2,4,8,12,14,15
# ICALL: inline assembly calls are forbidden in a nondefault Tile PE domain
# GMOV: GMOV is unsupported in a nondefault Tile PE domain
# TIMG2COL: TIMG2COL is unsupported in a nondefault Tile PE domain

#--- good.s
.pto_pe_domain 8
B.IOT mask=1111, last, ->t<128B>
B.IOT mask=1000, last, ->t<128B>
.pto_pe_domain 15
B.IOT mask=1111, last, ->t<128B>
.pto_pe_domain 8
VPAR target, <M: 1, N: 1, K: 1, MR> T#1, T#2, ->T<128B>
BSTART CALL, target
.pto_pe_domain 15
target:

#--- bad-subset.s
.pto_pe_domain 8
B.IOT mask=1100, last, ->t<128B>

#--- bad-shared.s
.pto_pe_domain 8
B.IOS mask=1111, ->S0<128B>

#--- bad-raw.s
.pto_pe_domain 8
.word 0

#--- bad-domain.s
.pto_pe_domain 3
B.IOT mask=1111, last, ->t<128B>

#--- bad-raw-alias.s
.pto_pe_domain 8
.4byte 0

#--- bad-raw-inst.s
.pto_pe_domain 8
.inst 0

#--- bad-raw-incbin.s
.pto_pe_domain 8
.incbin "/dev/null"

#--- bad-raw-fill.s
.pto_pe_domain 8
.fill 1, 4, 0

#--- bad-icall.s
.pto_pe_domain 8
BSTART ICALL

#--- bad-gmov.s
.pto_pe_domain 8
BSTART.GMOV U32

#--- bad-timg2col.s
.pto_pe_domain 8
BSTART.TLSU 28, U32
