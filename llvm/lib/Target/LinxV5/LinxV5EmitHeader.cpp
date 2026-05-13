// ===----------------------- LinxV5EmitHeader.cpp  ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
// add block inst info for LinxV5 Target before asm emit.
//
// ===--------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5TargetMachine.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include <queue>

using namespace llvm;

#define DEBUG_TYPE "linxv5-emit-header"
#define LINX_EMIT_HEADER "LinxV5 Emit Header"

enum KillScene {
  NONE,
  CALLSITE,
  All,
};

static cl::opt<KillScene>
    KILLInsertScene("linxv5-kill-insert", cl::init(NONE), cl::Hidden,
                    cl::desc("kill insert scene"),
                    cl::values(clEnumValN(NONE, "none", "explain..."),
                               clEnumValN(CALLSITE, "callsite", "explain..."),
                               clEnumValN(All, "all", "explain...")));

static cl::opt<bool> killExcludeCSR("linxv5-kill-exclude-csr", cl::init(true),
                                    cl::desc("kill exclude csr"), cl::Hidden);

static cl::opt<bool> killExcludeAR("linxv5-kill-exclude-argreg", cl::init(true),
                                   cl::desc("kill exclude argrument register"),
                                   cl::Hidden);

static cl::opt<unsigned> StrongLikelyWeight("linxv5-strong-likely-Weight",
                                            cl::desc("strong-likely Weight"),
                                            cl::init(999), cl::Hidden);
static cl::opt<unsigned>
    StrongUnLikelyWeight("linxv5-strong-unlikely-Weight",
                         cl::desc("strong-unlikely Weight"), cl::init(1),
                         cl::Hidden);

static bool boolVal(cl::boolOrDefault Val) { return Val == cl::BOU_TRUE; }

namespace {
class LinxV5EmitHeader : public MachineFunctionPass {
public:
  static char ID;
  LinxV5EmitHeader() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return LINX_EMIT_HEADER; }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineBranchProbabilityInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  void instIsolate(MachineFunction &MF);
  void addKillInst(MachineBasicBlock &MBB);
  void insertBlockHeader(MachineBasicBlock &MBB);
  unsigned getPEType(MachineBasicBlock &MBB);
  void emitBranchHint(MachineBasicBlock *MBB);
  void emitBlockStartWithoutTargetFall(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI);
  void emitBlockStartWithoutTargetRet(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI);
  void emitBlockStartWithoutTargetInd(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI);
  void emitBlockStartWithoutTargetIcall(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MBBI);
  void emitBlockStartDirect(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI,
                            MachineOperand &Op);
  void emitBlockStartCond(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator MBBI, MachineOperand &Op);
  void emitBlockStartCall(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator MBBI, MachineOperand &Op);
  void emitFunctionBlockHeader(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MBBI);
  void appendBstopToFunction(MachineFunction &MF);
  bool isBlockWithoutHeader(const MachineBasicBlock *MBB);
  bool isBlockWithoutHeader(MachineBasicBlock *MBB);
  bool splitMultiBranchBlock(MachineBasicBlock &MBB);

  void verify(MachineFunction &MF);

private:
  const LinxV5InstrInfo *TII;
};
} // namespace

char LinxV5EmitHeader::ID = 0;

INITIALIZE_PASS(LinxV5EmitHeader, DEBUG_TYPE, LINX_EMIT_HEADER, false, false)

static void appendAddpcFollowCall(MachineBasicBlock &MBB,
                                  const LinxV5InstrInfo *TII,
                                  MachineBasicBlock::iterator MBBI) {
  MCSymbol *LocSymbol = MBB.getParent()->getContext().createTempSymbol();
  BuildMI(MBB, MBBI, DebugLoc(), TII->get(LinxV5::ADDPC)).addSym(LocSymbol);
  BuildMI(MBB, MBB.end(), DebugLoc(), TII->get(LinxV5::PseudoLABEL))
      .add(MachineOperand::CreateMCSymbol(LocSymbol));
}

static bool isLinxV5BlockTerminator(MachineInstr *MI) {
  return MI->isTerminator() || LinxV5::isIsolateInstr(*MI);
}

template <typename T> inline T prev_linx_terminator(T It, T Begin) {
  --It;
  while (It != Begin && !isLinxV5BlockTerminator(&*It))
    --It;
  return It;
}

