// Minimal TableGen definitions for the LinxISA target (MC-layer bring-up).

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// LinxISA Subtarget features.
//===----------------------------------------------------------------------===//

def FeatureLNXS32
    : SubtargetFeature<"lnx-s32", "HasExtS32", "true",
                       "Enable Linx scalar 32-bit base extension">;

def FeatureLNXS64
    : SubtargetFeature<"lnx-s64", "HasExtS64", "true",
                       "Enable Linx scalar 64-bit extension",
                       [FeatureLNXS32]>;

def FeatureLNXC
    : SubtargetFeature<"lnx-c", "HasExtC", "true",
                       "Enable Linx compressed extension",
                       [FeatureLNXS32]>;

def FeatureLNXF
    : SubtargetFeature<"lnx-f", "HasExtF", "true",
                       "Enable Linx scalar floating-point extension",
                       [FeatureLNXS32]>;

def FeatureLNXA
    : SubtargetFeature<"lnx-a", "HasExtA", "true",
                       "Enable Linx atomic extension",
                       [FeatureLNXS32]>;

def FeatureLNXSYS
    : SubtargetFeature<"lnx-sys", "HasExtSys", "true",
                       "Enable Linx system/privilege/cache/SSR extension",
                       [FeatureLNXS32]>;

def FeatureLNXV
    : SubtargetFeature<"lnx-v", "HasExtV", "true",
                       "Enable Linx vector/SIMT extension",
                       [FeatureLNXS32]>;

def FeatureLNXM
    : SubtargetFeature<"lnx-m", "HasExtM", "true",
                       "Enable Linx matrix/tile extension",
                       [FeatureLNXS32]>;

def FeaturePTO0591Scoped
    : SubtargetFeature<"pto-v0591-scoped", "HasPTO0591Scoped", "true",
                       "Enable PTO 0.59.1 scoped Scalar assembly candidate">;

//===----------------------------------------------------------------------===//
// Register File, Calling Convention, Instruction Descriptions.
//===----------------------------------------------------------------------===//

include "LinxISARegisterInfo.td"
include "LinxISACallingConv.td"
include "LinxISAInstrInfo.td"

defm : RemapAllTargetPseudoPointerOperands<GPR>;

def LinxISAInstrInfo : InstrInfo;

class Proc<string Name, list<SubtargetFeature> Features>
    : ProcessorModel<Name, NoSchedModel, Features>;

def : Proc<"generic", [FeatureLNXS32]>;
def : Proc<"linx32", [FeatureLNXS32]>;
def : Proc<"linx64", [FeatureLNXS32, FeatureLNXS64]>;

def LinxISA : Target {
  let InstructionSet = LinxISAInstrInfo;
}
