add_llvm_component_group(LinxV5)

set(LLVM_TARGET_DEFINITIONS LinxV5.td)

tablegen(LLVM LinxV5GenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM LinxV5GenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM LinxV5GenDAGISel.inc -gen-dag-isel)
tablegen(LLVM LinxV5GenDisassemblerTables.inc -gen-disassembler)
tablegen(LLVM LinxV5GenGlobalISel.inc -gen-global-isel)
tablegen(LLVM LinxV5GenInstrInfo.inc -gen-instr-info)
tablegen(LLVM LinxV5GenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM LinxV5GenMCPseudoLowering.inc -gen-pseudo-lowering)
tablegen(LLVM LinxV5GenRegisterInfo.inc -gen-register-info)
tablegen(LLVM LinxV5GenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM LinxV5GenSearchableTables.inc -gen-searchable-tables)

add_public_tablegen_target(LinxV5CommonTableGen)

add_llvm_target(LinxV5CodeGen
  LinxV5MachineScheduler.cpp
  LinxV5AsmPrinter.cpp
  LinxV5ExpandPseudoInsts.cpp
  LinxV5FrameLowering.cpp
  LinxV5InstrInfo.cpp
  LinxV5ISelDAGToDAG.cpp
  LinxV5ISelLowering.cpp
  LinxV5LoopIndVarPromote.cpp
  LinxV5MCInstLower.cpp
  LinxV5RegisterInfo.cpp
  LinxV5Subtarget.cpp
  LinxV5TargetMachine.cpp
  LinxV5TargetTransformInfo.cpp
  LinxV5TargetObjectFile.cpp
  LinxV5CanonicalizeBlock.cpp
  LinxV5RegisterCanonicalization.cpp
  LinxV5BGPRFixup.cpp
  LinxV5TRegToOffset.cpp
  LinxV5PreEmitBlockOpt.cpp
  LinxV5RevertCSE.cpp
  LinxV5ClockhandsAllocHelper.cpp
  LinxV5ClockhandsColoring.cpp
  LinxV5EmitHeader.cpp
  LinxV5TileFixup.cpp
  LinxV5SIMTSpillFixup.cpp
  LinxV5StackSizeFixup.cpp
  LinxV5AnnotateControlFlow.cpp
  LinxV5TargetTransformInfo.cpp
  LinxV5BaseInfo.cpp
  LinxV5RebindGetTilePTR.cpp
  LinxV5LoadStoreBridgeOpt.cpp
  LinxV5ConstantRegOpt.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  Core
  CodeGen
  MC
  LinxV5Desc
  LinxV5Info
  SelectionDAG
  Support
  Target
  GlobalISel

  ADD_TO_COMPONENT
  LinxV5
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
