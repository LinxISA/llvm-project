// RUN: llvm-mc %s --triple=linx64v5 --show-encoding -linxv5-enable-compress-inst=false| FileCheck %s --dump-input always -vv

// CHECK: BSTART.TEPL     ESAVE, gprs
// CHECK: B.IOTI  [], last        ->t<32KB>
ESAVE  gprs, ->T<32KB>

// CHECK: BSTART.TEPL     ESAVE, tile
// CHECK: B.IOTI  [], last        ->t<32KB>
ESAVE  tile, ->T<32KB>

// CHECK: BSTART.TEPL     ERCOV, gprs
// CHECK: B.IOT   [t#1], last
ERCOV gprs, T#1

// CHECK: BSTART.TEPL     ERCOV, tile
// CHECK: B.IOT   [t#1], last
ERCOV tile, T#1

// CHECK: BSTART.VPAR     VS16
// CHECK: B.IOT  [] , last        ->t<zero>
VPAR ->T<zero>