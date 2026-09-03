// LinxISA register definitions (v0.2).

// Registers are encoded as 5-bit values in fields like SrcL/SrcR/RegDst.
class LinxISAReg<bits<16> Enc, string Name> : Register<Name> {
  let HWEncoding = Enc;
  let Namespace = "Linx";
}

// GPRs (architected) + queue/result pseudo-registers (RegDst/Src* encodings).
def R0  : LinxISAReg<0,  "zero">, DwarfRegNum<[0]> {
  let isConstant = true;
}
def R1  : LinxISAReg<1,  "sp">, DwarfRegNum<[1]>;
def R2  : LinxISAReg<2,  "a0">, DwarfRegNum<[2]>;
def R3  : LinxISAReg<3,  "a1">, DwarfRegNum<[3]>;
def R4  : LinxISAReg<4,  "a2">, DwarfRegNum<[4]>;
def R5  : LinxISAReg<5,  "a3">, DwarfRegNum<[5]>;
def R6  : LinxISAReg<6,  "a4">, DwarfRegNum<[6]>;
def R7  : LinxISAReg<7,  "a5">, DwarfRegNum<[7]>;
def R8  : LinxISAReg<8,  "a6">, DwarfRegNum<[8]>;
def R9  : LinxISAReg<9,  "a7">, DwarfRegNum<[9]>;
def R10 : LinxISAReg<10, "ra">, DwarfRegNum<[10]>;
def R11 : LinxISAReg<11, "s0">, DwarfRegNum<[11]>;
def R12 : LinxISAReg<12, "s1">, DwarfRegNum<[12]>;
def R13 : LinxISAReg<13, "s2">, DwarfRegNum<[13]>;
def R14 : LinxISAReg<14, "s3">, DwarfRegNum<[14]>;
def R15 : LinxISAReg<15, "s4">, DwarfRegNum<[15]>;
def R16 : LinxISAReg<16, "s5">, DwarfRegNum<[16]>;
def R17 : LinxISAReg<17, "s6">, DwarfRegNum<[17]>;
def R18 : LinxISAReg<18, "s7">, DwarfRegNum<[18]>;
def R19 : LinxISAReg<19, "s8">, DwarfRegNum<[19]>;
def R20 : LinxISAReg<20, "x0">, DwarfRegNum<[20]>;
def R21 : LinxISAReg<21, "x1">, DwarfRegNum<[21]>;
def R22 : LinxISAReg<22, "x2">, DwarfRegNum<[22]>;
def R23 : LinxISAReg<23, "x3">, DwarfRegNum<[23]>;

def T1  : LinxISAReg<24, "t#1">;
def T2  : LinxISAReg<25, "t#2">;
def T3  : LinxISAReg<26, "t#3">;
def T4  : LinxISAReg<27, "t#4">;

def U1  : LinxISAReg<28, "u#1">;
def U2  : LinxISAReg<29, "u#2">;
def U3  : LinxISAReg<30, "u#3">;
def U4  : LinxISAReg<31, "u#4">;

// Tile registers: 4 hands (T/U/M/N) × depth 8 = 32 tiles.
// The current ISA encoding uses 5-bit tile indices for sources (0..31) and a
// 3-bit destination depth (0..7) plus a 1-bit group selector. The backend maps
// physical tile registers TILE0..TILE31 to hands as:
//   TILE0..TILE7   = T hand
//   TILE8..TILE15  = U hand
//   TILE16..TILE23 = M hand
//   TILE24..TILE31 = N hand
def TILE0  : LinxISAReg<0,  "tile0">;
def TILE1  : LinxISAReg<1,  "tile1">;
def TILE2  : LinxISAReg<2,  "tile2">;
def TILE3  : LinxISAReg<3,  "tile3">;
def TILE4  : LinxISAReg<4,  "tile4">;
def TILE5  : LinxISAReg<5,  "tile5">;
def TILE6  : LinxISAReg<6,  "tile6">;
def TILE7  : LinxISAReg<7,  "tile7">;
def TILE8  : LinxISAReg<8,  "tile8">;
def TILE9  : LinxISAReg<9,  "tile9">;
def TILE10 : LinxISAReg<10, "tile10">;
def TILE11 : LinxISAReg<11, "tile11">;
def TILE12 : LinxISAReg<12, "tile12">;
def TILE13 : LinxISAReg<13, "tile13">;
def TILE14 : LinxISAReg<14, "tile14">;
def TILE15 : LinxISAReg<15, "tile15">;
def TILE16 : LinxISAReg<16, "tile16">;
def TILE17 : LinxISAReg<17, "tile17">;
def TILE18 : LinxISAReg<18, "tile18">;
def TILE19 : LinxISAReg<19, "tile19">;
def TILE20 : LinxISAReg<20, "tile20">;
def TILE21 : LinxISAReg<21, "tile21">;
def TILE22 : LinxISAReg<22, "tile22">;
def TILE23 : LinxISAReg<23, "tile23">;
def TILE24 : LinxISAReg<24, "tile24">;
def TILE25 : LinxISAReg<25, "tile25">;
def TILE26 : LinxISAReg<26, "tile26">;
def TILE27 : LinxISAReg<27, "tile27">;
def TILE28 : LinxISAReg<28, "tile28">;
def TILE29 : LinxISAReg<29, "tile29">;
def TILE30 : LinxISAReg<30, "tile30">;
def TILE31 : LinxISAReg<31, "tile31">;

