add_llvm_component_library(LLVMLinxV5Info
  LinxV5TargetInfo.cpp

  LINK_COMPONENTS
  Support

  ADD_TO_COMPONENT
  LinxV5
  )
