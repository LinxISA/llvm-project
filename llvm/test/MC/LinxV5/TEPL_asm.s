// RUN: llvm-mc %s --triple=linx64v5 --show-encoding -linxv5-enable-compress-inst=false| FileCheck %s --dump-input always -vv
// XFAIL: *
// The current JCore_Linxv5.patch implementation asserts in
// LinxV5AsmParser::processInstruction for TEPL tile-op expansion.

// CHECK: BSTART.TEPL     ESAVE
// CHECK: B.IOT  [], last        ->t<32KB>
ESAVE  gprs, ->T<32KB>

// CHECK: BSTART.TEPL     ESAVE
// CHECK: B.IOT  [], last        ->t<32KB>
ESAVE  tile, ->T<32KB>

// CHECK: BSTART.TEPL     ERCOV
// CHECK: B.IOT   [t#1], last
ERCOV gprs, T#1

// CHECK: BSTART.TEPL     ERCOV
// CHECK: B.IOT   [t#1], last
ERCOV tile, T#1

// CHECK: BSTART.TEPL     TADD
// CHECK: B.IOT  [], last        ->t<0B>
VPAR ->T<zero>
