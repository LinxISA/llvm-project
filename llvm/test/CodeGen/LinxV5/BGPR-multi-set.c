// RUN: clang %s --target=linx64v5-unknown-linux-musl -S -O2 -mllvm -linxv5-disable-proepi-memblock=true

double a[8];
b, c, h, i, j, k, m, n, r, t, w, v;
d[], e[], f[], g[], o[], p[], q[];
double l, s, u;
z() {
  double y[128];
  double x[w];
  aa(x);
  y[0] = b;
  v = 0;
  for (; v < 9; v++)
    y[v + 1] = 0.0;
  v = 0;
  for (; v < 3; v++) {
    y[1] += y[10 + v] = d[v];
    y[5] += y[4 + 9 * v] = e[v];
    y[7] += y[15 + 9 * v] = f[v];
    y[16 + v] = g[v];
  }
  y[37] = c;
  y[38] = h;
  y[39] = i;
  y[41] = j;
  y[42] = k = l;
  v = 0;
  for (; v < 3; v++)
    y[74] += y[83 + 9 * v] = o[5] += y[84 + 9 * v] = p[6] += y[85 + v] = q[v];
  m = n = r = s;
  t = u;
  v = 0;
  for (; v < 128; v++)
    a[v] = y[v];
}
