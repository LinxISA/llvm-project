# RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s

# Lifetime applies only to a Shared source.
B.IOS mask=1111, ->S3.reuse<512B>

# CHECK: error: Match Instruction Error!
