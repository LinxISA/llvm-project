// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS --match-full-lines

// Issue #72: the disassembler contract is deterministic and uses canonical
// operands, while preserving the complete B.IOR/B.IOS/B.IOT/B.DIM schemas.

// CHECK: {{.*}}B.IOR{{.*}}[a0, a1, a2], ->a3{{.*}}
B.IOR [a0, a1, a2], ->a3

// CHECK: {{.*}}B.IOR{{.*}}[a0, a1], []{{.*}}
B.IOR [a0, a1], []

// CHECK: {{.*}}B.IOR{{.*}}[], ->a0{{.*}}
B.IOR [], ->a0

// CHECK: {{.*}}B.DIM{{.*}}a0, 12,{{.*}}->lb0{{.*}}
B.DIM a0, 12, ->lb0
// CHECK: {{.*}}B.DIM{{.*}}a1, 13,{{.*}}->lb1{{.*}}
B.DIM a1, 13, ->lb1
// CHECK: {{.*}}B.DIM{{.*}}a2, 14,{{.*}}->lb2{{.*}}
B.DIM a2, 14, ->lb2

// CHECK: {{.*}}B.IOS{{.*}}S0, mask=1111{{.*}}
B.IOS S0, mask=1111
// CHECK: {{.*}}B.IOS{{.*}}mask=1100, ->S63<256KB>{{.*}}
B.IOS mask=1100, ->S63<256KB>

// CHECK: {{.*}}B.IOT{{.*}}mask=0000, last,{{.*}}->t<128B>{{.*}}
B.IOT mask=0000, last, ->t<128B>
// CHECK: {{.*}}B.IOT{{.*}}t#1, mask=1111, last{{.*}}
B.IOT t#1, mask=1111, last
// CHECK: {{.*}}B.IOT{{.*}}t#1, u#2, mask=1111, last{{.*}}
B.IOT t#1, u#2, mask=1111, last

// DIS: {{^[[:space:]]*0: 20310293[[:space:]]+B\.IOR[[:space:]]+\[a0, a1, a2\], ->a3$}}
// DIS: {{^[[:space:]]*4: 00310013[[:space:]]+B\.IOR[[:space:]]+\[a0, a1\], \[\]$}}
// DIS: {{^[[:space:]]*8: 00000113[[:space:]]+B\.IOR[[:space:]]+\[\], ->a0$}}
// DIS: {{^[[:space:]]*c: 00c10043[[:space:]]+B\.DIM[[:space:]]+a0, 12,[[:space:]]+->lb0$}}
// DIS: {{^[[:space:]]*10: 00d19043[[:space:]]+B\.DIM[[:space:]]+a1, 13,[[:space:]]+->lb1$}}
// DIS: {{^[[:space:]]*14: 00e22043[[:space:]]+B\.DIM[[:space:]]+a2, 14,[[:space:]]+->lb2$}}
// DIS: {{^[[:space:]]*18: 00001e13[[:space:]]+B\.IOS[[:space:]]+S0, mask=1111$}}
// DIS: {{^[[:space:]]*1c: 03f61a13[[:space:]]+B\.IOS[[:space:]]+mask=1100, ->S63<256KB>$}}
// DIS: {{^[[:space:]]*20: 0008e013[[:space:]]+B\.IOT[[:space:]]+mask=0000, last,[[:space:]]+->t<128B>$}}
// DIS: {{^[[:space:]]*24: 00085e13[[:space:]]+B\.IOT[[:space:]]+t#1, mask=1111, last$}}
// DIS: {{^[[:space:]]*28: 44084e13[[:space:]]+B\.IOT[[:space:]]+t#1, u#2, mask=1111, last$}}

// A single object also exercises the raw-byte column with 16-, 32-, 48-, and
// 64-bit instructions. The B.IOS source and destination forms are included
// with the boundary masks and destination sizes required by Issue #72.
C.B.DIMI 8, ->lb0
B.IOS S0, mask=0000
B.IOS S63, mask=0001
B.IOS mask=0000, ->S0<128B>
B.IOS mask=0001, ->S63<8KB>
C.B.DIMI 8, ->lb1
hl.sb.pr a0, [a1, t#3.uw], ->u
v.lwi.local [to3, lc0<<2, 1024], ->vt.w

// DIS: {{^[[:space:]]*2c: 023c[[:space:]]+C\.B\.DIMI[[:space:]]+8,[[:space:]]+->lb0$}}
// DIS: {{^[[:space:]]*2e: 00001013[[:space:]]+B\.IOS[[:space:]]+S0, mask=0000$}}
// DIS: {{^[[:space:]]*32: 03f01813[[:space:]]+B\.IOS[[:space:]]+S63, mask=0001$}}
// DIS: {{^[[:space:]]*36: 00009013[[:space:]]+B\.IOS[[:space:]]+mask=0000, ->S0<128B>$}}
// DIS: {{^[[:space:]]*3a: 03f39813[[:space:]]+B\.IOS[[:space:]]+mask=0001, ->S63<8KB>$}}
// DIS: {{^[[:space:]]*3e: 423c[[:space:]]+C\.B\.DIMI[[:space:]]+8,[[:space:]]+->lb1$}}
// DIS: {{^[[:space:]]*40: f02e 8049 13a1[[:space:]]+hl\.sb\.pr[[:space:]]+a0, \[a1, t#3\.uw\],[[:space:]]+->u$}}
// DIS: {{^[[:space:]]*46: 0001527f 100da019[[:space:]]+v\.lwi\.local[[:space:]]+\[to3, lc0<<2, 1024\],[[:space:]]+->vt\.w$}}
