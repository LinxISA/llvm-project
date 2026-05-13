add_llvm_component_library(LLVMLinxV5Disassembler
  LinxV5Disassembler.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  LinxV5Desc
  LinxV5Info
  Support

  ADD_TO_COMPONENT
  LinxV5
  )
