//===-- LinxV5SearchableTables.td - ------------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

class SourceOfDivergence<Intrinsic intr> {
  Intrinsic Intr = intr;
}

def SourcesOfDivergence : GenericTable {
  let FilterClass = "SourceOfDivergence";
  let Fields = ["Intr"];

  let PrimaryKey = ["Intr"];
  let PrimaryKeyName = "lookupSourceOfDivergence";
}

def : SourceOfDivergence<int_blkv_get_index_x>;
def : SourceOfDivergence<int_blkv_get_index_y>;
def : SourceOfDivergence<int_blkv_get_index_z>;

// The dummy boolean output is divergent from the IR's perspective,
// but the mask results are uniform. These produce a divergent and
// uniform result, so the returned struct is collectively divergent.
// isAlwaysUniform can override the extract of the uniform component.
// For Linx, we just treat them as uniform
// def : SourceOfDivergence<int_blkv_if>;
// def : SourceOfDivergence<int_blkv_flow>;
// def : SourceOfDivergence<int_blkv_loop>;

   def : SourceOfDivergence<int_blkv_merge_cf>;
