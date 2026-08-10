# LinxISA LLVM Compiler

## Scope

`compiler/llvm` is the LinxISA LLVM fork used by this superproject for compiler, linker, and runtime bring-up. It is the canonical source for Linx target codegen, MC/asm, ABI lowering, and toolchain binaries used by AVS and Linux/QEMU integration.

The active architectural contract is [LinxISA v0.58](https://github.com/LinxISA/linx-isa/releases/tag/v0.58), including its locked PTO-SPEC 0.58 common subset. This is a hard break from earlier profiles: removed spellings and encodings are rejected rather than normalized.

## Upstream

- Repository: `https://github.com/LinxISA/llvm-project`
- Merge-back target branch: `main`

## What This Submodule Owns

- LinxISA LLVM backend (`llvm/lib/Target/Linx*`)
- Linx-specific Clang/LLD integration
- Linx toolchain artifacts used by superproject gates
- LinxISA v0.58 assembly/disassembly and PTO common-subset identity checks

## Canonical Build and Test Commands

Run from the LinxISA superproject root. Replace the parallelism expression as needed on non-macOS hosts.

```bash
cmake -S compiler/llvm/llvm -B compiler/llvm/build-linxisa-clang -G Ninja \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=LinxISA \
  -DCMAKE_BUILD_TYPE=Release
cmake --build compiler/llvm/build-linxisa-clang \
  -j "$(sysctl -n hw.ncpu)" \
  --target clang lld llvm-mc llvm-objdump llc FileCheck yaml2obj

compiler/llvm/build-linxisa-clang/bin/llvm-lit \
  -j "$(sysctl -n hw.ncpu)" -sv \
  compiler/llvm/llvm/test/MC/LinxISA/v058-* \
  compiler/llvm/llvm/test/CodeGen/LinxISA/v058-* \
  compiler/llvm/clang/test/CodeGen/linxisa-builtins.c \
  compiler/llvm/lld/test/ELF/linxisa-pto-identity.test

cd avs/compiler/linx-llvm/tests
CLANG="$PWD/../../../../compiler/llvm/build-linxisa-clang/bin/clang" ./run.sh
CLANGXX="$PWD/../../../../compiler/llvm/build-linxisa-clang/bin/clang++" ./run_cpp.sh
```

## LinxISA Integration Touchpoints

- Primary compiler lane in `tools/regression/run.sh`
- Strict cross-repo gate in `tools/regression/strict_cross_repo.sh`
- ABI/runtime coupling with `lib/musl`, `lib/glibc`, and kernel userspace bring-up

## Related Docs

- `/Users/zhoubot/linx-isa/docs/project/navigation.md`
- `/Users/zhoubot/linx-isa/docs/bringup/`
- `/Users/zhoubot/linx-isa/avs/compiler/linx-llvm/README.md`
