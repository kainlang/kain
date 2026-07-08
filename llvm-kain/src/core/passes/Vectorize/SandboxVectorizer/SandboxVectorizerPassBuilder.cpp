#include "core/passes/Vectorize/SandboxVectorizer/SandboxVectorizerPassBuilder.h"

#include "core/passes/Vectorize/SandboxVectorizer/Passes/BottomUpVec.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/LoadStoreVec.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/NullPass.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/PackReuse.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/PrintInstructionCount.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/PrintRegion.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/RegionsFromBBs.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/RegionsFromMetadata.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/SeedCollection.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/TransactionAcceptOrRevert.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/TransactionAlwaysAccept.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/TransactionAlwaysRevert.h"
#include "core/passes/Vectorize/SandboxVectorizer/Passes/TransactionSave.h"

namespace llvm::sandboxir {

std::unique_ptr<sandboxir::RegionPass>
SandboxVectorizerPassBuilder::createRegionPass(StringRef Name, StringRef Args,
                                               StringRef AuxArg) {
#define REGION_PASS(NAME, CLASS_NAME)                                          \
  if (Name == NAME) {                                                          \
    assert(Args.empty() && "Unexpected arguments for pass '" NAME "'.");       \
    assert(AuxArg.empty() && "TODO: Add RegionPass support for AuxArge);");    \
    return std::make_unique<CLASS_NAME>();                                     \
  }
// TODO: Support region passes with params.
#include "Passes/PassRegistry.def"
  return nullptr;
}

std::unique_ptr<sandboxir::FunctionPass>
SandboxVectorizerPassBuilder::createFunctionPass(StringRef Name, StringRef Args,
                                                 StringRef AuxArg) {
#define FUNCTION_PASS_WITH_PARAMS(NAME, CLASS_NAME)                            \
  if (Name == NAME)                                                            \
    return std::make_unique<CLASS_NAME>(Args, AuxArg);
#include "Passes/PassRegistry.def"
  return nullptr;
}

} // namespace llvm::sandboxir