/// Inst isolate fixup.
/// Like branch folding pass combine A fall-through B into one block.
/// We do not want to do lots of hack at these open source pass.
/// Just do some fixup work here.
void LinxV5EmitHeader::instIsolate(MachineFunction &MF) {
  SmallVector<MachineBasicBlock *> ToVisitBBs;
  for (MachineFunction::iterator I = MF.begin(), E = MF.end(); I != E; I++)
    ToVisitBBs.push_back(&*I);
  while (!ToVisitBBs.empty()) {
    MachineBasicBlock *MBB = ToVisitBBs.pop_back_val();
    for (auto I = MBB->rbegin(), E = MBB->rend(); I != E; I++) {
      MachineInstr &MI = *I;
      if (MI.isDebugInstr() || MI.isCFIInstruction())
        continue;
      uint64_t TSFlags = MI.getDesc().TSFlags;
      if (LinxV5II::isTileOp(TSFlags) || LinxV5II::isHeaderOnly(TSFlags)) {
        if (MI.getIterator() == MBB->getLastNonDebugInstr()) {
          if (MI.getIterator() == MBB->getFirstNonDebugInstr())
            break;
          auto SplitPos = next_nodbg(I, MBB->rend());
          MBB->splitAt(*SplitPos);
        } else {
          MBB->splitAt(MI);
        }
        ToVisitBBs.push_back(MBB);
        break;
      }
    }
  }
}

void LinxV5EmitHeader::addKillInst(MachineBasicBlock &MBB) {
  if (KILLInsertScene == KillScene::NONE)
    return;

  for (MachineInstr &MI : MBB)
    if (LinxV5::isIsolateInstr(MI))
      return;

  DebugLoc DL = DebugLoc();
  MachineBasicBlock::iterator FTI = MBB.getFirstTerminator();
  if (FTI != MBB.end()) {
    MachineInstr &MI = *FTI;
    if (!MI.isCall() && KILLInsertScene == KillScene::CALLSITE)
      return;
  }

  unsigned ActiveRegBitsMap = 0x0;
  for (const auto &LI : MBB.liveins()) {
    unsigned Reg = LI.PhysReg;
    ActiveRegBitsMap |= (0b1 << (Reg - LinxV5::R0));
  }
  ActiveRegBitsMap |= (0b1 << (LinxV5::R0 - LinxV5::R0));  // zero
  ActiveRegBitsMap |= (0b1 << (LinxV5::R1 - LinxV5::R0));  // sp
  ActiveRegBitsMap |= (0b1 << (LinxV5::R10 - LinxV5::R0)); // ra
  for (MachineInstr &MI : make_range(MBB.begin(), FTI)) {
    for (MachineOperand &MO : MI.uses()) {
      if (!MO.isReg())
        continue;
      Register Reg = MO.getReg();
      if (!LinxV5::GRRegClass.contains(Reg))
        continue;
      unsigned RegMask = (0b1 << (Reg - LinxV5::R0));
      if (MO.isKill()) {
        ActiveRegBitsMap &= (RegMask ^ 0xFFFFFF);
      } else {
        if ((ActiveRegBitsMap & RegMask) == 0)
          return; // give up, cause PEI split block has bug
      }
    }
    for (MachineOperand &MO : MI.defs()) {
      if (!MO.isReg())
        continue;
      Register Reg = MO.getReg();
      if (!LinxV5::GRRegClass.contains(Reg))
        continue;
      unsigned RegMask = (0b1 << (Reg - LinxV5::R0));
      if (!MO.isDead())
        ActiveRegBitsMap |= RegMask;
    }
  }

  unsigned KillRegBitsMap = ActiveRegBitsMap ^ 0xFFFFFF;
  KillRegBitsMap &= 0b111111111111101111111100; // kill zero, sp, ra
  if (killExcludeCSR)
    KillRegBitsMap &= 0b111100000000011111111111;
  if (killExcludeAR)
    KillRegBitsMap &= 0b111111111111110000000011;
  if (KillRegBitsMap == 0x0)
    return;
  BuildMI(MBB, FTI, DL, TII->get(LinxV5::KILLInst)).addImm(KillRegBitsMap);
}

