add_llvm_component_library(LLVMLinxV5AsmParser
  LinxV5AsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  LinxV5Desc
  LinxV5Info
  Support

  ADD_TO_COMPONENT
  LinxV5
  )
