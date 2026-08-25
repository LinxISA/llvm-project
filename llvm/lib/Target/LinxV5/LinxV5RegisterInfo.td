//===-- LinxV5RegisterInfo.td - LinxV5 Register defs --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the LinxV5 register files
//===----------------------------------------------------------------------===//

let Namespace = "LinxV5" in {

class LinxV5TReg<bits<16> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding = Enc;
  let AltNames = alt;
}

def ABIRegAltName : RegAltNameIndex;

} // Namespace = "LinxV5"

// Integer registers
// CostPerUse is set higher for registers that may not be compressible as they
// are not part of GPRC, the most restrictive register class used by the
// compressed instruction set. This will influence the greedy register
// allocator to reduce the use of registers that can't be encoded in 16 bit
// instructions. This affects register allocation even when compressed
// instruction isn't targeted, we see no major negative codegen impact.

def XLenVT : ValueTypeByHwMode<[RV32, RV64],
                               [i32,  i64]>;

// remain this for vbx
let RegAltNameIndices = [ABIRegAltName] in {
  foreach Index = 0-7 in {
    def VBXT#Index : LinxV5TReg<Index, "t"#Index, ["t"#Index]>, DwarfRegNum<[!add(Index, 32)]>;
  }
}

// The order of registers represents the preferred allocation sequence.
def VBXTR : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    (sequence "VBXT%u", 0, 7)
  )>;

let Namespace = "LinxV5" in {
class LinxV5Reg<bits<5> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding{4-0} = Enc;
  let AltNames = alt;
}

} // Namespace = "LinxV5"

let RegAltNameIndices = [ABIRegAltName] in {
  // reserved BPC alias bpc, compiler do not use it.
  // FIXME: Use subTarget fix Dwarf Reg Num alias between LinxV5V3 and LinxV5 V4

  // Global Register
  def R0   :  LinxV5Reg<0,  "r0",  ["zero"]>,      DwarfRegNum<[0 ]>;
  def R1   :  LinxV5Reg<1,  "r1",  ["sp"]>,        DwarfRegNum<[1 ]>;
  def R2   :  LinxV5Reg<2,  "r2",  ["a0"]>,        DwarfRegNum<[2 ]>;
  def R3   :  LinxV5Reg<3,  "r3",  ["a1"]>,        DwarfRegNum<[3 ]>;
  def R4   :  LinxV5Reg<4,  "r4",  ["a2"]>,        DwarfRegNum<[4 ]>;
  def R5   :  LinxV5Reg<5,  "r5",  ["a3"]>,        DwarfRegNum<[5 ]>;
  def R6   :  LinxV5Reg<6,  "r6",  ["a4"]>,        DwarfRegNum<[6 ]>;
  def R7   :  LinxV5Reg<7,  "r7",  ["a5"]>,        DwarfRegNum<[7 ]>;
  def R8   :  LinxV5Reg<8,  "r8",  ["a6"]>,        DwarfRegNum<[8 ]>;
  def R9   :  LinxV5Reg<9,  "r9",  ["a7"]>,        DwarfRegNum<[9 ]>;
  def R10  :  LinxV5Reg<10, "r10", ["ra"]>,        DwarfRegNum<[10]>;
  def R11  :  LinxV5Reg<11, "r11", ["s0", "fp"]>,  DwarfRegNum<[11]>;
  def R12  :  LinxV5Reg<12, "r12", ["s1"]>,        DwarfRegNum<[12]>;
  def R13  :  LinxV5Reg<13, "r13", ["s2"]>,        DwarfRegNum<[13]>;
  def R14  :  LinxV5Reg<14, "r14", ["s3"]>,        DwarfRegNum<[14]>;
  def R15  :  LinxV5Reg<15, "r15", ["s4"]>,        DwarfRegNum<[15]>;
  def R16  :  LinxV5Reg<16, "r16", ["s5"]>,        DwarfRegNum<[16]>;
  def R17  :  LinxV5Reg<17, "r17", ["s6"]>,        DwarfRegNum<[17]>;
  def R18  :  LinxV5Reg<18, "r18", ["s7"]>,        DwarfRegNum<[18]>;
  def R19  :  LinxV5Reg<19, "r19", ["s8"]>,        DwarfRegNum<[19]>;
  def R20  :  LinxV5Reg<20, "r20", ["x0"]>,        DwarfRegNum<[20]>;
  def R21  :  LinxV5Reg<21, "r21", ["x1"]>,        DwarfRegNum<[21]>;
  def R22  :  LinxV5Reg<22, "r22", ["x2"]>,        DwarfRegNum<[22]>;
  def R23  :  LinxV5Reg<23, "r23", ["x3"]>,        DwarfRegNum<[23]>;

  def TP   :  LinxV5Reg<24, "tp",  ["tp"]>,        DwarfRegNum<[24]>;
  // Local Register: Input(abs index)
  def T1   :  LinxV5Reg<24, "t1",  ["t1"]>,        DwarfRegNum<[100]>;
  def T2   :  LinxV5Reg<25, "t2",  ["t2"]>,        DwarfRegNum<[100]>;
  def T3   :  LinxV5Reg<26, "t3",  ["t3"]>,        DwarfRegNum<[100]>;
  def T4   :  LinxV5Reg<27, "t4",  ["t4"]>,        DwarfRegNum<[100]>;
  def U1   :  LinxV5Reg<28, "u1",  ["u1"]>,        DwarfRegNum<[100]>;
  def U2   :  LinxV5Reg<29, "u2",  ["u2"]>,        DwarfRegNum<[100]>;
  def U3   :  LinxV5Reg<30, "u3",  ["u3"]>,        DwarfRegNum<[100]>;
  def U4   :  LinxV5Reg<31, "u4",  ["u4"]>,        DwarfRegNum<[100]>;

  // Local Register: Output
  def TX2  :  LinxV5Reg<24, "tx2", ["tx2"]>,       DwarfRegNum<[100]>;
  def UX2  :  LinxV5Reg<25, "ux2", ["ux2"]>,       DwarfRegNum<[100]>;
  def TX4  :  LinxV5Reg<26, "tx4", ["tx4"]>,       DwarfRegNum<[100]>;
  def UX4  :  LinxV5Reg<27, "ux4", ["ux4"]>,       DwarfRegNum<[100]>;
  def T    :  LinxV5Reg<31, "t",   ["t"]>,         DwarfRegNum<[100]>;
  def U    :  LinxV5Reg<30, "u",   ["u"]>,         DwarfRegNum<[100]>;

  // Local Register: Input(offset)
  def TOS1  :  LinxV5Reg<24, "t#1",  ["t#1"]>,     DwarfRegNum<[100]>;
  def TOS2  :  LinxV5Reg<25, "t#2",  ["t#2"]>,     DwarfRegNum<[100]>;
  def TOS3  :  LinxV5Reg<26, "t#3",  ["t#3"]>,     DwarfRegNum<[100]>;
  def TOS4  :  LinxV5Reg<27, "t#4",  ["t#4"]>,     DwarfRegNum<[100]>;
  def UOS1  :  LinxV5Reg<28, "u#1",  ["u#1"]>,     DwarfRegNum<[100]>;
  def UOS2  :  LinxV5Reg<29, "u#2",  ["u#2"]>,     DwarfRegNum<[100]>;
  def UOS3  :  LinxV5Reg<30, "u#3",  ["u#3"]>,     DwarfRegNum<[100]>;
  def UOS4  :  LinxV5Reg<31, "u#4",  ["u#4"]>,     DwarfRegNum<[100]>;
}

