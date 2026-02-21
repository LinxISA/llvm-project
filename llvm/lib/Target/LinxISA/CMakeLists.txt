add_llvm_component_group(LinxISA)

set(LLVM_TARGET_DEFINITIONS LinxISA.td)

tablegen(LLVM LinxISAGenCallingConv.inc -gen-callingconv)
tablegen(LLVM LinxISAGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM LinxISAGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM LinxISAGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM LinxISAGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(LinxISACommonTableGen)

add_llvm_target(LinxISACodeGen
  LinxISA.cpp
  LinxISAAsmPrinter.cpp
  LinxISABlockify.cpp
  LinxISAFrameLowering.cpp
  LinxISAInstrInfo.cpp
  LinxISAISelDAGToDAG.cpp
  LinxISAISelLowering.cpp
  LinxISAMemOpsCombine.cpp
  LinxISATileSSABalance.cpp
  LinxISASIMTAutoVectorize.cpp
  LinxISAMCInstLower.cpp
  LinxISARegisterInfo.cpp
  LinxISASubtarget.cpp
  LinxISATargetTransformInfo.cpp
  LinxISATargetMachine.cpp

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  LinxISADesc
  LinxISAInfo
  MC
  SelectionDAG
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  LinxISA
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