// Core-private Shared tile register identifiers.  They are compiler-managed
// opaque handles carried by inline assembly, not ordinary scalar values.
foreach I = 0-255 in
  def SHARED#I : LinxISAReg<I, "shared"#I>;

// GPR_Arch: Architectural GPRs only (R0-R23), no T/U queue references.
// Use this for operands that must hold actual values (like comparison results).
def GPR_Arch : RegisterClass<"Linx", [i64, i32, f64, f32], 64,
    (sequence "R%i", 0, 23)>;

// Indirect call targets must not occupy ABI argument registers. CALL/ICALL
// block headers consume a0-a7 implicitly for arguments, so placing the callee
// in a0 can overwrite argument 0 at the transfer boundary.
def GPR_CallTarget : RegisterClass<"Linx", [i64, i32], 64,
    (sequence "R%i", 20, 23)>;

// GPR: Full GPR class including T/U queue read-only references.
// The T/U queue registers can be used as source operands to read from the queue.
def GPR : RegisterClass<"Linx", [i64, i32, f64, f32], 64,
    (add (sequence "R%i", 0, 23),
         T1, T2, T3, T4,
         U1, U2, U3, U4)>;

// Tile register classes.
//
// Keep `v1024i32` as a temporary bridge VT for legacy clang builtins while
// the frontend migrates to target("linx.tile") values.
def TILE : RegisterClass<"Linx", [linxtile, v1024i32], 32768,
    (sequence "TILE%i", 0, 31)>;

// Sub-classes for encodable destination banks used by block types:
// - TLSU blocks write into T/U hands (TILE0..TILE15) selected by group.
// - CUBE blocks write into M/N hands (TILE16..TILE31) selected by group.
def TILE_TU : RegisterClass<"Linx", [linxtile, v1024i32], 32768,
    (sequence "TILE%i", 0, 15)>;
def TILE_MN : RegisterClass<"Linx", [linxtile, v1024i32], 32768,
    (sequence "TILE%i", 16, 31)>;

// Hand-specific classes for the canonical LinxISA 0.58 policy.
//
// These are used to enforce deterministic hand placement in post-isel MIR and
// when constructing spill/reload sequences for tile pressure scenarios.
def TILE_T : RegisterClass<"Linx", [linxtile, v1024i32], 32768,
    (sequence "TILE%i", 0, 7)>;
def TILE_U : RegisterClass<"Linx", [linxtile, v1024i32], 32768,
    (sequence "TILE%i", 8, 15)>;
def TILE_M : RegisterClass<"Linx", [linxtile, v1024i32], 32768,
    (sequence "TILE%i", 16, 23)>;
def TILE_N : RegisterClass<"Linx", [linxtile, v1024i32], 32768,
    (sequence "TILE%i", 24, 31)>;

def SHARED : RegisterClass<"Linx", [i64], 64,
    (sequence "SHARED%i", 0, 63)>;

def CSR_Linx : CalleeSavedRegs<(add R10,
                                   (sequence "R%i", 11, 19))>;