// This is actually a System Register in LinxV5. But we do not generate it.
// So just introduce without name and coding info.
// We use it to describe relationships between setc and branch instructions.
let Namespace = "LinxV5" in
def CARG : Register<"">;

// -------- -- These classes are used for register allocation -- -------- //

def BSTATE : RegisterClass<"LinxV5", [i64], 64, (add CARG)> {
  let CopyCost = -1; // Don't allow copying of status registers
  let isAllocatable = 0;
}

// The order of registers represents the preferred allocation sequence.
// Registers are listed in the order caller-save, callee-save, specials.
def GR : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    (sequence "R%u", 2, 9),
    (sequence "R%u", 20, 23),
    (sequence "R%u", 11, 19),
    R10,
    R1,
    R0
)>;

def GRNoRA : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    (sequence "R%u", 2, 9),
    (sequence "R%u", 20, 23),
    (sequence "R%u", 11, 19),
    R1,
    R0
)>;

def GRNoR0 : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    (sequence "R%u", 2, 9),
    (sequence "R%u", 20, 23),
    (sequence "R%u", 11, 19),
    R10,
    R1
)>;

def LTR : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    (sequence "T%u", 1, 4)
)>;

def LUR : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    (sequence "U%u", 1, 4)
)>;

def MixedGPR : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    GR,
    LTR,
    LUR
)>;

def MixedGPRNoRA : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    GRNoRA,
    LTR,
    LUR
)>;

// ------------- -- These classes are used for MC layer -- ------------- //
let isAllocatable = 0 in
def MCSrc : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    // for vbx
    VBXTR,
    // for code-gen
    MixedGPR,
    // for mc
    (sequence "TOS%u", 1, 4),
    (sequence "UOS%u", 1, 4)
)>;

