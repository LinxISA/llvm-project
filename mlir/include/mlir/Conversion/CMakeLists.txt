
set(LLVM_TARGET_DEFINITIONS Passes.td)

# LinxISA extension: the Conversion pass set optionally references PTO.
# Gate the TableGen pass definitions with a preprocessor define.
set(_mlir_conversion_passes_td_defines "")
if (MLIR_ENABLE_LINXISA_PTO)
  list(APPEND _mlir_conversion_passes_td_defines -DMLIR_ENABLE_LINXISA_PTO)
endif()

mlir_tablegen(Passes.h.inc -gen-pass-decls -name Conversion ${_mlir_conversion_passes_td_defines})
mlir_tablegen(Passes.capi.h.inc -gen-pass-capi-header --prefix Conversion ${_mlir_conversion_passes_td_defines})
mlir_tablegen(Passes.capi.cpp.inc -gen-pass-capi-impl --prefix Conversion ${_mlir_conversion_passes_td_defines})
add_mlir_generic_tablegen_target(MLIRConversionPassIncGen)

add_mlir_doc(Passes ConversionPasses ./ -gen-pass-doc ${_mlir_conversion_passes_td_defines})

add_subdirectory(ConvertToLLVM)
