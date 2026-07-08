// Stub IRPrintingPasses for X86CodeGenPassBuilder
#ifndef LLVM_IRPRINTER_IRPRINTINGPASSES_H
#define LLVM_IRPRINTER_IRPRINTINGPASSES_H
#include "core/ir/PassManager.h"
#include "core/ir/Function.h"
#include "core/ir/Module.h"
namespace llvm {
class raw_ostream;
class PrintFunctionPass : public PassInfoMixin<PrintFunctionPass> {
public:
  PrintFunctionPass() {}
  PrintFunctionPass(raw_ostream &OS, bool DeleteStr = false) {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    return PreservedAnalyses::all();
  }
};
} // namespace llvm
#endif