let isAllocatable = 0 in
def MCDst : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    // for vbx
    VBXTR,
    // for code-gen
    MixedGPR,
    // for mc
    T,
    U,
    TX2,
    UX2,
    TX4,
    UX4
)>;

let isAllocatable = 0 in
def MCDstNoRA : RegisterClass<"LinxV5", [i64, f64, f32, f16], 64, (add
    // for vbx
    VBXTR,
    // for code-gen
    MixedGPRNoRA,
    // for mc
    T,
    U,
    TX2,
    UX2,
    TX4,
    UX4
)>;

let Namespace = "LinxV5" in {
class LinxV5TileRegBase<bits<6> Enc, string n, list<string> alt = []> : Register<n> {
  // v5: source reuse (KillFlag) removed. HWEncoding is 6-bit only.
  let HWEncoding{5-0} = Enc;
  let AltNames = alt;
}

class LinxV5TileReg<bits<6> Enc, string n, list<string> alt = []> : LinxV5TileRegBase<Enc, n, alt>;

class LinxV5TileDstReg<bits<3> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding{2-0} = Enc;
  let AltNames = alt;
}

class LinxV5DepSrcReg<bits<5> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding{4-0} = Enc;
  let AltNames = alt;
}

class LinxV5SharedReg<bits<8> Enc, string n, list<string> alt = []>
    : Register<n> {
  let HWEncoding{7-0} = Enc;
  let AltNames = alt;
}

} // Namespace = "LinxV5"

