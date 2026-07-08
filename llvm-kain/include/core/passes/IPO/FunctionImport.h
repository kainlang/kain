// llvm-kain stub for FunctionImport.h
#ifndef LLVM_TRANSFORMS_IPO_FUNCTIONIMPORT_H
#define LLVM_TRANSFORMS_IPO_FUNCTIONIMPORT_H
#include "core/ir/Module.h"
#include "llvm/ADT/DenseSet.h"
namespace llvm {
class FunctionImporter {
public:
  FunctionImporter() = default;
};

class FunctionImportPass : public PassInfoMixin<FunctionImportPass> {};
using GVSummaryMapTy = DenseSet<GlobalValue::GUID>;
} // namespace llvm
#endif
