// llvm-kain stub
#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_MEMPROFUSE_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_MEMPROFUSE_H
#include "core/ir/PassManager.h"
namespace llvm {
class MemProfUsePass : public PassInfoMixin<MemProfUsePass> {
public:
  MemProfUsePass(std::string ProfileFilename = "") {}
};
} // namespace llvm
#endif
