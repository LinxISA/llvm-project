//==--- BuiltinsLinxISA.td - LinxISA Builtin function database --*- tablegen -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the LinxISA-specific builtin function database.
//
//===----------------------------------------------------------------------===//

include "clang/Basic/BuiltinsBase.td"

class LinxISABuiltin<string prototype, string features = ""> : TargetBuiltin {
  let Spellings = ["__builtin_linx_" # NAME];
  let Prototype = prototype;
  let Features = features;
}

// Tile (TAU) bring-up: 4 KiB tile registers as _Vector<1024, int>.
let Attributes = [NoThrow] in {
  def tma_tload : LinxISABuiltin<"_Vector<1024, int>(void const *, _Constant unsigned int)">;
  def tma_tstore : LinxISABuiltin<"void(void *, _Vector<1024, int>, _Constant unsigned int)">;
  def tma_tload_desc : LinxISABuiltin<"_Vector<1024, int>(void const *, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int)">;
  def tma_tstore_desc : LinxISABuiltin<"void(void *, _Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int)">;
  def cube_mamulb : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int)">;
  def cube_mamulb_acc : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int)">;
  def vblock_launch : LinxISABuiltin<"void(_Constant unsigned int, void const *, _Constant unsigned long long int, _Constant unsigned long long int, _Constant unsigned long long int, _Constant unsigned int)">;

  // Tile vector elementwise ops (VPAR bring-up).
  def vpar_tadd : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int)">;
  def vpar_tsub : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int)">;
}
