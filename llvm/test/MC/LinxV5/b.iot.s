// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --dump-input always -vv

// CHECK: B.IOT [], last ->t<512B>
B.IOT [], last ->t<512B>

// CHECK: B.IOT [] ->t<512B>
B.IOT [] ->t<512B>

// CHECK: B.IOT [t#1]
B.IOT [t#1]

// CHECK: B.IOT [u#1], last
B.IOT [u#1], last

// CHECK: B.IOT [m#2], last ->u<32KB>
B.IOT [m#2], last ->u<32KB>

// CHECK: B.IOT [n#4] ->n<32KB>
B.IOT [n#4] ->n<32KB>

// CHECK: B.IOT [t#1, u#1]
B.IOT [t#1, u#1]

// CHECK: B.IOT [u#1, u#2], last
B.IOT [u#1, u#2], last

// CHECK: B.IOT [m#2, u#7], last ->u<32KB>
B.IOT [m#2, u#7], last ->u<32KB>

// CHECK: B.IOT [n#4, n#1] ->n<32KB>
B.IOT [n#4, n#1] ->n<32KB>
