// RUN: not %clang++ --target=linx64v5 -O2 -mlxbc -mllvm -enable-all-vector-as-tilereg=true -S -o - %s 2>&1 | FileCheck %s --dump-input always -vv

typedef double tile tile_size(512);
extern void __mtc__ copyin(tile __out__ out, double *p);

// CHECK: error: TileOP 'ACCCVT' Not supported yet, please use numeric codes for entry.
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
    "BSTART.PAR ACCCVT, %c1\n"
    "B.IOTI [%2, %3], last ->%0<%c4>\n"
    "B.IOR [%5],[]\n"
    "C.B.DIMI %c6, ->lb0\n"
    : "=Tr"(a)
    : "i"(DataType), "Tr"(b), "Tr"(c), "i"(TilesizeType), "r"(d), "i"(M)
  );
}