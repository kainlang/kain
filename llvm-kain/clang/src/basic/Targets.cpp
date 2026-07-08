// stripped target — clang target info is not needed for Kain's use of clang
#include "basic/TargetInfo.h"
#include "support/target/Triple.h"
namespace clang {
TargetInfo *AllocateTargetInfo(const llvm::Triple &TT, const TargetOptions &TO) {
  return nullptr;
}
} // namespace clang
