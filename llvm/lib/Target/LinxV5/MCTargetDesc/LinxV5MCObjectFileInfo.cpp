//===-- LinxV5MCObjectFileInfo.cpp - LinxV5 object file properties -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the LinxV5MCObjectFileInfo properties.
//
//===----------------------------------------------------------------------===//

#include "LinxV5MCObjectFileInfo.h"
#include "LinxV5MCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

unsigned LinxV5MCObjectFileInfo::getTextSectionAlignment() const { return 2; }
