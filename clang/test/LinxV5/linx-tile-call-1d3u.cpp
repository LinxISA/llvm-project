// RUN: %clang++ --target=linx64 -mlxbc -O2 -S -o - %s | FileCheck %s --dump-input always -vv

typedef float tile tile_size(1024);

extern void __vec__ f1d3u(tile __out__ out, tile __in__ in1, tile __in__ in2,
                          tile __in__ in3);
extern void __mtc__ copyin(tile __out__ out, float *p);
extern void __mtc__ copyout(tile __in__ in, float *p);

// CHECK: VPAR       _Z5f1d3uDv1024_fS_S_S_, <M: 32, N: 32, K: 1, MR>    t#3, t#2, t#1,  ->t<4KB>
void tile_caller(float *p1, float *p2, float *p3, float *p4) {
  tile out;
  tile in1, in2, in3;
  __linx_vcall_par(copyin, 32, 32, 1, in1, p1);
  __linx_vcall_par(copyin, 32, 32, 1, in2, p2);
  __linx_vcall_par(copyin, 32, 32, 1, in3, p3);
  __linx_vcall_par(f1d3u, 32, 32, 1, out, in1, in2, in3);
  __linx_vcall_par(copyout, 32, 32, 1, out, p4);
}
