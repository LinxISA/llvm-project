# RUN: not llvm-mc -triple=linx64v5 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

# Only the existing public Shared TMOV mode spellings are Function 2
# aliases. Direction/mode inventions and unknown MSCATTER selectors stay closed.
BSTART.TLSU TMOV.L2S.BROADCAST, FP32
# CHECK: error: TileOP 'tmov.l2s.broadcast' Not supported yet, please use numeric codes for entry.
BSTART.TLSU TMOV.S2L.INSERT, FP32
# CHECK: error: TileOP 'tmov.s2l.insert' Not supported yet, please use numeric codes for entry.
BSTART.TLSU MSCATTER.MAXIMUM, U32
# CHECK: error: TileOP 'mscatter.maximum' Not supported yet, please use numeric codes for entry.
BSTART.TLSU MSCATTER.MINIMUM, U32
# CHECK: error: TileOP 'mscatter.minimum' Not supported yet, please use numeric codes for entry.
BSTART.TLSU MSCATTER.POPCOUNT, U32
# CHECK: error: TileOP 'mscatter.popcount' Not supported yet, please use numeric codes for entry.
BSTART.TLSU TSTORE.PART, U32
# CHECK: error: TileOP 'tstore.part' Not supported yet, please use numeric codes for entry.

# Function 14 belongs to MGATHER.INC; it cannot alias a Shared store.
BSTART.TLSU TSTORE.SPART, U32
# CHECK: error: TileOP 'tstore.spart' Not supported yet, please use numeric codes for entry.
