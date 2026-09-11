//===-- LinxV5TileOpSchema.h - TileOp macro-assembly schema ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Data-only schema generated from the PTO tile-macro-assembly catalog
// (pto-spec #263). It maps single-line TileOp macro forms onto the
// already-supported physical MC layer. This header declares the query
// surface used by the AsmParser (macro -> physical expansion) and the
// Disassembler (physical bundle -> macro folding).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEOPSCHEMA_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEOPSCHEMA_H

#include <cstdint>

namespace llvm {
namespace LinxV5TileOpSchema {

// Bounds (defined in the generated .inc).
extern const unsigned OperationCount;
extern const unsigned FormCount;
extern const char CatalogSHA256[];
extern const char ArchitectureIdentity[];

enum BindingKind {
  BK_BundleConfiguration = 0,
  BK_ScalarBinding = 1,
  BK_TileSource = 2,
  BK_TileDestination = 3,
  BK_SharedTile = 4,
  BK_PredicateTileSource = 5,
  BK_PredicateTileDestination = 6,
  BK_PredicateCellSource = 7,
  BK_PredicateCellDestination = 8,
  BK_PredicateGPRSource = 9,
  BK_PredicateGPRDestination = 10,
};

enum FieldSection { FS_Config = 0, FS_Source = 1, FS_Destination = 2 };

// Find a form record by its full spelling (e.g. "TADD", "TMATMUL.SHARED_BOTH").
// Returns the form index or -1 when the mnemonic is not a TileOp macro.
int lookupFormBySpelling(const char *Spelling);
/// Iterate all form indices whose spelling matches (dotted forms share a
/// mnemonic across carrier variants). Returns count; indices are written
/// to Out (capacity >= 4).
unsigned formsWithSpelling(const char *Spelling, int *Out, unsigned Cap);


// Accessors into the generated tables (index -1 yields nullptr/0).
const char *formMnemonic(int FormIdx);
const char *formSpelling(int FormIdx);
const char *formHeaderCommand(int FormIdx);
const char *formHeaderSelector(int FormIdx);
int formHeaderFunction(int FormIdx);
bool formFoldsUniquely(int FormIdx);

// Field-slot iteration: section ranges for a form.
unsigned formSectionCount(int FormIdx, int Section);
int formSectionBegin(int FormIdx, int Section);

// Field-slot accessors.
const char *slotField(int SlotIdx);
const char *slotSyntax(int SlotIdx);
uint8_t slotBindingKind(int SlotIdx);
bool slotOptional(int SlotIdx);
const char *slotDefault(int SlotIdx);
const char *slotCondition(int SlotIdx);

// Verification entry point: called once at process start (or under
// LLVM_DEBUG) to assert schema invariants. Returns false on violation.
bool verifySchema();

} // namespace LinxV5TileOpSchema
} // namespace llvm

#endif
