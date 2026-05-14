//===--- LinxV5RegisterInfoSIMT.td - LinxV5 SIMT Register defs ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------------===//

class V5SIMTReg<bits<2> extenc, bits<5> enc, string n, list<string> altNames = []>
                : Register<n, altNames> {
  let Namespace = "LinxV5";
  let HWEncoding{6-5} = extenc;
  let HWEncoding{4-0} = enc;
  let AltNames = altNames;
}

let RegAltNameIndices = [ABIRegAltName] in {
  // Dst
  def SIMT_VT   :  V5SIMTReg<0,  0, "vt",  ["vt"]>, DwarfRegNum<[100]>;
  def SIMT_VU   :  V5SIMTReg<0,  1, "vu",  ["vu"]>, DwarfRegNum<[100]>;
  def SIMT_VM   :  V5SIMTReg<0,  2, "vm",  ["vm"]>, DwarfRegNum<[100]>;
  def SIMT_VN   :  V5SIMTReg<0,  3, "vn",  ["vn"]>, DwarfRegNum<[100]>;

  def SIMT_RO0   :  V5SIMTReg<1,  0, "ro0",  ["ro0"]>, DwarfRegNum<[100]>;
  def SIMT_RO1   :  V5SIMTReg<1,  1, "ro1",  ["ro1"]>, DwarfRegNum<[100]>;
  def SIMT_RO2   :  V5SIMTReg<1,  2, "ro2",  ["ro2"]>, DwarfRegNum<[100]>;
  def SIMT_RO3   :  V5SIMTReg<1,  3, "ro3",  ["ro3"]>, DwarfRegNum<[100]>;

  def SIMT_U   :  V5SIMTReg<1, 30, "u",  ["u"]>, DwarfRegNum<[100]>;
  def SIMT_T   :  V5SIMTReg<1, 31, "t",  ["t"]>, DwarfRegNum<[100]>;

  // Src
  def SIMT_OSVT1  :  V5SIMTReg<0,  0, "vt#1",  ["vt#1"]>, DwarfRegNum<[100]>;
  def SIMT_OSVT2  :  V5SIMTReg<0,  1, "vt#2",  ["vt#2"]>, DwarfRegNum<[100]>;
  def SIMT_OSVT3  :  V5SIMTReg<0,  2, "vt#3",  ["vt#3"]>, DwarfRegNum<[100]>;
  def SIMT_OSVT4  :  V5SIMTReg<0,  3, "vt#4",  ["vt#4"]>, DwarfRegNum<[100]>;

  def SIMT_OSVU1  :  V5SIMTReg<0,  8, "vu#1",  ["vu#1"]>, DwarfRegNum<[100]>;
  def SIMT_OSVU2  :  V5SIMTReg<0,  9, "vu#2",  ["vu#2"]>, DwarfRegNum<[100]>;
  def SIMT_OSVU3  :  V5SIMTReg<0, 10, "vu#3",  ["vu#3"]>, DwarfRegNum<[100]>;
  def SIMT_OSVU4  :  V5SIMTReg<0, 11, "vu#4",  ["vu#4"]>, DwarfRegNum<[100]>;

  def SIMT_OSVM1  :  V5SIMTReg<0, 16, "vm#1",  ["vm#1"]>, DwarfRegNum<[100]>;
  def SIMT_OSVM2  :  V5SIMTReg<0, 17, "vm#2",  ["vm#2"]>, DwarfRegNum<[100]>;
  def SIMT_OSVM3  :  V5SIMTReg<0, 18, "vm#3",  ["vm#3"]>, DwarfRegNum<[100]>;
  def SIMT_OSVM4  :  V5SIMTReg<0, 19, "vm#4",  ["vm#4"]>, DwarfRegNum<[100]>;

  def SIMT_OSVN1  :  V5SIMTReg<0, 24, "vn#1",  ["vn#1"]>, DwarfRegNum<[100]>;
  def SIMT_OSVN2  :  V5SIMTReg<0, 25, "vn#2",  ["vn#2"]>, DwarfRegNum<[100]>;
  def SIMT_OSVN3  :  V5SIMTReg<0, 26, "vn#3",  ["vn#3"]>, DwarfRegNum<[100]>;
  def SIMT_OSVN4  :  V5SIMTReg<0, 27, "vn#4",  ["vn#4"]>, DwarfRegNum<[100]>;

  def SIMT_OSVT1_RU  :  V5SIMTReg<3,  0, "vt#1.reuse",  ["vt#1.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVT2_RU  :  V5SIMTReg<3,  1, "vt#2.reuse",  ["vt#2.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVT3_RU  :  V5SIMTReg<3,  2, "vt#3.reuse",  ["vt#3.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVT4_RU  :  V5SIMTReg<3,  3, "vt#4.reuse",  ["vt#4.reuse"]>, DwarfRegNum<[100]>;

  def SIMT_OSVU1_RU  :  V5SIMTReg<3,  8, "vu#1.reuse",  ["vu#1.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVU2_RU  :  V5SIMTReg<3,  9, "vu#2.reuse",  ["vu#2.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVU3_RU  :  V5SIMTReg<3, 10, "vu#3.reuse",  ["vu#3.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVU4_RU  :  V5SIMTReg<3, 11, "vu#4.reuse",  ["vu#4.reuse"]>, DwarfRegNum<[100]>;

  def SIMT_OSVM1_RU  :  V5SIMTReg<3, 16, "vm#1.reuse",  ["vm#1.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVM2_RU  :  V5SIMTReg<3, 17, "vm#2.reuse",  ["vm#2.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVM3_RU  :  V5SIMTReg<3, 18, "vm#3.reuse",  ["vm#3.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVM4_RU  :  V5SIMTReg<3, 19, "vm#4.reuse",  ["vm#4.reuse"]>, DwarfRegNum<[100]>;

  def SIMT_OSVN1_RU  :  V5SIMTReg<3, 24, "vn#1.reuse",  ["vn#1.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVN2_RU  :  V5SIMTReg<3, 25, "vn#2.reuse",  ["vn#2.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVN3_RU  :  V5SIMTReg<3, 26, "vn#3.reuse",  ["vn#3.reuse"]>, DwarfRegNum<[100]>;
  def SIMT_OSVN4_RU  :  V5SIMTReg<3, 27, "vn#4.reuse",  ["vn#4.reuse"]>, DwarfRegNum<[100]>;

  def SIMT_OST1  :  V5SIMTReg<1, 24, "t#1",  ["t#1"]>, DwarfRegNum<[100]>;
  def SIMT_OST2  :  V5SIMTReg<1, 25, "t#2",  ["t#2"]>, DwarfRegNum<[100]>;
  def SIMT_OST3  :  V5SIMTReg<1, 26, "t#3",  ["t#3"]>, DwarfRegNum<[100]>;
  def SIMT_OST4  :  V5SIMTReg<1, 27, "t#4",  ["t#4"]>, DwarfRegNum<[100]>;

  def SIMT_RI0  :  V5SIMTReg<1, 0, "ri0",  ["ri0"]>, DwarfRegNum<[100]>;
  def SIMT_RI1  :  V5SIMTReg<1, 1, "ri1",  ["ri1"]>, DwarfRegNum<[100]>;
  def SIMT_RI2  :  V5SIMTReg<1, 2, "ri2",  ["ri2"]>, DwarfRegNum<[100]>;
  def SIMT_RI3  :  V5SIMTReg<1, 3, "ri3",  ["ri3"]>, DwarfRegNum<[100]>;
  def SIMT_RI4  :  V5SIMTReg<1, 4, "ri4",  ["ri4"]>, DwarfRegNum<[100]>;
  def SIMT_RI5  :  V5SIMTReg<1, 5, "ri5",  ["ri5"]>, DwarfRegNum<[100]>;
  def SIMT_RI6  :  V5SIMTReg<1, 6, "ri6",  ["ri6"]>, DwarfRegNum<[100]>;
  def SIMT_RI7  :  V5SIMTReg<1, 7, "ri7",  ["ri7"]>, DwarfRegNum<[100]>;
  def SIMT_RI8  :  V5SIMTReg<1, 8, "ri8",  ["ri8"]>, DwarfRegNum<[100]>;
  def SIMT_RI9  :  V5SIMTReg<1, 9, "ri9",  ["ri9"]>, DwarfRegNum<[100]>;
  def SIMT_RI10  :  V5SIMTReg<1, 10, "ri10",  ["ri10"]>, DwarfRegNum<[100]>;
  def SIMT_RI11  :  V5SIMTReg<1, 11, "ri11",  ["ri11"]>, DwarfRegNum<[100]>;

  def SIMT_OSU1  :  V5SIMTReg<1, 28, "u#1",  ["u#1"]>, DwarfRegNum<[100]>;
  def SIMT_OSU2  :  V5SIMTReg<1, 29, "u#2",  ["u#2"]>, DwarfRegNum<[100]>;
  def SIMT_OSU3  :  V5SIMTReg<1, 30, "u#3",  ["u#3"]>, DwarfRegNum<[100]>;
  def SIMT_OSU4  :  V5SIMTReg<1, 31, "u#4",  ["u#4"]>, DwarfRegNum<[100]>;

  def SIMT_LC0 :   V5SIMTReg<2,  0, "lc0",   ["lc0"]>, DwarfRegNum<[100]>;
  def SIMT_LB0 :   V5SIMTReg<2,  1, "lb0",   ["lb0"]>, DwarfRegNum<[100]>;

  def SIMT_LC1 :   V5SIMTReg<2,  4, "lc1",   ["lc1"]>, DwarfRegNum<[100]>;
  def SIMT_LB1 :   V5SIMTReg<2,  5, "lb1",   ["lb1"]>, DwarfRegNum<[100]>;

  def SIMT_LC2 :   V5SIMTReg<2,  8, "lc2",   ["lc2"]>, DwarfRegNum<[100]>;
  def SIMT_LB2 :   V5SIMTReg<2,  9, "lb2",   ["lb2"]>, DwarfRegNum<[100]>;

  def SIMT_TA :   V5SIMTReg<2,  16, "ta",   ["ta"]>, DwarfRegNum<[100]>;
  def SIMT_TB :   V5SIMTReg<2,  17, "tb",   ["tb"]>, DwarfRegNum<[100]>;
  def SIMT_TC :   V5SIMTReg<2,  18, "tc",   ["tc"]>, DwarfRegNum<[100]>;
  def SIMT_TD :   V5SIMTReg<2,  19, "td",   ["td"]>, DwarfRegNum<[100]>;
  def SIMT_TE :   V5SIMTReg<2,  20, "te",   ["te"]>, DwarfRegNum<[100]>;
  def SIMT_TF :   V5SIMTReg<2,  21, "tf",   ["tf"]>, DwarfRegNum<[100]>;
  def SIMT_TG :   V5SIMTReg<2,  22, "tg",   ["tg"]>, DwarfRegNum<[100]>;
  def SIMT_TH :   V5SIMTReg<2,  23, "th",   ["th"]>, DwarfRegNum<[100]>;
  def SIMT_TO :   V5SIMTReg<2,  24, "to",   ["to"]>, DwarfRegNum<[100]>;
  def SIMT_TO1 :   V5SIMTReg<2,  25, "to1",   ["to1", "ts"]>, DwarfRegNum<[100]>;
  def SIMT_TO2 :   V5SIMTReg<2,  26, "to2",   ["to2"]>, DwarfRegNum<[100]>;
  def SIMT_TO3 :   V5SIMTReg<2,  27, "to3",   ["to3"]>, DwarfRegNum<[100]>;

  def SIMT_P  :   V5SIMTReg<2,  28, "p",   ["p"]>, DwarfRegNum<[100]>;
  def SIMT_ZERO : V5SIMTReg<2,  31, "zero", ["zero"]>, DwarfRegNum<[100]>;

  def SIMT_VT1  :  V5SIMTReg<0,  0, "vt1",  ["vt1"]>, DwarfRegNum<[100]>;
  def SIMT_VT2  :  V5SIMTReg<0,  1, "vt2",  ["vt2"]>, DwarfRegNum<[100]>;
  def SIMT_VT3  :  V5SIMTReg<0,  2, "vt3",  ["vt3"]>, DwarfRegNum<[100]>;
  def SIMT_VT4  :  V5SIMTReg<0,  3, "vt4",  ["vt4"]>, DwarfRegNum<[100]>;

  def SIMT_VU1  :  V5SIMTReg<0,  8, "vu1",  ["vu1"]>, DwarfRegNum<[100]>;
  def SIMT_VU2  :  V5SIMTReg<0,  9, "vu2",  ["vu2"]>, DwarfRegNum<[100]>;
  def SIMT_VU3  :  V5SIMTReg<0, 10, "vu3",  ["vu3"]>, DwarfRegNum<[100]>;
  def SIMT_VU4  :  V5SIMTReg<0, 11, "vu4",  ["vu4"]>, DwarfRegNum<[100]>;

  def SIMT_VM1  :  V5SIMTReg<0, 16, "vm1",  ["vm1"]>, DwarfRegNum<[100]>;
  def SIMT_VM2  :  V5SIMTReg<0, 17, "vm2",  ["vm2"]>, DwarfRegNum<[100]>;
  def SIMT_VM3  :  V5SIMTReg<0, 18, "vm3",  ["vm3"]>, DwarfRegNum<[100]>;
  def SIMT_VM4  :  V5SIMTReg<0, 19, "vm4",  ["vm4"]>, DwarfRegNum<[100]>;

  def SIMT_VN1  :  V5SIMTReg<0, 24, "vn1",  ["vn1"]>, DwarfRegNum<[100]>;
  def SIMT_VN2  :  V5SIMTReg<0, 25, "vn2",  ["vn2"]>, DwarfRegNum<[100]>;
  def SIMT_VN3  :  V5SIMTReg<0, 26, "vn3",  ["vn3"]>, DwarfRegNum<[100]>;
  def SIMT_VN4  :  V5SIMTReg<0, 27, "vn4",  ["vn4"]>, DwarfRegNum<[100]>;

}

def LoopReg : RegisterClass<"LinxV5", [i64], 64, (add
  SIMT_LC0, SIMT_LB0,
  SIMT_LC1, SIMT_LB1,
  SIMT_LC2, SIMT_LB2
)>;

let isAllocatable = 0 in
def SIMT_TileBase : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  SIMT_TA, SIMT_TB, SIMT_TC, SIMT_TD, SIMT_TE, SIMT_TF, SIMT_TG, SIMT_TH,
  SIMT_TO, SIMT_TO1, SIMT_TO2, SIMT_TO3
)>;

def SIMT_VTR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_VT%u", 1, 4)
)>;

def SIMT_VUR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_VU%u", 1, 4)
)>;

def SIMT_VMR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_VM%u", 1, 4)
)>;

