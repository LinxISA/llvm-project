LinxV5 scalar PTO instructions
=============================

Scalar floating-point profiles
------------------------------

The non-conversion PTO FSU instructions accept FP32 and FP64 carriers.
The ``.fh`` and ``.fb`` arithmetic, comparison, min/max and unary encodings
are reserved. Half precision carriers remain legal for the scalar ABI,
loads/stores and the assigned conversion instructions. Native scalar half
arithmetic widens operands to double precision and explicitly rounds each
result back to half. Half comparisons widen to single precision; negation
flips the half sign bit without arithmetic.

The following target intrinsics execute the PTO numeric profile directly::

  declare float @llvm.linx.scalar.fsqrt.f32(float)
  declare double @llvm.linx.scalar.fsqrt.f64(double)
  declare float @llvm.linx.scalar.fexp.f32(float)
  declare double @llvm.linx.scalar.fexp.f64(double)

These intrinsics require native scalar code generation with floating-point
support. Other overloads are unsupported. They use the active PTO rounding
mode and update sticky numeric status even when their result is unused.
They are side-effecting, convergent and not duplicable.

These are distinct from generic ``llvm.sqrt`` and ``llvm.exp``, which retain
the scalar library-call fallback. In particular, the PTO v0 FEXP reference
profile specifies a fixed 18-term Taylor algorithm, so it cannot implement
the generic LLVM exponential contract by an unconditional instruction
selection pattern. This does not enable constrained LLVM floating-point
intrinsics to use that profile automatically.

Trap fallback
-------------

``llvm.trap`` expands to LLVM's ``abort()`` runtime-call fallback. The
runtime must supply ``abort`` with its non-returning termination contract.
``llvm.debugtrap`` uses the generic trap fallback as well. Neither lowers
to BWT, which publishes a nonblocking wait-timeout request. The dedicated
``llvm.linx.scalar.bwt`` intrinsic continues to select BWT in a SYS block.
The old misleading ``trap`` assembly alias is not accepted.

Assembly operands
-----------------

``fence.d pred, succ`` accepts explicit integer masks in the range 0..15
and prints those numeric masks canonically, including zero. The optional
legacy letters ``r``, ``w``, ``o`` and ``i`` correspond to mask bits 0, 1,
2 and 3 respectively. ACRE accepts only request selectors 0 and 1;
selectors 2..15 are reserved and do not disassemble as ACRE.

A numeric ``c.setret imm, ->ra`` operand is an unsigned five-bit halfword
count, 0..31. Its target is the instruction address plus ``imm * 2``.
For a symbolic operand, the relocation carries the byte address difference
and the linker checks alignment/range and scales it once into the same
five-bit field. A label addend is also expressed in bytes. Disassembly
prints the raw halfword count and can annotate the computed target symbol.