void LinxV5EmitHeader::insertBlockHeader(MachineBasicBlock &MBB) {
  // skip EHLABEL
  auto HeaderPos = MBB.SkipPHIsLabelsAndDebug(MBB.begin());
  MachineBasicBlock::iterator MBBI = MBB.getFirstTerminator();

  if (isBlockWithoutHeader(&MBB)) {
    return;
  }

  if (MBBI != MBB.end()) {
    MachineInstr &MI = *MBBI;
    switch (MI.getOpcode()) {
    case LinxV5::PseudoBR:
    case LinxV5::PseudoTAIL:
      emitBlockStartDirect(MBB, HeaderPos, MI.getOperand(0));
      MBBI->eraseFromParent();
      break;
    case LinxV5::PseudoBRCondCARG:
      emitBlockStartCond(MBB, HeaderPos, MI.getOperand(0));
      MBBI->eraseFromParent();
      break;
    case LinxV5::PseudoCALL:
      emitBlockStartCall(MBB, HeaderPos, MI.getOperand(0));
      appendAddpcFollowCall(MBB, TII, HeaderPos);
      // DO NOT erase callInst here. We will skip its print when emit asm.
      // CallInst is needed by c++-exception callsite table, which indicate
      // callsite region corresponding exception action and landing pad.
      break;
    case LinxV5::PseudoCALLIndCARG:
      emitBlockStartWithoutTargetIcall(MBB, HeaderPos);
      appendAddpcFollowCall(MBB, TII, HeaderPos);
      break;
    case LinxV5::PseudoBRIndCARG:
    case LinxV5::PseudoTAILIndCARG:
      emitBlockStartWithoutTargetInd(MBB, HeaderPos);
      MBBI->eraseFromParent();
      break;
    case LinxV5::PseudoRETCARG:
      emitBlockStartWithoutTargetRet(MBB, HeaderPos);
      MBBI->eraseFromParent();
      break;
    case LinxV5::PseudoFunctionBlock: {
      if (HeaderPos != MBBI)
        // bb.1:
        //   INSN1 ...
        //   External @simt_func
        emitBlockStartWithoutTargetFall(MBB, HeaderPos);
      // If V5 executes the emitFunctionBlockHeader function and directly
      // reports an error.
      assert(0 && "LinxV5 simt Reached unexpected code path!");
      emitFunctionBlockHeader(MBB, MBBI);
      MBBI->eraseFromParent();
      break;
    }
    default:
      llvm_unreachable("Invalid Pseudo NextType");
      break;
    }
  } else {
    emitBlockStartWithoutTargetFall(MBB, HeaderPos);
  }
}

bool LinxV5EmitHeader::isBlockWithoutHeader(MachineBasicBlock *MBB) {
  unsigned TargetInstNum = 0;
  for (MachineInstr &MI : *MBB) {
    if (MI.isDebugInstr() || MI.isCFIInstruction())
      continue;
    else if (LinxV5::isIsolateInstr(MI))
      ++TargetInstNum;
  }
  assert(TargetInstNum <= 1);
  return TargetInstNum == 1;
}

void LinxV5EmitHeader::emitBranchHint(MachineBasicBlock *MBB) {
  MachineBasicBlock::iterator HintPos;
  if (MBB->succ_size() < 2)
    return;
  const Instruction *TI = MBB->getBasicBlock()->getTerminator();
  if (!TI->getMetadata("linx_branch_hint"))
    return;
  for (auto &MI : *MBB) {
    unsigned MIFrm = LinxV5II::getFormat(MI.getDesc().TSFlags);
    if (MIFrm == LinxV5II::InstFormat_BSTART_WITH_TARGET_42 ||
        MIFrm == LinxV5II::InstFormat_BSTART_WITHOUT_TARGET_32) {
      // Check all block header types. Currently, only two block header types
      // are generated in the insertBlockHeader function.
      HintPos = std::next(MI.getIterator());
      break;
    }
  }

  auto &MBPI = getAnalysis<MachineBranchProbabilityInfo>();
  for (MachineBasicBlock *Succ : MBB->successors()) {
    if (!MBB->isLayoutSuccessor(&*Succ))
      continue;
    BranchProbability Prob = MBPI.getEdgeProbability(MBB, Succ);
    uint32_t SumWeight = StrongLikelyWeight + StrongUnLikelyWeight;
    if (Prob > BranchProbability(StrongLikelyWeight, SumWeight)) {
      BuildMI(*MBB, HintPos, DebugLoc(), TII->get(LinxV5::B_HINT_NONE_unlikely))
          .addImm(0);
    }
    if (Prob < BranchProbability(StrongUnLikelyWeight, SumWeight)) {
      BuildMI(*MBB, HintPos, DebugLoc(), TII->get(LinxV5::B_HINT_NONE_likely))
          .addImm(0);
    }
    return;
  }
}

