// RUN: %clang++ --target=linx64 -O2 -mlxbc -emit-llvm -S -o - %s | FileCheck %s --dump-input always -vv

typedef double tile tile_size(512);
extern void __mtc__ copyin(tile __out__ out, double *p);

// Simplified implementation of internal test cases in the compiler; for specific custom template block usage,
// refer to the manual.
// CHECK-LABEL: entry
// CHECK:%2 = tail call <512 x double> asm sideeffect "BSTART.CUBE 0, ${1:c}\0AB.IOTI [$2, $3], last ->$0<${4:c}>\0AB.IOR [$5],[]\0AC.B.DIMI ${6:c}, ->lb0\0A", "=@2Tr,i,@2Tr,@2Tr,i,r,i"(i32 0, <512 x double> %0, <512 x double> %1, i32 9, i32 1, i32 128)
void foo(double *p1, double *p2) {
  tile a;
  tile b;
  tile c;
  int M = 128;
  int DataType = 0; // type_traits<double>::TypeCode
  int TilesizeType = 9; // tile_type_traits<tile>::TilesizeCode
  copyin<<<M, 1, 1>>>(b, p1);
  copyin<<<M, 1, 1>>>(c, p2);
  int d = 1;

  asm volatile(
    "BSTART.CUBE 0, %c1\n"
    "B.IOTI [%2, %3], last ->%0<%c4>\n"
    "B.IOR [%5],[]\n"
    "C.B.DIMI %c6, ->lb0\n"
    : "=Tr"(a)
    : "i"(DataType), "Tr"(b), "Tr"(c), "i"(TilesizeType), "r"(d), "i"(M)
  );
}