let RegAltNameIndices = [ABIRegAltName] in {
  // For RA
  def Tile_T1   :  LinxV5TileReg<0,   "tile_t1",  ["tile_t1"]>,      DwarfRegNum<[100]>;
  def Tile_T2   :  LinxV5TileReg<1,   "tile_t2",  ["tile_t2"]>,      DwarfRegNum<[100]>;
  def Tile_T3   :  LinxV5TileReg<2,   "tile_t3",  ["tile_t3"]>,      DwarfRegNum<[100]>;
  def Tile_T4   :  LinxV5TileReg<3,   "tile_t4",  ["tile_t4"]>,      DwarfRegNum<[100]>;
  def Tile_T5   :  LinxV5TileReg<4,   "tile_t5",  ["tile_t5"]>,      DwarfRegNum<[100]>;
  def Tile_T6   :  LinxV5TileReg<5,   "tile_t6",  ["tile_t6"]>,      DwarfRegNum<[100]>;
  def Tile_T7   :  LinxV5TileReg<6,   "tile_t7",  ["tile_t7"]>,      DwarfRegNum<[100]>;
  def Tile_T8   :  LinxV5TileReg<7,   "tile_t8",  ["tile_t8"]>,      DwarfRegNum<[100]>;
  def Tile_T9   :  LinxV5TileReg<8,   "tile_t9",  ["tile_t9"]>,      DwarfRegNum<[100]>;
  def Tile_T10  :  LinxV5TileReg<9,   "tile_t10", ["tile_t10"]>,     DwarfRegNum<[100]>;
  def Tile_T11  :  LinxV5TileReg<10,  "tile_t11", ["tile_t11"]>,     DwarfRegNum<[100]>;
  def Tile_T12  :  LinxV5TileReg<11,  "tile_t12", ["tile_t12"]>,     DwarfRegNum<[100]>;
  def Tile_T13  :  LinxV5TileReg<12,  "tile_t13", ["tile_t13"]>,     DwarfRegNum<[100]>;
  def Tile_T14  :  LinxV5TileReg<13,  "tile_t14", ["tile_t14"]>,     DwarfRegNum<[100]>;
  def Tile_T15  :  LinxV5TileReg<14,  "tile_t15", ["tile_t15"]>,     DwarfRegNum<[100]>;
  def Tile_T16  :  LinxV5TileReg<15,  "tile_t16", ["tile_t16"]>,     DwarfRegNum<[100]>;

  def Tile_U1   :  LinxV5TileReg<16,  "tile_u1",  ["tile_u1"]>,      DwarfRegNum<[100]>;
  def Tile_U2   :  LinxV5TileReg<17,  "tile_u2",  ["tile_u2"]>,      DwarfRegNum<[100]>;
  def Tile_U3   :  LinxV5TileReg<18,  "tile_u3",  ["tile_u3"]>,      DwarfRegNum<[100]>;
  def Tile_U4   :  LinxV5TileReg<19,  "tile_u4",  ["tile_u4"]>,      DwarfRegNum<[100]>;
  def Tile_U5   :  LinxV5TileReg<20,  "tile_u5",  ["tile_u5"]>,      DwarfRegNum<[100]>;
  def Tile_U6   :  LinxV5TileReg<21,  "tile_u6",  ["tile_u6"]>,      DwarfRegNum<[100]>;
  def Tile_U7   :  LinxV5TileReg<22,  "tile_u7",  ["tile_u7"]>,      DwarfRegNum<[100]>;
  def Tile_U8   :  LinxV5TileReg<23,  "tile_u8",  ["tile_u8"]>,      DwarfRegNum<[100]>;
  def Tile_U9   :  LinxV5TileReg<24, "tile_u9",  ["tile_u9"]>,       DwarfRegNum<[100]>;
  def Tile_U10  :  LinxV5TileReg<25, "tile_u10", ["tile_u10"]>,      DwarfRegNum<[100]>;
  def Tile_U11  :  LinxV5TileReg<26, "tile_u11", ["tile_u11"]>,      DwarfRegNum<[100]>;
  def Tile_U12  :  LinxV5TileReg<27, "tile_u12", ["tile_u12"]>,      DwarfRegNum<[100]>;
  def Tile_U13  :  LinxV5TileReg<28, "tile_u13", ["tile_u13"]>,      DwarfRegNum<[100]>;
  def Tile_U14  :  LinxV5TileReg<29, "tile_u14", ["tile_u14"]>,      DwarfRegNum<[100]>;
  def Tile_U15  :  LinxV5TileReg<30, "tile_u15", ["tile_u15"]>,      DwarfRegNum<[100]>;
  def Tile_U16  :  LinxV5TileReg<31, "tile_u16", ["tile_u16"]>,      DwarfRegNum<[100]>;

  def Tile_M1   :  LinxV5TileReg<32,  "tile_m1",  ["tile_m1"]>,      DwarfRegNum<[100]>;
  def Tile_M2   :  LinxV5TileReg<33,  "tile_m2",  ["tile_m2"]>,      DwarfRegNum<[100]>;
  def Tile_M3   :  LinxV5TileReg<34,  "tile_m3",  ["tile_m3"]>,      DwarfRegNum<[100]>;
  def Tile_M4   :  LinxV5TileReg<35,  "tile_m4",  ["tile_m4"]>,      DwarfRegNum<[100]>;
  def Tile_M5   :  LinxV5TileReg<36,  "tile_m5",  ["tile_m5"]>,      DwarfRegNum<[100]>;
  def Tile_M6   :  LinxV5TileReg<37,  "tile_m6",  ["tile_m6"]>,      DwarfRegNum<[100]>;
  def Tile_M7   :  LinxV5TileReg<38,  "tile_m7",  ["tile_m7"]>,      DwarfRegNum<[100]>;
  def Tile_M8   :  LinxV5TileReg<39,  "tile_m8",  ["tile_m8"]>,      DwarfRegNum<[100]>;
  def Tile_M9   :  LinxV5TileReg<40, "tile_m9",  ["tile_m9"]>,       DwarfRegNum<[100]>;
  def Tile_M10  :  LinxV5TileReg<41, "tile_m10", ["tile_m10"]>,      DwarfRegNum<[100]>;
  def Tile_M11  :  LinxV5TileReg<42, "tile_m11", ["tile_m11"]>,      DwarfRegNum<[100]>;
  def Tile_M12  :  LinxV5TileReg<43, "tile_m12", ["tile_m12"]>,      DwarfRegNum<[100]>;
  def Tile_M13  :  LinxV5TileReg<44, "tile_m13", ["tile_m13"]>,      DwarfRegNum<[100]>;
  def Tile_M14  :  LinxV5TileReg<45, "tile_m14", ["tile_m14"]>,      DwarfRegNum<[100]>;
  def Tile_M15  :  LinxV5TileReg<46, "tile_m15", ["tile_m15"]>,      DwarfRegNum<[100]>;
  def Tile_M16  :  LinxV5TileReg<47, "tile_m16", ["tile_m16"]>,      DwarfRegNum<[100]>;

  def Tile_N1   :  LinxV5TileReg<48,  "tile_n1",  ["tile_n1"]>,      DwarfRegNum<[100]>;
  def Tile_N2   :  LinxV5TileReg<49,  "tile_n2",  ["tile_n2"]>,      DwarfRegNum<[100]>;
  def Tile_N3   :  LinxV5TileReg<50,  "tile_n3",  ["tile_n3"]>,      DwarfRegNum<[100]>;
  def Tile_N4   :  LinxV5TileReg<51,  "tile_n4",  ["tile_n4"]>,      DwarfRegNum<[100]>;
  def Tile_N5   :  LinxV5TileReg<52,  "tile_n5",  ["tile_n5"]>,      DwarfRegNum<[100]>;
  def Tile_N6   :  LinxV5TileReg<53,  "tile_n6",  ["tile_n6"]>,      DwarfRegNum<[100]>;
  def Tile_N7   :  LinxV5TileReg<54,  "tile_n7",  ["tile_n7"]>,      DwarfRegNum<[100]>;
  def Tile_N8   :  LinxV5TileReg<55,  "tile_n8",  ["tile_n8"]>,      DwarfRegNum<[100]>;
  def Tile_N9   :  LinxV5TileReg<56, "tile_n9",  ["tile_n9"]>,       DwarfRegNum<[100]>;
  def Tile_N10  :  LinxV5TileReg<57, "tile_n10", ["tile_n10"]>,      DwarfRegNum<[100]>;
  def Tile_N11  :  LinxV5TileReg<58, "tile_n11", ["tile_n11"]>,      DwarfRegNum<[100]>;
  def Tile_N12  :  LinxV5TileReg<59, "tile_n12", ["tile_n12"]>,      DwarfRegNum<[100]>;
  def Tile_N13  :  LinxV5TileReg<60, "tile_n13", ["tile_n13"]>,      DwarfRegNum<[100]>;
  def Tile_N14  :  LinxV5TileReg<61, "tile_n14", ["tile_n14"]>,      DwarfRegNum<[100]>;
  def Tile_N15  :  LinxV5TileReg<62, "tile_n15", ["tile_n15"]>,      DwarfRegNum<[100]>;
  def Tile_N16  :  LinxV5TileReg<63, "tile_n16", ["tile_n16"]>,      DwarfRegNum<[100]>;

  def Tile_ACC1 :  LinxV5TileDstReg<4, "acc1",  ["acc1"]>, DwarfRegNum<[100]>;

  // For MC
  def Tile_TOS1   :  LinxV5TileReg<0,  "t#1",  ["t#1"]>,      DwarfRegNum<[100]>;
  def Tile_TOS2   :  LinxV5TileReg<1,  "t#2",  ["t#2"]>,      DwarfRegNum<[100]>;
  def Tile_TOS3   :  LinxV5TileReg<2,  "t#3",  ["t#3"]>,      DwarfRegNum<[100]>;
  def Tile_TOS4   :  LinxV5TileReg<3,  "t#4",  ["t#4"]>,      DwarfRegNum<[100]>;
  def Tile_TOS5   :  LinxV5TileReg<4,  "t#5",  ["t#5"]>,      DwarfRegNum<[100]>;
  def Tile_TOS6   :  LinxV5TileReg<5,  "t#6",  ["t#6"]>,      DwarfRegNum<[100]>;
  def Tile_TOS7   :  LinxV5TileReg<6,  "t#7",  ["t#7"]>,      DwarfRegNum<[100]>;
  def Tile_TOS8   :  LinxV5TileReg<7,  "t#8",  ["t#8"]>,      DwarfRegNum<[100]>;
  def Tile_TOS9   :  LinxV5TileReg<8,  "t#9",   ["t#9"]>,     DwarfRegNum<[100]>;
  def Tile_TOS10  :  LinxV5TileReg<9,  "t#10",  ["t#10"]>,    DwarfRegNum<[100]>;
  def Tile_TOS11  :  LinxV5TileReg<10, "t#11",  ["t#11"]>,    DwarfRegNum<[100]>;
  def Tile_TOS12  :  LinxV5TileReg<11, "t#12",  ["t#12"]>,    DwarfRegNum<[100]>;
  def Tile_TOS13  :  LinxV5TileReg<12, "t#13",  ["t#13"]>,    DwarfRegNum<[100]>;
  def Tile_TOS14  :  LinxV5TileReg<13, "t#14",  ["t#14"]>,    DwarfRegNum<[100]>;
  def Tile_TOS15  :  LinxV5TileReg<14, "t#15",  ["t#15"]>,    DwarfRegNum<[100]>;
  def Tile_TOS16  :  LinxV5TileReg<15, "t#16",  ["t#16"]>,    DwarfRegNum<[100]>;

  def Tile_UOS1   :  LinxV5TileReg<16, "u#1",  ["u#1"]>,      DwarfRegNum<[100]>;
  def Tile_UOS2   :  LinxV5TileReg<17, "u#2",  ["u#2"]>,      DwarfRegNum<[100]>;
  def Tile_UOS3   :  LinxV5TileReg<18, "u#3",  ["u#3"]>,      DwarfRegNum<[100]>;
  def Tile_UOS4   :  LinxV5TileReg<19, "u#4",  ["u#4"]>,      DwarfRegNum<[100]>;
  def Tile_UOS5   :  LinxV5TileReg<20, "u#5",  ["u#5"]>,      DwarfRegNum<[100]>;
  def Tile_UOS6   :  LinxV5TileReg<21, "u#6",  ["u#6"]>,      DwarfRegNum<[100]>;
  def Tile_UOS7   :  LinxV5TileReg<22, "u#7",  ["u#7"]>,      DwarfRegNum<[100]>;
  def Tile_UOS8   :  LinxV5TileReg<23, "u#8",  ["u#8"]>,      DwarfRegNum<[100]>;
  def Tile_UOS9   :  LinxV5TileReg<24, "u#9",   ["u#9"]>,     DwarfRegNum<[100]>;
  def Tile_UOS10  :  LinxV5TileReg<25, "u#10",  ["u#10"]>,    DwarfRegNum<[100]>;
  def Tile_UOS11  :  LinxV5TileReg<26, "u#11",  ["u#11"]>,    DwarfRegNum<[100]>;
  def Tile_UOS12  :  LinxV5TileReg<27, "u#12",  ["u#12"]>,    DwarfRegNum<[100]>;
  def Tile_UOS13  :  LinxV5TileReg<28, "u#13",  ["u#13"]>,    DwarfRegNum<[100]>;
  def Tile_UOS14  :  LinxV5TileReg<29, "u#14",  ["u#14"]>,    DwarfRegNum<[100]>;
  def Tile_UOS15  :  LinxV5TileReg<30, "u#15",  ["u#15"]>,    DwarfRegNum<[100]>;
  def Tile_UOS16  :  LinxV5TileReg<31, "u#16",  ["u#16"]>,    DwarfRegNum<[100]>;

  def Tile_MOS1   :  LinxV5TileReg<32,  "m#1",  ["m#1"]>,      DwarfRegNum<[100]>;
  def Tile_MOS2   :  LinxV5TileReg<33,  "m#2",  ["m#2"]>,      DwarfRegNum<[100]>;
  def Tile_MOS3   :  LinxV5TileReg<34,  "m#3",  ["m#3"]>,      DwarfRegNum<[100]>;
  def Tile_MOS4   :  LinxV5TileReg<35,  "m#4",  ["m#4"]>,      DwarfRegNum<[100]>;
  def Tile_MOS5   :  LinxV5TileReg<36,  "m#5",  ["m#5"]>,      DwarfRegNum<[100]>;
  def Tile_MOS6   :  LinxV5TileReg<37,  "m#6",  ["m#6"]>,      DwarfRegNum<[100]>;
  def Tile_MOS7   :  LinxV5TileReg<38,  "m#7",  ["m#7"]>,      DwarfRegNum<[100]>;
  def Tile_MOS8   :  LinxV5TileReg<39,  "m#8",  ["m#8"]>,      DwarfRegNum<[100]>;
  def Tile_MOS9   :  LinxV5TileReg<40, "m#9",   ["m#9"]>,     DwarfRegNum<[100]>;
  def Tile_MOS10  :  LinxV5TileReg<41, "m#10",  ["m#10"]>,    DwarfRegNum<[100]>;
  def Tile_MOS11  :  LinxV5TileReg<42, "m#11",  ["m#11"]>,    DwarfRegNum<[100]>;
  def Tile_MOS12  :  LinxV5TileReg<43, "m#12",  ["m#12"]>,    DwarfRegNum<[100]>;
  def Tile_MOS13  :  LinxV5TileReg<44, "m#13",  ["m#13"]>,    DwarfRegNum<[100]>;
  def Tile_MOS14  :  LinxV5TileReg<45, "m#14",  ["m#14"]>,    DwarfRegNum<[100]>;
  def Tile_MOS15  :  LinxV5TileReg<46, "m#15",  ["m#15"]>,    DwarfRegNum<[100]>;
  def Tile_MOS16  :  LinxV5TileReg<47, "m#16",  ["m#16"]>,    DwarfRegNum<[100]>;

  def Tile_NOS1   :  LinxV5TileReg<48,  "n#1",  ["n#1"]>,      DwarfRegNum<[100]>;
  def Tile_NOS2   :  LinxV5TileReg<49,  "n#2",  ["n#2"]>,      DwarfRegNum<[100]>;
  def Tile_NOS3   :  LinxV5TileReg<50,  "n#3",  ["n#3"]>,      DwarfRegNum<[100]>;
  def Tile_NOS4   :  LinxV5TileReg<51,  "n#4",  ["n#4"]>,      DwarfRegNum<[100]>;
  def Tile_NOS5   :  LinxV5TileReg<52,  "n#5",  ["n#5"]>,      DwarfRegNum<[100]>;
  def Tile_NOS6   :  LinxV5TileReg<53,  "n#6",  ["n#6"]>,      DwarfRegNum<[100]>;
  def Tile_NOS7   :  LinxV5TileReg<54,  "n#7",  ["n#7"]>,      DwarfRegNum<[100]>;
  def Tile_NOS8   :  LinxV5TileReg<55,  "n#8",  ["n#8"]>,      DwarfRegNum<[100]>;
  def Tile_NOS9   :  LinxV5TileReg<56, "n#9",   ["n#9"]>,      DwarfRegNum<[100]>;
  def Tile_NOS10  :  LinxV5TileReg<57, "n#10",  ["n#10"]>,     DwarfRegNum<[100]>;
  def Tile_NOS11  :  LinxV5TileReg<58, "n#11",  ["n#11"]>,     DwarfRegNum<[100]>;
  def Tile_NOS12  :  LinxV5TileReg<59, "n#12",  ["n#12"]>,     DwarfRegNum<[100]>;
  def Tile_NOS13  :  LinxV5TileReg<60, "n#13",  ["n#13"]>,     DwarfRegNum<[100]>;
  def Tile_NOS14  :  LinxV5TileReg<61, "n#14",  ["n#14"]>,     DwarfRegNum<[100]>;
  def Tile_NOS15  :  LinxV5TileReg<62, "n#15",  ["n#15"]>,     DwarfRegNum<[100]>;
  def Tile_NOS16  :  LinxV5TileReg<63, "n#16",  ["n#16"]>,     DwarfRegNum<[100]>;

  def Tile_ACCOS1   :  LinxV5TileReg<31,  "acc#1",  ["acc#1"]>,      DwarfRegNum<[100]>;

  def Tile_T   :  LinxV5TileDstReg<0, "t",  ["t"]>, DwarfRegNum<[100]>;
  def Tile_U   :  LinxV5TileDstReg<1, "u",  ["u"]>, DwarfRegNum<[100]>;
  def Tile_M   :  LinxV5TileDstReg<2, "m",  ["m"]>, DwarfRegNum<[100]>;
  def Tile_N   :  LinxV5TileDstReg<3, "n",  ["n"]>, DwarfRegNum<[100]>;
  def Tile_ACC :  LinxV5TileDstReg<4, "acc",  ["acc"]>, DwarfRegNum<[100]>;
  def Tile_S   :  LinxV5TileDstReg<5, "s",  ["s"]>, DwarfRegNum<[100]>;

  def Dep_DOS0 :  LinxV5DepSrcReg<0, "d#0",  ["d#0"]>, DwarfRegNum<[100]>;
  def Dep_DOS1 :  LinxV5DepSrcReg<1, "d#1",  ["d#1"]>, DwarfRegNum<[100]>;
  def Dep_DOS2 :  LinxV5DepSrcReg<2, "d#2",  ["d#2"]>, DwarfRegNum<[100]>;
  def Dep_DOS3 :  LinxV5DepSrcReg<3, "d#3",  ["d#3"]>, DwarfRegNum<[100]>;
  def Dep_DOS4 :  LinxV5DepSrcReg<4, "d#4",  ["d#4"]>, DwarfRegNum<[100]>;
  def Dep_DOS5 :  LinxV5DepSrcReg<5, "d#5",  ["d#5"]>, DwarfRegNum<[100]>;
  def Dep_DOS6 :  LinxV5DepSrcReg<6, "d#6",  ["d#6"]>, DwarfRegNum<[100]>;
  def Dep_DOS7 :  LinxV5DepSrcReg<7, "d#7",  ["d#7"]>, DwarfRegNum<[100]>;
  def Dep_DOS8 :  LinxV5DepSrcReg<8, "d#8",  ["d#8"]>, DwarfRegNum<[100]>;
}