unsigned LinxV5EmitHeader::getPEType(MachineBasicBlock &MBB) {
  unsigned Mask = LinxV5II::PET_ALL;
  for (MachineInstr &MI : MBB) {
    Mask &= LinxV5II::getPEMask(MI.getDesc().TSFlags);
    if (Mask == 0) {
      errs() << "Block PEType conflict at:\n";
      errs() << MI;
      errs() << MBB;
      report_fatal_error("Block PEType conflict!");
    }
  }
  return LinxV5II::evalPETypeFromMask(Mask);
}

// Temporarily retain, assert at the call site that linxV5 simt is not expected
// to use this function.
void LinxV5EmitHeader::emitFunctionBlockHeader(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  assert(getPEType(MBB) == LinxV5II::PET_STD ||
         getPEType(MBB) == LinxV5II::PET_FP ||
         getPEType(MBB) == LinxV5II::PET_SYS);
  BuildMI(MBB, MBBI, DL, TII->get(LinxV5::BSTART_VPAR))
      .addImm(LinxV5Op::TileOPMode::VS16);
  BuildMI(MBB, MBBI, DL, TII->get(LinxV5::BTEXT)).add(MBBI->getOperand(0));
}

void LinxV5EmitHeader::emitBlockStartDirect(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MBBI,
                                            MachineOperand &Op) {
  DebugLoc DL = MBBI->getDebugLoc();
  unsigned BlockTypeOpcode;
  if (getPEType(MBB) == LinxV5II::PET_STD)
    BlockTypeOpcode = LinxV5::BSTART_STD_WITH_TARGET_64_DIRECT;
  else if (getPEType(MBB) == LinxV5II::PET_FP)
    BlockTypeOpcode = LinxV5::BSTART_FP_WITH_TARGET_64_DIRECT;
  else if (getPEType(MBB) == LinxV5II::PET_SYS)
    BlockTypeOpcode = LinxV5::BSTART_AUX_WITH_TARGET_64_DIRECT;
  else
    assert(0 && "Unsupport Block type!");
  BuildMI(MBB, MBBI, DL, TII->get(BlockTypeOpcode)).add(Op);
}

void LinxV5EmitHeader::emitBlockStartCond(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI,
                                          MachineOperand &Op) {
  DebugLoc DL = MBBI->getDebugLoc();
  unsigned BlockTypeOpcode;
  if (getPEType(MBB) == LinxV5II::PET_STD)
    BlockTypeOpcode = LinxV5::BSTART_STD_WITH_TARGET_64_COND;
  else if (getPEType(MBB) == LinxV5II::PET_FP)
    BlockTypeOpcode = LinxV5::BSTART_FP_WITH_TARGET_64_COND;
  else if (getPEType(MBB) == LinxV5II::PET_SYS)
    BlockTypeOpcode = LinxV5::BSTART_AUX_WITH_TARGET_64_COND;
  else
    assert(0 && "Unsupport Block type!");
  BuildMI(MBB, MBBI, DL, TII->get(BlockTypeOpcode)).add(Op);
}

void LinxV5EmitHeader::emitBlockStartCall(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI,
                                          MachineOperand &Op) {
  DebugLoc DL = MBBI->getDebugLoc();
  unsigned BlockTypeOpcode;
  if (getPEType(MBB) == LinxV5II::PET_STD)
    BlockTypeOpcode = LinxV5::BSTART_STD_WITH_TARGET_64_CALL;
  else if (getPEType(MBB) == LinxV5II::PET_FP)
    BlockTypeOpcode = LinxV5::BSTART_FP_WITH_TARGET_64_CALL;
  else if (getPEType(MBB) == LinxV5II::PET_SYS)
    BlockTypeOpcode = LinxV5::BSTART_AUX_WITH_TARGET_64_CALL;
  else
    assert(0 && "Unsupport Block type!");
  BuildMI(MBB, MBBI, DL, TII->get(BlockTypeOpcode)).add(Op);
}