def SIMT_VNR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_VN%u", 1, 4)
)>;

let isAllocatable = 0 in
def SIMT_OSVTR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_OSVT%u", 1, 4)
)>;

let isAllocatable = 0 in
def SIMT_OSVUR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_OSVU%u", 1, 4)
)>;

let isAllocatable = 0 in
def SIMT_OSVMR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_OSVM%u", 1, 4)
)>;

let isAllocatable = 0 in
def SIMT_OSVNR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_OSVN%u", 1, 4)
)>;

let isAllocatable = 0 in
def SIMT_OSVR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_OSVT%u", 1, 4),
  (sequence "SIMT_OSVU%u", 1, 4),
  (sequence "SIMT_OSVM%u", 1, 4),
  (sequence "SIMT_OSVN%u", 1, 4)
)> {
  let isAllocatable = 0;
}

let isAllocatable = 0 in
def SIMT_OSVKR : RegisterClass<"LinxV5", [i64], 64, (add
  (sequence "SIMT_OSVT%u_RU", 1, 4),
  (sequence "SIMT_OSVU%u_RU", 1, 4),
  (sequence "SIMT_OSVM%u_RU", 1, 4),
  (sequence "SIMT_OSVN%u_RU", 1, 4)
)> {
  let isAllocatable = 0;
}

