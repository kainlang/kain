// llvm-kain stub — TextAPI deleted in Phase 3
#ifndef LLVM_OBJECT_TAPIUNIVERSAL_H
#define LLVM_OBJECT_TAPIUNIVERSAL_H
#include "core/object/Binary.h"
namespace llvm { namespace object {
class TapiUniversal : public Binary {
public:
  static bool classof(const Binary *v) { return false; }
  static Expected<std::unique_ptr<TapiUniversal>> create(MemoryBufferRef B) {
    return createStringError(std::errc::not_supported, "TAPI not supported");
  }
};
} }
#endif