void LinxV5EmitHeader::emitBlockStartWithoutTargetFall(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
  if (MBBI != MBB.end()) {
    MachineInstr &MI = *MBBI;
    if (LinxV5II::isTileOp(MI.getDesc().TSFlags))
      return;
  }
  DebugLoc DL = DebugLoc();

  unsigned Opc = LinxV5::BSTART_STD_WITHOUT_TARGET_32_FALL;
  switch (getPEType(MBB)) {
  default:
    llvm_unreachable("Unsupport PE Type!");
  case LinxV5II::PET_STD:
    Opc = LinxV5::BSTART_STD_WITHOUT_TARGET_32_FALL;
    break;
  case LinxV5II::PET_FP:
    Opc = LinxV5::BSTART_FP_WITHOUT_TARGET_32_FALL;
    break;
  case LinxV5II::PET_SYS:
    Opc = LinxV5::BSTART_AUX_WITHOUT_TARGET_32_FALL;
    break;
  }

  BuildMI(MBB, MBBI, DL, TII->get(Opc));
}

void LinxV5EmitHeader::emitBlockStartWithoutTargetRet(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = DebugLoc();

  unsigned Opc = LinxV5::BSTART_STD_WITHOUT_TARGET_32_RET;
  switch (getPEType(MBB)) {
  default:
    llvm_unreachable("Unsupport PE Type!");
  case LinxV5II::PET_STD:
    Opc = LinxV5::BSTART_STD_WITHOUT_TARGET_32_RET;
    break;
  case LinxV5II::PET_FP:
    Opc = LinxV5::BSTART_FP_WITHOUT_TARGET_32_RET;
    break;
  case LinxV5II::PET_SYS:
    Opc = LinxV5::BSTART_AUX_WITHOUT_TARGET_32_RET;
    break;
  }

  BuildMI(MBB, MBBI, DL, TII->get(Opc));
}

void LinxV5EmitHeader::emitBlockStartWithoutTargetInd(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = DebugLoc();

  unsigned Opc = LinxV5::BSTART_STD_WITHOUT_TARGET_32_IND;
  switch (getPEType(MBB)) {
  default:
    llvm_unreachable("Unsupport PE Type!");
  case LinxV5II::PET_STD:
    Opc = LinxV5::BSTART_STD_WITHOUT_TARGET_32_IND;
    break;
  case LinxV5II::PET_FP:
    Opc = LinxV5::BSTART_FP_WITHOUT_TARGET_32_IND;
    break;
  case LinxV5II::PET_SYS:
    Opc = LinxV5::BSTART_AUX_WITHOUT_TARGET_32_IND;
    break;
  }

  BuildMI(MBB, MBBI, DL, TII->get(Opc));
}

void LinxV5EmitHeader::emitBlockStartWithoutTargetIcall(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = DebugLoc();

  unsigned Opc = LinxV5::BSTART_STD_WITHOUT_TARGET_32_ICALL;
  switch (getPEType(MBB)) {
  default:
    llvm_unreachable("Unsupport PE Type!");
  case LinxV5II::PET_STD:
    Opc = LinxV5::BSTART_STD_WITHOUT_TARGET_32_ICALL;
    break;
  case LinxV5II::PET_FP:
    Opc = LinxV5::BSTART_FP_WITHOUT_TARGET_32_ICALL;
    break;
  case LinxV5II::PET_SYS:
    Opc = LinxV5::BSTART_AUX_WITHOUT_TARGET_32_ICALL;
    break;
  }

  BuildMI(MBB, MBBI, DL, TII->get(Opc));
}

// each block should end with 'bstop', and 'bstart' also has the semantics
// of 'bstop'. then we can emit like:
//    bstart
//    inst0
//    bstart
//    inst0
// this saves one bstop each block. but we can quikly find the problem:
//   where is the last bstop?
// so we need append the last bstop in each function.
//
// !! A perfect 'bstop' at the end of function is a 'bstart'
void LinxV5EmitHeader::appendBstopToFunction(MachineFunction &MF) {
  emitBlockStartWithoutTargetFall(MF.back(), MF.back().end());
}