let isAllocatable = 0 in
def SIMT_RIO : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  (sequence "SIMT_RI%u", 0, 11),
  (sequence "SIMT_RO%u", 0, 3)
)>;

let isAllocatable = 0 in
def UniformReg : RegisterClass<"LinxV5", [i64], 64, (add
  SIMT_LC1, SIMT_LC2,
  SIMT_LB0, SIMT_LB1, SIMT_LB2,
  SIMT_T, SIMT_U,
  SIMT_P,
  SIMT_TileBase,
  Tile_ABS,
  SIMT_RIO,
  LTR,
  LUR,
  R0
)>;

// This regclass is compat for 32-bits scalar instr. As the 32-bits scalar gpr does not
// write P
def CGSL : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  LTR, LUR, R0
)>;

def SIMTCGSL : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  LTR, LUR, SIMT_P, R0
)>;

def SIMTCGS : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  SIMTCGSL,
  (sequence "SIMT_RI%u", 0, 11),
  LoopReg,
  SIMT_TileBase,
  R0
)>;

def SIMTCGVL : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  (sequence "SIMT_VT%u", 1, 4),
  (sequence "SIMT_VU%u", 1, 4),
  (sequence "SIMT_VM%u", 1, 4),
  (sequence "SIMT_VN%u", 1, 4)
)>;

