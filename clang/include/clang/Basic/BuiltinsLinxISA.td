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

// PTO/Linx 0.58 tile bridge: 4 KiB frontend values as _Vector<1024, int>.
let Attributes = [NoThrow] in {
  // TLSU transfers and Local-to-Local movement.
  def tile_tload : LinxISABuiltin<"_Vector<1024, int>(void const *, _Constant unsigned int, _Constant unsigned int, _Constant long long int, long long int, long long int, long long int, long long int)">;
  def tile_tstore : LinxISABuiltin<"void(void *, _Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant long long int, long long int, long long int, long long int, long long int)">;
  def tile_tmov : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant long long int, _Constant unsigned int)">;

  // VEC/SFU operations use the stable TEPL Mode/Function encoding carrier.
  // TileOp selects the architectural mnemonic; it is not an execution engine.
  def tileop_unary : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, long long int, long long int, long long int)">;
  def tileop_binary : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, long long int, long long int, long long int)">;
  def tileop_binary_scalar : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, long long int, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, long long int, long long int, long long int)">;
  def tileop_splat : LinxISABuiltin<"_Vector<1024, int>(long long int, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int, long long int, long long int, long long int)">;

  // Core cube/vblock builtins.
  def cube_tmatmul : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int)">;
  def cube_tmatmul_acc : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int, _Constant unsigned int, _Constant unsigned int)">;
  def vblock_launch : LinxISABuiltin<"void(_Constant unsigned int, void const *, _Constant unsigned long long int, _Constant unsigned long long int, _Constant unsigned long long int, _Constant unsigned int)">;

  // Tile vector elementwise ops (VPAR bring-up).
  def vpar_tadd : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int)">;
  def vpar_tsub : LinxISABuiltin<"_Vector<1024, int>(_Vector<1024, int>, _Vector<1024, int>, _Constant unsigned int)">;
}
