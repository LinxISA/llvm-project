//==- LinxV5SchedSIMT.td - LinxV5 SIMT Scheduling Definitions --*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===-----------------------------------------------------------------------===//

// Configuration for single PE.
def SIMTSchedModel : SchedMachineModel {
  let MicroOpBufferSize = 128;
  let IssueWidth = 4;
  let LoadLatency = 3;
  let CompleteModel = false;
}

// The pipelines of LinxV5 **SIMT** microarchitecture.
// Pipe A can handle integer alu, integer multiply and divide, and floating point computation.
// Pipe B can handle control flow.
// Pipe C can handle memory.
let SchedModel = SIMTSchedModel in {
// BufferSize is the Size of OoO Unit IssueQ.
let BufferSize = 24 in {
def SIMTPipeA       : ProcResource<4>;
def SIMTPipeB       : ProcResource<4>;
def SIMTPipeC       : ProcResource<4>;
}

//===----------------------------- Write Result -----------------------------===//

def : WriteRes<WriteDafultA, [SIMTPipeA]> {
  let Latency = 2;
}

def : WriteRes<WriteDafultB, [SIMTPipeB]> {
  let Latency = 2;
}

def : WriteRes<WriteLD, [SIMTPipeC]> {
  let Latency = 6;
}

def : WriteRes<WriteST, [SIMTPipeC]> {
  let Latency = 2;
}

//===-------------------------- Bypass and advance --------------------------===//
def : ReadAdvance<ReadDafult, 0>;

}