def SIMTCGV : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  SIMTCGVL,
  SIMTCGSL,
  (sequence "SIMT_RI%u", 0, 11),
  LoopReg,
  SIMT_TileBase,
  R0
)>;

let isAllocatable = 0 in
def SIMT_SRC_Scalar_MC : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  (sequence "SIMT_RI%u", 0, 11),
  (sequence "SIMT_OST%u", 1, 4),
  (sequence "SIMT_OSU%u", 1, 4),
  LoopReg,
  SIMT_TileBase,
  SIMT_P
)>;

let isAllocatable = 0 in
def SIMT_SRC_Vec_MC : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  (sequence "SIMT_RI%u", 0, 11),
  (sequence "SIMT_OST%u", 1, 4),
  (sequence "SIMT_OSU%u", 1, 4),
  SIMT_OSVR,
  SIMT_OSVKR,
  LoopReg,
  SIMT_TileBase
)>;

let isAllocatable = 0 in
def SIMT_DST_Scalar_MC : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  (sequence "SIMT_RO%u", 0, 3),
  SIMT_T, SIMT_U,
  SIMT_P
)>;

let isAllocatable = 0 in
def SIMT_DST_CMP_Scalar_MC : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  (sequence "SIMT_RO%u", 0, 3),
  SIMT_T, SIMT_U
)>;

