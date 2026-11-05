# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=DIS
# Owning ASL DecodeScalarBinaryRightModifier: sw=00, uw=01, neg/not=10,
# unchanged=11. CAS-loop arithmetic must preserve all 64 operand bits.
sub a0, a1, ->a2
# ENC: sub{{.*}}encoding: [0x05,0x12,0x31,0x06]
# DIS: sub a0, a1, ->a2
sub a0, a1.sw, ->a2
# ENC: sub{{.*}}encoding: [0x05,0x12,0x31,0x00]
# DIS: sub a0, a1.sw, ->a2
sub a0, a1.uw, ->a2
# ENC: sub{{.*}}encoding: [0x05,0x12,0x31,0x02]
# DIS: sub a0, a1.uw, ->a2
sub a0, a1.neg, ->a2
# ENC: sub{{.*}}encoding: [0x05,0x12,0x31,0x04]
# DIS: sub a0, a1.neg, ->a2
add a0, a1, ->a2
# ENC: add{{.*}}encoding: [0x05,0x02,0x31,0x06]
# DIS: add a0, a1, ->a2
and a0, a1, ->a2
# ENC: and{{.*}}encoding: [0x05,0x22,0x31,0x06]
# DIS: and a0, a1, ->a2
xor a0, a1.not, ->a2
# ENC: xor{{.*}}encoding: [0x05,0x42,0x31,0x04]
# DIS: xor a0, a1.not, ->a2

# Register shifts have no binary modifier: all bits [31:25] stay zero.
sll a0, a1, ->a2
# ENC: sll{{.*}}encoding: [0x05,0x72,0x31,0x00]
# DIS: sll a0, a1, ->a2
srl a0, a1, ->a2
# ENC: srl{{.*}}encoding: [0x05,0x52,0x31,0x00]
# DIS: srl a0, a1, ->a2
sra a0, a1, ->a2
# ENC: sra{{.*}}encoding: [0x05,0x62,0x31,0x00]
# DIS: sra a0, a1, ->a2
sllw a0, a1, ->a2
# ENC: sllw{{.*}}encoding: [0x25,0x72,0x31,0x00]
# DIS: sllw a0, a1, ->a2
