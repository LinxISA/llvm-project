// PTO 0.58.1: deleted/reserved Tile selectors must not assemble as accepted
// mnemonics; their raw TEPL numeric forms may still assemble (the printer
// must not emit the retired name). Work Package C2.
// RUN: not llvm-mc %s --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s --check-prefix=NEG
// RUN: grep -E '^BSTART\.TEPL [0-9]+' %s | llvm-mc --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s --check-prefix=POS

// NEG: error: Match Instruction Error!
BSTART.TEPL TFMOD, FP32
// NEG: error: Match Instruction Error!
BSTART.TEPL TPRELU, FP32
// NEG: error: Match Instruction Error!
BSTART.TEPL TADDC, FP32
// NEG: error: Match Instruction Error!
BSTART.TEPL TSUBSC, FP32

// POS: BSTART.TEPL 5, FP32
BSTART.TEPL 5, FP32
// POS: BSTART.TEPL 14, FP32
BSTART.TEPL 14, FP32