let isAllocatable = 0 in
def SIMT_SRC_Vec : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  // For RA
  SIMTCGV,
  // For MC
  SIMT_SRC_Vec_MC
)>;

let isAllocatable = 0 in
def SIMT_SRC_Scalar : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  // For RA
  SIMTCGS,
  // For MC
  SIMT_SRC_Scalar_MC
)>;

let isAllocatable = 0 in
def SIMT_SRC : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  SIMT_SRC_Vec,
  SIMT_SRC_Scalar
)>;

let isAllocatable = 0 in
def SIMT_DST_Vec_MC : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  SIMT_VT, SIMT_VU,
  SIMT_VM, SIMT_VN
)>;

let isAllocatable = 0 in
def SIMT_DST_Vec : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
  // For RA
  SIMTCGVL,
  // For MC
  SIMT_DST_Vec_MC
)>;

let isAllocatable = 0 in
def SIMT_DST_Scalar : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
   // For RA
  SIMTCGSL,
  // For MC
  SIMT_DST_Scalar_MC
)>;

let isAllocatable = 0 in
def SIMT_DST : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
 SIMT_DST_Scalar,
 SIMT_DST_Vec
)>;

let isAllocatable = 0 in
def SIMT_DST_CMP_Scalar : RegisterClass<"LinxV5", [i64, i32, i16, i8, bf16, f16, f32, f64], 64, (add
   // For RA
  CGSL,
  // For MC
  SIMT_DST_CMP_Scalar_MC
)>;
