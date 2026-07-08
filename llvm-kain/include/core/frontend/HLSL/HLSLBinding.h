// llvm-kain stub: HLSLBinding.h (HLSL frontend deleted in Phase 2)
#ifndef LLVM_FRONTEND_HLSL_HLSLBINDING_H
#define LLVM_FRONTEND_HLSL_HLSLBINDING_H

#include <cstdint>
#include <optional>

namespace llvm {
namespace hlsl {

struct BindingInfo {
  uint32_t Register = 0;
  uint32_t Space = 0;
  template<typename T> std::optional<uint32_t> findAvailableBinding(T, uint32_t, uint32_t) const { return 0; }
};

} // namespace hlsl
} // namespace llvm

#endif