def Tile_TR  : RegisterClass<"LinxV5", [vAny], 64, (add
  (sequence "Tile_T%u", 1, 16)
)> {
  let Size = 64;
}

def Tile_UR  : RegisterClass<"LinxV5", [vAny], 64, (add
  (sequence "Tile_U%u", 1, 16)
)> {
  let Size = 64;
}

def Tile_MR  : RegisterClass<"LinxV5", [vAny], 64, (add
  (sequence "Tile_M%u", 1, 16)
)> {
  let Size = 64;
}

def Tile_NR  : RegisterClass<"LinxV5", [vAny], 64, (add
  (sequence "Tile_N%u", 1, 16)
)> {
  let Size = 64;
}

def Tile_ABS  : RegisterClass<"LinxV5", [vAny], 64, (add
  Tile_T1,  Tile_U1,  Tile_M1,  Tile_N1,
  Tile_T2,  Tile_U2,  Tile_M2,  Tile_N2,
  Tile_T3,  Tile_U3,  Tile_M3,  Tile_N3,
  Tile_T4,  Tile_U4,  Tile_M4,  Tile_N4,
  Tile_T5,  Tile_U5,  Tile_M5,  Tile_N5,
  Tile_T6,  Tile_U6,  Tile_M6,  Tile_N6,
  Tile_T7,  Tile_U7,  Tile_M7,  Tile_N7,
  Tile_T8,  Tile_U8,  Tile_M8,  Tile_N8,
  Tile_T9,  Tile_U9,  Tile_M9,  Tile_N9,
  Tile_T10, Tile_U10, Tile_M10, Tile_N10,
  Tile_T11, Tile_U11, Tile_M11, Tile_N11,
  Tile_T12, Tile_U12, Tile_M12, Tile_N12,
  Tile_T13, Tile_U13, Tile_M13, Tile_N13,
  Tile_T14, Tile_U14, Tile_M14, Tile_N14,
  Tile_T15, Tile_U15, Tile_M15, Tile_N15,
  Tile_T16, Tile_U16, Tile_M16, Tile_N16
)> {
  let Size = 64;
}

