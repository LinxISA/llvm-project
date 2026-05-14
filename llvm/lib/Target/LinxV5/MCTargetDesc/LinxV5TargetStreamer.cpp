//===-- LinxV5TargetStreamer.cpp - LinxV5 Target Streamer Methods --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides LinxV5 specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "LinxV5TargetStreamer.h"
#include "LinxV5MCTargetDesc.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/LinxV5Attributes.h"
#include "llvm/Support/LinxV5ISAInfo.h"

using namespace llvm;

LinxV5TargetStreamer::LinxV5TargetStreamer(MCStreamer &S)
    : MCTargetStreamer(S) {}

void LinxV5TargetStreamer::finish() { finishAttributeSection(); }
void LinxV5TargetStreamer::reset() {}
void LinxV5TargetStreamer::emitDirectiveOptionRelax() {}
void LinxV5TargetStreamer::emitDirectiveOptionNoRelax() {}
void LinxV5TargetStreamer::emitAttribute(unsigned Attribute, unsigned Value) {}
void LinxV5TargetStreamer::finishAttributeSection() {}
void LinxV5TargetStreamer::emitTextAttribute(unsigned Attribute,
                                             StringRef String) {}
void LinxV5TargetStreamer::emitRawText(StringRef Str) {}

void LinxV5TargetStreamer::setTargetABI(LinxV5ABI::ABI ABI) {
  assert(ABI != LinxV5ABI::ABI_Unknown && "Improperly initialized target ABI");
  TargetABI = ABI;
}

// This part is for ascii assembly output
LinxV5TargetAsmStreamer::LinxV5TargetAsmStreamer(MCStreamer &S,
                                                 formatted_raw_ostream &OS)
    : LinxV5TargetStreamer(S), OS(OS) {}

void LinxV5TargetAsmStreamer::emitDirectiveOptionRelax() {
  OS << "\t.option\trelax\n";
}

void LinxV5TargetAsmStreamer::emitDirectiveOptionNoRelax() {
  OS << "\t.option\tnorelax\n";
}

void LinxV5TargetAsmStreamer::emitAttribute(unsigned Attribute,
                                            unsigned Value) {
  OS << "\t.attribute\t" << Attribute << ", " << Twine(Value) << "\n";
}

void LinxV5TargetAsmStreamer::emitTextAttribute(unsigned Attribute,
                                                StringRef String) {
  OS << "\t.attribute\t" << Attribute << ", \"" << String << "\"\n";
}

void LinxV5TargetAsmStreamer::emitRawText(StringRef Str) {
  OS << Str;
}

void LinxV5TargetAsmStreamer::finishAttributeSection() {}