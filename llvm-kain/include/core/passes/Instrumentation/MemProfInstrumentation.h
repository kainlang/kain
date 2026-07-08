// llvm-kain stub
#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_MEMPROFINSTRUMENTATION_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_MEMPROFINSTRUMENTATION_H
#include "core/ir/PassManager.h"
namespace llvm {
class ModuleMemProfilerPass : public PassInfoMixin<ModuleMemProfilerPass> {};
class MemProfilerPass : public PassInfoMixin<MemProfilerPass> {};
} // namespace llvm
#endif
