//==- LinxV5Schedule.td - LinxV5 Scheduling Definitions -------*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

/// Define scheduler resources associated with def operands.
def WriteDafultA    : SchedWrite; // Pipe A: integer alu, integer multiply and divide, and floating point computation.
def WriteDafultB    : SchedWrite; // Pipe B: control flow, such as jump, branch
def WriteLD         : SchedWrite;
def WriteST         : SchedWrite;

/// Define scheduler resources associated with use operands.
def ReadDafult      : SchedRead; // treat all op without read advance

def DefaultOoOModel : SchedMachineModel {
  let MicroOpBufferSize = 128;  // Out-Of-Order, ROB size is 128.
  let IssueWidth = 16;          // 16 micro-op is dispatched per cycle.
  let LoadLatency = 1;          // For SchedDAG Height calculate.
  let CompleteModel = false;    // Only define Model, not sched Op or Inst.
}