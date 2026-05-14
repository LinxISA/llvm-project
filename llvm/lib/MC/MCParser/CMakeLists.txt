include_directories(
  ${LLVM_BINARY_DIR}/lib/Target/Linx
  ${LLVM_MAIN_SRC_DIR}/lib/Target/Linx
  )

add_llvm_component_library(LLVMMCParser
  AsmLexer.cpp
  AsmParser.cpp
  COFFAsmParser.cpp
  COFFMasmParser.cpp
  GOFFAsmParser.cpp
  DarwinAsmParser.cpp
  ELFAsmParser.cpp
  MCAsmLexer.cpp
  MCAsmParser.cpp
  MCAsmParserExtension.cpp
  MCTargetAsmParser.cpp
  MasmParser.cpp
  WasmAsmParser.cpp
  XCOFFAsmParser.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/MC/MCParser

  LINK_COMPONENTS
  MC
  Support
  )
