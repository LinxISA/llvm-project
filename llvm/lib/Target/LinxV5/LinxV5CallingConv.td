//===-- LinxV5CallingConv.td - Calling Conventions LinxV5 ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the LinxV5 architecture.
//
//===----------------------------------------------------------------------===//

// use this to generate CSR_ILP32_LP64_RegMask
def CSR_ILP32_LP64
    : CalleeSavedRegs<(add (sequence "R%u", 10, 19))>;

// Needed for implementation of LinxV5RegisterInfo::getNoPreservedMask()
def CSR_NoRegs : CalleeSavedRegs<(add)>;

// Interrupt handler needs to save/restore all registers that are used,
// both Caller and Callee saved registers.
def CSR_Interrupt : CalleeSavedRegs<(sequence "R%u", 1, 23)>;
