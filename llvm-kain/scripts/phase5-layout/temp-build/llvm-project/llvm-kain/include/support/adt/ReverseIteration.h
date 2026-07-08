#ifndef LLVM_SUPPORT_REVERSEITERATION_H
#define LLVM_SUPPORT_REVERSEITERATION_H

#include "core/config/abi-breaking.h"
#include "support/adt/PointerLikeTypeTraits.h"

namespace llvm {

template <class T = void *> constexpr bool shouldReverseIterate() {
#if LLVM_ENABLE_REVERSE_ITERATION
  return detail::IsPointerLike<T>::value;
#else
  return false;
#endif
}

} // namespace llvm
#endif