// split block if end with two or more terminators
bool LinxV5EmitHeader::splitMultiBranchBlock(MachineBasicBlock &MBB) {
  if (MBB.getParent()->getSubtarget<LinxV5Subtarget>().isSIMT())
    // simt body function do not have blocks
    return false;
  bool Changed = false;
  while (true) {
    auto Term = MBB.getLastNonDebugInstr();
    if (Term == MBB.begin() || Term == MBB.end())
      break;
    if (Term->getOpcode() == LinxV5::PseudoBR &&
        MBB.isLayoutSuccessor(Term->getOperand(0).getMBB())) {
      Term->eraseFromParent();
      Changed = true;
      continue;
    }
    auto Prev = prev_linx_terminator(Term, MBB.begin());
    if (Prev == Term)
      break;
    if (!isLinxV5BlockTerminator(&*Prev))
      break;

    LLVM_DEBUG(dbgs() << "Split " << MBB << "At " << *Prev);
    MBB.splitAt(*Prev);
    Changed = true;
  }

  // verify
  auto MBBE = prev_linx_terminator(MBB.end(), MBB.begin());
  auto MBBI = MBB.begin();
  for (; MBBI != MBBE; ++MBBI) {
    if (isLinxV5BlockTerminator(&*MBBI)) {
      dbgs() << "Block layout fatal:\n" << MBB;
      report_fatal_error("Block layout fatal!");
    }
  }
  return Changed;
}

enum {
  S_BSTART, // bstart
  S_BMOD,   // block modifier
  S_INST,   // micro-instr
  S_BSTOP,
  S_TMPL
};

const char *stateName(unsigned state) {
  switch (state) {
  case S_BSTART:
    return "BSTART";
  case S_BMOD:
    return "BlockMod";
  case S_INST:
    return "MicroInstr";
  case S_BSTOP:
    return "BSTOP";
  case S_TMPL:
    return "Template";
  default:
    return "???";
  }
}

static bool isBSTOP(unsigned Op) {
  return Op == LinxV5::BSTOP || Op == LinxV5::BSTOP_C ||
         Op == LinxV5::SIMT_BSTOP;
}

#define CHECK_STATE(cond)                                                      \
  if (!(cond)) {                                                               \
    errs() << stateName(State) << " -> " << stateName(Next) << "\n"            \
           << MI << "in\n"                                                     \
           << MBB;                                                             \
    report_fatal_error(#cond " failed");                                       \
  }

void LinxV5EmitHeader::verify(MachineFunction &MF) {
  for (auto &MBB : MF) {
    unsigned State = S_BSTOP;
    for (auto &MI : MBB) {
      if (MI.isDebugInstr() || MI.isCFIInstruction() ||
          MI.getOpcode() == LinxV5::PseudoLABEL || MI.isLabel())
        continue;

      uint64_t Flags = MI.getDesc().TSFlags;
      unsigned Next;
      if (LinxV5II::isBSTART(Flags))
        Next = S_BSTART;
      else if (LinxV5II::isBlockModifier(Flags))
        Next = S_BMOD;
      else if (isBSTOP(MI.getOpcode()))
        Next = S_BSTOP;
      else if (LinxV5::isIsolateInstr(MI))
        Next = S_TMPL;
      else if (LinxV5II::isMicroInstr(Flags))
        Next = S_INST;
      else
        llvm_unreachable("Invalid opcode in LinxV5 basic block");

      switch (State) {
      case S_BSTART:
        CHECK_STATE(Next == S_BSTART || Next == S_BMOD || Next == S_INST ||
                    Next == S_BSTOP || Next == S_TMPL);
        State = Next;
        continue;
      case S_BMOD:
        CHECK_STATE(Next == S_BSTART || Next == S_BMOD || Next == S_INST ||
                    Next == S_BSTOP || Next == S_TMPL);
        State = Next;
        continue;
      case S_INST:
        CHECK_STATE(Next == S_BSTART || Next == S_INST || Next == S_BSTOP ||
                    Next == S_TMPL);
        State = Next;
        continue;
      case S_BSTOP:
        CHECK_STATE(Next == S_BSTART || Next == S_BSTOP || Next == S_TMPL);
        State = Next;
        continue;
      case S_TMPL:
        CHECK_STATE(Next == S_BSTART || Next == S_BMOD || Next == S_TMPL);
        State = Next;
        continue;
      default:
        llvm_unreachable("Impossible state!");
        break;
      }
    }
  }
}

bool LinxV5EmitHeader::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const LinxV5InstrInfo *>(MF.getSubtarget().getInstrInfo());

  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  instIsolate(MF);

  for (auto &MBB : make_early_inc_range(MF)) {
    splitMultiBranchBlock(MBB);
  }

  for (auto &MBB : MF) {
    addKillInst(MBB);
    insertBlockHeader(MBB);

    emitBranchHint(&MBB);
  }
  appendBstopToFunction(MF);

  verify(MF);

  return true;
}

FunctionPass *llvm::createLinxV5EmitHeaderPass() {
  return new LinxV5EmitHeader();
}