// CodeGen only abs register
def Tile_ABS_CG  : RegisterClass<"LinxV5", [vAny], 64, (add
  Tile_TR, Tile_UR, Tile_MR, Tile_NR, Tile_ACC1
)> {
  let Size = 64;
}

def Tile_TROS  : RegisterClass<"LinxV5", [vAny], 64, (add
  (sequence "Tile_TOS%u", 1, 16)
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def Tile_UROS  : RegisterClass<"LinxV5", [vAny], 64, (add
  (sequence "Tile_UOS%u", 1, 16)
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def Tile_MROS  : RegisterClass<"LinxV5", [vAny], 64, (add
  (sequence "Tile_MOS%u", 1, 16)
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def Tile_NROS  : RegisterClass<"LinxV5", [vAny], 64, (add
  (sequence "Tile_NOS%u", 1, 16)
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def Tile_OS : RegisterClass<"LinxV5", [vAny], 64, (add
  Tile_TROS, Tile_UROS, Tile_MROS, Tile_NROS
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def TILE_SRC : RegisterClass<"LinxV5", [vAny], 64, (add
  // For RA
  Tile_ABS,
  // For MC
  (sequence "Tile_TOS%u", 1, 16),
  (sequence "Tile_UOS%u", 1, 16),
  (sequence "Tile_MOS%u", 1, 16),
  (sequence "Tile_NOS%u", 1, 16)
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def TILE_SRC_MMA : RegisterClass<"LinxV5", [vAny], 64, (add
  // For RA
  Tile_ABS_CG,
  // For MC
  (sequence "Tile_TOS%u", 1, 16),
  (sequence "Tile_UOS%u", 1, 16),
  (sequence "Tile_MOS%u", 1, 16),
  (sequence "Tile_NOS%u", 1, 16)
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def TILE_DST : RegisterClass<"LinxV5", [vAny], 64, (add
  // For RA
  Tile_ABS,
  // For MC
  Tile_T, Tile_U, Tile_M, Tile_N
)>  {
  let Size = 64;
  let isAllocatable = 0;
}

def TILE_DSTS : RegisterClass<"LinxV5", [vAny], 64, (add
  Tile_S
)>  {
  let Size = 64;
  let isAllocatable = 0;
}

def ACC_DST : RegisterClass<"LinxV5", [vAny], 64, (add
  // For RA
  Tile_ACC1,
  // For MC
  Tile_ACC
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def TILE_ABS_ACC : RegisterClass<"LinxV5", [vAny], 64, (add
  Tile_ACC1
)> {
  let Size = 64;
}

def ACC_SRC : RegisterClass<"LinxV5", [vAny], 64, (add
  // For MC
  Tile_ACCOS1
)> {
  let Size = 64;
  let isAllocatable = 0;
}

def Dep_SRC : RegisterClass<"LinxV5", [vAny], 64, (add
  // For MC
  (sequence "Dep_DOS%u", 0, 8)
)> {
  let Size = 64;
  let isAllocatable = 0;
}

// Shared tile handles are architectural global registers. Keep them in a
// separate register file so allocation is an absolute S0..S63 assignment.
// (PTO v0.58.4: Shared spelling is "S0".."S63", no '#' prefix.)
let RegAltNameIndices = [ABIRegAltName] in {
  foreach Index = 0-63 in {
    def Shared_S#Index : LinxV5SharedReg<Index, "shared_s"#Index,
                                           ["S"#Index]>,
                                           DwarfRegNum<[100]>;
  }
}

def Shared_ABS : RegisterClass<"LinxV5", [i64], 64,
                               (sequence "Shared_S%u", 0, 63)> {
  let Size = 64;
  let isAllocatable = 1;
}
