# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o > %t.diss
# RUN: FileCheck %s < %t.diss
# RUN: %python %S/../../../utils/linxv5/verify_tile_macro_forms.py %s %t.diss
# RUN: %python %S/../../../utils/linxv5/roundtrip_tile_macro_forms.py llvm-mc llvm-objdump %s
# RUN: %python %S/../../../utils/linxv5/verify_tile_macro_required_operands.py llvm-mc %s
.text
# FORM: fold=1 spelling=GMOV operation=GMOV
# CHECK: GMOV{{ +}}<
GMOV <FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER operation=MGATHER
# CHECK: MGATHER{{ +}}<
MGATHER <Row=32, Col=1, FP32>, [a0], a0, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_ADD operation=MGATHER_ADD
# CHECK: MGATHER_ADD{{ +}}<
MGATHER_ADD <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_AND operation=MGATHER_AND
# CHECK: MGATHER_AND{{ +}}<
MGATHER_AND <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_CAS operation=MGATHER_CAS
# CHECK: MGATHER_CAS{{ +}}<
MGATHER_CAS <FP32>, [a0], a1, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_DEC operation=MGATHER_DEC
# CHECK: MGATHER_DEC{{ +}}<
MGATHER_DEC <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_EXCH operation=MGATHER_EXCH
# CHECK: MGATHER_EXCH{{ +}}<
MGATHER_EXCH <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_INC operation=MGATHER_INC
# CHECK: MGATHER_INC{{ +}}<
MGATHER_INC <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_MASK operation=MGATHER_MASK
# CHECK: MGATHER_MASK{{ +}}<
MGATHER_MASK <Row=32, Col=1, FP32>, [a0], a2, T#1, U#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_MAX operation=MGATHER_MAX
# CHECK: MGATHER_MAX{{ +}}<
MGATHER_MAX <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_MIN operation=MGATHER_MIN
# CHECK: MGATHER_MIN{{ +}}<
MGATHER_MIN <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_OR operation=MGATHER_OR
# CHECK: MGATHER_OR{{ +}}<
MGATHER_OR <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=MGATHER_XOR operation=MGATHER_XOR
# CHECK: MGATHER_XOR{{ +}}<
MGATHER_XOR <FP32>, [a0], T#1, T#1, ->T<128B>
# FORM: fold=0 spelling=MSCATTER operation=MSCATTER
MSCATTER <Row=32, Col=1, FP32>, [a0], a3, T#1, T#1
# FORM: fold=1 spelling=MSCATTER_ADD operation=MSCATTER_ADD
# CHECK: MSCATTER_ADD{{ +}}<
MSCATTER_ADD <FP32>, [a0], T#1, T#1
# FORM: fold=1 spelling=MSCATTER_AND operation=MSCATTER_AND
# CHECK: MSCATTER_AND{{ +}}<
MSCATTER_AND <FP32>, [a0], T#1, T#1
# FORM: fold=1 spelling=MSCATTER_DEC operation=MSCATTER_DEC
# CHECK: MSCATTER_DEC{{ +}}<
MSCATTER_DEC <FP32>, [a0], T#1, T#1
# FORM: fold=1 spelling=MSCATTER_INC operation=MSCATTER_INC
# CHECK: MSCATTER_INC{{ +}}<
MSCATTER_INC <FP32>, [a0], T#1, T#1
# FORM: fold=0 spelling=MSCATTER_MASK operation=MSCATTER_MASK
MSCATTER_MASK <Row=32, Col=1, FP32>, [a0], a4, T#1, T#1, U#1
# FORM: fold=1 spelling=MSCATTER_MAX operation=MSCATTER_MAX
# CHECK: MSCATTER_MAX{{ +}}<
MSCATTER_MAX <FP32>, [a0], T#1, T#1
# FORM: fold=1 spelling=MSCATTER_MIN operation=MSCATTER_MIN
# CHECK: MSCATTER_MIN{{ +}}<
MSCATTER_MIN <FP32>, [a0], T#1, T#1
# FORM: fold=1 spelling=MSCATTER_OR operation=MSCATTER_OR
# CHECK: MSCATTER_OR{{ +}}<
MSCATTER_OR <FP32>, [a0], T#1, T#1
# FORM: fold=1 spelling=MSCATTER_POPC operation=MSCATTER_POPC
# CHECK: MSCATTER_POPC{{ +}}<
MSCATTER_POPC <ValidCol=1, FP32>, [a0], T#1
# FORM: fold=1 spelling=MSCATTER_XOR operation=MSCATTER_XOR
# CHECK: MSCATTER_XOR{{ +}}<
MSCATTER_XOR <FP32>, [a0], T#1, T#1
# FORM: fold=1 spelling=TABS operation=TABS
# CHECK: TABS{{ +}}<
TABS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TADD operation=TADD
# CHECK: TADD{{ +}}<
TADD <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TADDS operation=TADDS
# CHECK: TADDS{{ +}}<
TADDS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TAND operation=TAND
# CHECK: TAND{{ +}}<
TAND <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TANDS operation=TANDS
# CHECK: TANDS{{ +}}<
TANDS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TCI operation=TCI
# CHECK: TCI{{ +}}<
TCI <Row=32, Col=1, FP32>, ->T<128B>
# FORM: fold=0 spelling=TCMP operation=TCMP
TCMP <Row=32, Col=1, FP32>, T#1, T#1, ->U<128B>
# FORM: fold=0 spelling=TCMP operation=TCMP
TCMP <Row=32, Col=1, FP32>, T#1, T#1, ->M<128B>
# FORM: fold=0 spelling=TCMP operation=TCMP
TCMP <Row=32, Col=1, FP32>, T#1, T#1, ->a5
# FORM: fold=0 spelling=TCMPS operation=TCMPS
TCMPS <Row=32, Col=1, FP32>, T#1, ->U<128B>
# FORM: fold=0 spelling=TCMPS operation=TCMPS
TCMPS <Row=32, Col=1, FP32>, T#1, ->M<128B>
# FORM: fold=0 spelling=TCMPS operation=TCMPS
TCMPS <Row=32, Col=1, FP32>, T#1, ->a6
# FORM: fold=1 spelling=TCOLARGMAX operation=TCOLARGMAX
# CHECK: TCOLARGMAX{{ +}}<
TCOLARGMAX <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLARGMIN operation=TCOLARGMIN
# CHECK: TCOLARGMIN{{ +}}<
TCOLARGMIN <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLEXPAND operation=TCOLEXPAND
# CHECK: TCOLEXPAND{{ +}}<
TCOLEXPAND <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLEXPANDADD operation=TCOLEXPANDADD
# CHECK: TCOLEXPANDADD{{ +}}<
TCOLEXPANDADD <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLEXPANDDIV operation=TCOLEXPANDDIV
# CHECK: TCOLEXPANDDIV{{ +}}<
TCOLEXPANDDIV <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLEXPANDEXPDIF operation=TCOLEXPANDEXPDIF
# CHECK: TCOLEXPANDEXPDIF{{ +}}<
TCOLEXPANDEXPDIF <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLEXPANDMAX operation=TCOLEXPANDMAX
# CHECK: TCOLEXPANDMAX{{ +}}<
TCOLEXPANDMAX <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLEXPANDMIN operation=TCOLEXPANDMIN
# CHECK: TCOLEXPANDMIN{{ +}}<
TCOLEXPANDMIN <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLEXPANDMUL operation=TCOLEXPANDMUL
# CHECK: TCOLEXPANDMUL{{ +}}<
TCOLEXPANDMUL <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLEXPANDSUB operation=TCOLEXPANDSUB
# CHECK: TCOLEXPANDSUB{{ +}}<
TCOLEXPANDSUB <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLMAX operation=TCOLMAX
# CHECK: TCOLMAX{{ +}}<
TCOLMAX <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLMIN operation=TCOLMIN
# CHECK: TCOLMIN{{ +}}<
TCOLMIN <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLPROD operation=TCOLPROD
# CHECK: TCOLPROD{{ +}}<
TCOLPROD <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TCOLSUM operation=TCOLSUM
# CHECK: TCOLSUM{{ +}}<
TCOLSUM <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=0 spelling=TCVT operation=TCVT
TCVT <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TDIV operation=TDIV
# CHECK: TDIV{{ +}}<
TDIV <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TDIVS operation=TDIVS
# CHECK: TDIVS{{ +}}<
TDIVS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TEXP operation=TEXP
# CHECK: TEXP{{ +}}<
TEXP <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TEXPANDS operation=TEXPANDS
# CHECK: TEXPANDS{{ +}}<
TEXPANDS <Row=32, Col=1, FP32>, ->T<128B>
# FORM: fold=1 spelling=TFMA operation=TFMA
# CHECK: TFMA{{ +}}<
TFMA <Row=32, Col=1, FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TGATHER operation=TGATHER
# CHECK: TGATHER{{ +}}<
TGATHER <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TGEMV operation=TGEMV
# CHECK: TGEMV{{ +}}<
TGEMV <FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TGEMV_ACC operation=TGEMV_ACC
# CHECK: TGEMV_ACC{{ +}}<
TGEMV_ACC <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TGEMV_BIAS operation=TGEMV_BIAS
# CHECK: TGEMV_BIAS{{ +}}<
TGEMV_BIAS <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TGEMV_MX operation=TGEMV_MX
# CHECK: TGEMV_MX{{ +}}<
TGEMV_MX <FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TGEMV_MX_ACC operation=TGEMV_MX_ACC
# CHECK: TGEMV_MX_ACC{{ +}}<
TGEMV_MX_ACC <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TGEMV_MX_BIAS operation=TGEMV_MX_BIAS
# CHECK: TGEMV_MX_BIAS{{ +}}<
TGEMV_MX_BIAS <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TGPR2T operation=TGPR2T
# CHECK: TGPR2T{{ +}}<
TGPR2T <Row=32, Col=4, U8>, a7, a0, a1, a2, ->T<128B>
# FORM: fold=1 spelling=TLOAD operation=TLOAD
# CHECK: TLOAD{{ +}}<
TLOAD <Row=32, Col=1, FP32>, ->T<128B>
# FORM: fold=1 spelling=TLOAD operation=TLOAD
# CHECK: TLOAD{{ +}}<
TLOAD <Row=32, Col=1, FP32>, ->S0<128B>
# FORM: fold=1 spelling=TLOAD operation=TLOAD
# CHECK: TLOAD{{ +}}<
TLOAD <Row=1, Col=32, FP32, ND2M32>, ->T<128B>
# FORM: fold=1 spelling=TLOAD operation=TLOAD
# CHECK: TLOAD{{ +}}<
TLOAD <ValidK=16, ValidN=8, TotalK=32, FP32, OHWI2NK>, [a0, a1, a2], ->S1<128B>
# FORM: fold=1 spelling=TLOG operation=TLOG
# CHECK: TLOG{{ +}}<
TLOG <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL operation=TMATMUL
# CHECK: TMATMUL{{ +}}<
TMATMUL <FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL operation=TMATMUL
# CHECK: TMATMUL{{ +}}<
TMATMUL <FP32>, T#1, S2, ->T<128B>
# FORM: fold=1 spelling=TMATMUL operation=TMATMUL
# CHECK: TMATMUL{{ +}}<
TMATMUL <FP32>, S3, S4, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_ACC operation=TMATMUL_ACC
# CHECK: TMATMUL_ACC{{ +}}<
TMATMUL_ACC <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_ACC operation=TMATMUL_ACC
# CHECK: TMATMUL_ACC{{ +}}<
TMATMUL_ACC <FP32>, T#1, T#1, S5, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_ACC operation=TMATMUL_ACC
# CHECK: TMATMUL_ACC{{ +}}<
TMATMUL_ACC <FP32>, T#1, S6, S7, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_BIAS operation=TMATMUL_BIAS
# CHECK: TMATMUL_BIAS{{ +}}<
TMATMUL_BIAS <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_BIAS operation=TMATMUL_BIAS
# CHECK: TMATMUL_BIAS{{ +}}<
TMATMUL_BIAS <FP32>, T#1, S0, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_BIAS operation=TMATMUL_BIAS
# CHECK: TMATMUL_BIAS{{ +}}<
TMATMUL_BIAS <FP32>, S1, S2, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX operation=TMATMUL_MX
# CHECK: TMATMUL_MX{{ +}}<
TMATMUL_MX <FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX operation=TMATMUL_MX
# CHECK: TMATMUL_MX{{ +}}<
TMATMUL_MX <FP32>, T#1, S3, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX operation=TMATMUL_MX
# CHECK: TMATMUL_MX{{ +}}<
TMATMUL_MX <FP32>, S4, S5, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX_ACC operation=TMATMUL_MX_ACC
# CHECK: TMATMUL_MX_ACC{{ +}}<
TMATMUL_MX_ACC <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX_ACC operation=TMATMUL_MX_ACC
# CHECK: TMATMUL_MX_ACC{{ +}}<
TMATMUL_MX_ACC <FP32>, T#1, T#1, S6, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX_ACC operation=TMATMUL_MX_ACC
# CHECK: TMATMUL_MX_ACC{{ +}}<
TMATMUL_MX_ACC <FP32>, T#1, S7, S0, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX_BIAS operation=TMATMUL_MX_BIAS
# CHECK: TMATMUL_MX_BIAS{{ +}}<
TMATMUL_MX_BIAS <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX_BIAS operation=TMATMUL_MX_BIAS
# CHECK: TMATMUL_MX_BIAS{{ +}}<
TMATMUL_MX_BIAS <FP32>, T#1, S1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMATMUL_MX_BIAS operation=TMATMUL_MX_BIAS
# CHECK: TMATMUL_MX_BIAS{{ +}}<
TMATMUL_MX_BIAS <FP32>, S2, S3, T#1, ->T<128B>
# FORM: fold=1 spelling=TMAX operation=TMAX
# CHECK: TMAX{{ +}}<
TMAX <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMAXS operation=TMAXS
# CHECK: TMAXS{{ +}}<
TMAXS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TMIN operation=TMIN
# CHECK: TMIN{{ +}}<
TMIN <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMINS operation=TMINS
# CHECK: TMINS{{ +}}<
TMINS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TMOV operation=TMOV
# CHECK: TMOV{{ +}}<
TMOV <FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TMUL operation=TMUL
# CHECK: TMUL{{ +}}<
TMUL <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TMULS operation=TMULS
# CHECK: TMULS{{ +}}<
TMULS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TNEG operation=TNEG
# CHECK: TNEG{{ +}}<
TNEG <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TNOT operation=TNOT
# CHECK: TNOT{{ +}}<
TNOT <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TOR operation=TOR
# CHECK: TOR{{ +}}<
TOR <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TORS operation=TORS
# CHECK: TORS{{ +}}<
TORS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TPACK operation=TPACK
# CHECK: TPACK{{ +}}<
TPACK <U32>, T#1, T#1, a3, ->T<128B>
# FORM: fold=1 spelling=TPERMUTE operation=TPERMUTE
# CHECK: TPERMUTE{{ +}}<
TPERMUTE <FP32>, T#1, T#1, T#1, ->T<128B>
# FORM: fold=0 spelling=TPREFETCH operation=TPREFETCH
TPREFETCH <Row=32, Col=1, FP32>
# FORM: fold=1 spelling=TRECIP operation=TRECIP
# CHECK: TRECIP{{ +}}<
TRECIP <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TRELU operation=TRELU
# CHECK: TRELU{{ +}}<
TRELU <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TREM operation=TREM
# CHECK: TREM{{ +}}<
TREM <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TREMS operation=TREMS
# CHECK: TREMS{{ +}}<
TREMS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWARGMAX operation=TROWARGMAX
# CHECK: TROWARGMAX{{ +}}<
TROWARGMAX <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWARGMIN operation=TROWARGMIN
# CHECK: TROWARGMIN{{ +}}<
TROWARGMIN <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWEXPAND operation=TROWEXPAND
# CHECK: TROWEXPAND{{ +}}<
TROWEXPAND <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWEXPANDADD operation=TROWEXPANDADD
# CHECK: TROWEXPANDADD{{ +}}<
TROWEXPANDADD <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWEXPANDDIV operation=TROWEXPANDDIV
# CHECK: TROWEXPANDDIV{{ +}}<
TROWEXPANDDIV <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWEXPANDEXPDIF operation=TROWEXPANDEXPDIF
# CHECK: TROWEXPANDEXPDIF{{ +}}<
TROWEXPANDEXPDIF <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWEXPANDMAX operation=TROWEXPANDMAX
# CHECK: TROWEXPANDMAX{{ +}}<
TROWEXPANDMAX <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWEXPANDMIN operation=TROWEXPANDMIN
# CHECK: TROWEXPANDMIN{{ +}}<
TROWEXPANDMIN <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWEXPANDMUL operation=TROWEXPANDMUL
# CHECK: TROWEXPANDMUL{{ +}}<
TROWEXPANDMUL <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWEXPANDSUB operation=TROWEXPANDSUB
# CHECK: TROWEXPANDSUB{{ +}}<
TROWEXPANDSUB <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWMAX operation=TROWMAX
# CHECK: TROWMAX{{ +}}<
TROWMAX <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWMIN operation=TROWMIN
# CHECK: TROWMIN{{ +}}<
TROWMIN <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWPROD operation=TROWPROD
# CHECK: TROWPROD{{ +}}<
TROWPROD <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TROWSUM operation=TROWSUM
# CHECK: TROWSUM{{ +}}<
TROWSUM <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TRSQRT operation=TRSQRT
# CHECK: TRSQRT{{ +}}<
TRSQRT <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TSCATTER operation=TSCATTER
# CHECK: TSCATTER{{ +}}<
TSCATTER <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=0 spelling=TSEL operation=TSEL
TSEL <Row=32, Col=1, FP32>, U#1, T#1, T#1, ->T<128B>
# FORM: fold=0 spelling=TSEL operation=TSEL
TSEL <Row=32, Col=1, FP32>, M#1, T#1, T#1, ->T<128B>
# FORM: fold=0 spelling=TSEL operation=TSEL
TSEL <Row=32, Col=1, FP32>, a4, T#1, T#1, ->T<128B>
# FORM: fold=0 spelling=TSELS operation=TSELS
TSELS <Row=32, Col=1, FP32>, U#1, T#1, ->T<128B>
# FORM: fold=0 spelling=TSELS operation=TSELS
TSELS <Row=32, Col=1, FP32>, M#1, T#1, ->T<128B>
# FORM: fold=0 spelling=TSELS operation=TSELS
TSELS <Row=32, Col=1, FP32>, a5, T#1, ->T<128B>
# FORM: fold=1 spelling=TSHL operation=TSHL
# CHECK: TSHL{{ +}}<
TSHL <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TSHLS operation=TSHLS
# CHECK: TSHLS{{ +}}<
TSHLS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TSHR operation=TSHR
# CHECK: TSHR{{ +}}<
TSHR <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TSHRS operation=TSHRS
# CHECK: TSHRS{{ +}}<
TSHRS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TSHUF operation=TSHUF
# CHECK: TSHUF{{ +}}<
TSHUF <FP32>, T#1, T#1, a6, ->T<128B>
# FORM: fold=1 spelling=TSQRT operation=TSQRT
# CHECK: TSQRT{{ +}}<
TSQRT <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=0 spelling=TSTORE operation=TSTORE
TSTORE <Row=32, Col=1, FP32>, T#1
# FORM: fold=0 spelling=TSTORE operation=TSTORE
TSTORE <Row=32, Col=1, FP32>, S4
# FORM: fold=1 spelling=TSTORE operation=TSTORE
# CHECK: TSTORE{{ +}}<
TSTORE <Row=1, Col=32, FP32, M322ND>, T#1
# FORM: fold=1 spelling=TSUB operation=TSUB
# CHECK: TSUB{{ +}}<
TSUB <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TSUBS operation=TSUBS
# CHECK: TSUBS{{ +}}<
TSUBS <Row=32, Col=1, FP32>, T#1, ->T<128B>
# FORM: fold=1 spelling=TTRI operation=TTRI
# CHECK: TTRI{{ +}}<
TTRI <Row=32, Col=1, FP32>, ->T<128B>
# FORM: fold=1 spelling=TUNPACK operation=TUNPACK
# CHECK: TUNPACK{{ +}}<
TUNPACK <U32>, T#1, a7, ->T<128B>
# FORM: fold=1 spelling=TXOR operation=TXOR
# CHECK: TXOR{{ +}}<
TXOR <Row=32, Col=1, FP32>, T#1, T#1, ->T<128B>
# FORM: fold=1 spelling=TXORS operation=TXORS
# CHECK: TXORS{{ +}}<
TXORS <Row=32, Col=1, FP32>, T#1, ->T<128B>
