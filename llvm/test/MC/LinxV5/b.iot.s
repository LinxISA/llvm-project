// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --dump-input always -vv

// CHECK: B.IOT mask=1111, last, ->t<512B>
B.IOT mask=1111, last, ->t<512B>

// CHECK: B.IOT mask=1111, ->t<512B>
B.IOT mask=1111, ->t<512B>

// CHECK: B.IOT t#1, mask=1111
B.IOT t#1, mask=1111

// CHECK: B.IOT u#1, mask=1111, last
B.IOT u#1, mask=1111, last

// CHECK: B.IOT m#2, mask=1111, last, ->u<32KB>
B.IOT m#2, mask=1111, last, ->u<32KB>

// CHECK: B.IOT n#4, mask=1111, ->n<32KB>
B.IOT n#4, mask=1111, ->n<32KB>

// CHECK: B.IOT t#1, u#1, mask=1111
B.IOT t#1, u#1, mask=1111

// CHECK: B.IOT u#1, u#2, mask=1111, last
B.IOT u#1, u#2, mask=1111, last

// CHECK: B.IOT m#2, u#7, mask=1111, last, ->u<32KB>
B.IOT m#2, u#7, mask=1111, last, ->u<32KB>

// CHECK: B.IOT n#4, n#1, mask=1111, ->n<32KB>
B.IOT n#4, n#1, mask=1111, ->n<32KB>
