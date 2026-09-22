# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s
# RUN: %python %S/../../../utils/linxv5/roundtrip_tile_macro_forms.py llvm-mc llvm-objdump %s

# Every additive B.IOT reuse opcode must remain eligible for macro folding,
# and the folded spelling must preserve the independent source decisions.
TEXP <Row=32, Col=1, FP32>, T#1.reuse, ->T<128B>
TADD <Row=32, Col=1, FP32>, T#1.reuse, U#1, ->T<128B>
TADD <Row=32, Col=1, FP32>, T#1, U#1.reuse, ->T<128B>
TADD <Row=32, Col=1, FP32>, T#1.reuse, U#1.reuse, ->T<128B>
TSTORE <Row=32, Col=1, FP32>, T#1.reuse, [base=a0, stride=a1]
MSCATTER_ADD <ValidRow=32, ValidCol=8, FP32>, [base=a0], T#1.reuse, U#1
MSCATTER_ADD <ValidRow=32, ValidCol=8, FP32>, [base=a0], T#1, U#1.reuse
MSCATTER_ADD <ValidRow=32, ValidCol=8, FP32>, [base=a0], T#1.reuse, U#1.reuse

# CHECK: TEXP{{ +}}<Row=32, Col=1, FP32>, T#1.reuse, ->T<128B>
# CHECK: TADD{{ +}}<Row=32, Col=1, FP32>, T#1.reuse, U#1, ->T<128B>
# CHECK: TADD{{ +}}<Row=32, Col=1, FP32>, T#1, U#1.reuse, ->T<128B>
# CHECK: TADD{{ +}}<Row=32, Col=1, FP32>, T#1.reuse, U#1.reuse, ->T<128B>
# CHECK: TSTORE{{ +}}<FP32>, T#1.reuse, [base=a0, stride=a1]
# CHECK: MSCATTER_ADD{{ +}}<ValidRow=32, ValidCol=8, FP32>, [base=a0], T#1.reuse, U#1
# CHECK: MSCATTER_ADD{{ +}}<ValidRow=32, ValidCol=8, FP32>, [base=a0], T#1, U#1.reuse
# CHECK: MSCATTER_ADD{{ +}}<ValidRow=32, ValidCol=8, FP32>, [base=a0], T#1.reuse, U#1.reuse
