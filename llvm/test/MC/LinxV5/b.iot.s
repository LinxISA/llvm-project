// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
// RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC

// Source lifetime is independent from the B.IOT `last` sequence flag: a bare
// source is the last-use consumer for the participating PEs and `.reuse`
// retains the source vtag value. Every pre-change word stays retain/retain.

// DIS: B.IOT mask=1111, last, ->t<512B>
B.IOT mask=1111, last, ->t<512B>

// DIS: B.IOT mask=1111, ->t<512B>
B.IOT mask=1111, ->t<512B>

// One-source Func=101: bit 26 one is last-use, zero keeps the source.
// DIS: B.IOT t#1, mask=1111
B.IOT t#1, mask=1111

// DIS: B.IOT t#1.reuse, mask=1111
B.IOT t#1.reuse, mask=1111

// DIS: B.IOT u#1, mask=1111, last
B.IOT u#1, mask=1111, last

// DIS: B.IOT u#1.reuse, mask=1111, last
B.IOT u#1.reuse, mask=1111, last

// DIS: B.IOT m#2, mask=1111, last, ->u<32KB>
B.IOT m#2, mask=1111, last, ->u<32KB>

// DIS: B.IOT n#4, mask=1111, ->n<32KB>
B.IOT n#4, mask=1111, ->n<32KB>

// Two-source Func 010/011/111/100 carry the four lifetime combinations.
// DIS: B.IOT t#1, u#1, mask=1111
B.IOT t#1, u#1, mask=1111

// DIS: B.IOT t#1.reuse, u#1, mask=1111
B.IOT t#1.reuse, u#1, mask=1111

// DIS: B.IOT t#1, u#1.reuse, mask=1111
B.IOT t#1, u#1.reuse, mask=1111

// The historical two-source word is retain/retain and prints explicitly.
// DIS: B.IOT t#1.reuse, u#1.reuse, mask=1111
B.IOT t#1.reuse, u#1.reuse, mask=1111

// DIS: B.IOT u#1, u#2, mask=1111, last
B.IOT u#1, u#2, mask=1111, last

// DIS: B.IOT m#2, u#7, mask=1111, last, ->u<32KB>
B.IOT m#2, u#7, mask=1111, last, ->u<32KB>

// DIS: B.IOT n#4, n#1, mask=1111, ->n<32KB>
B.IOT n#4, n#1, mask=1111, ->n<32KB>
