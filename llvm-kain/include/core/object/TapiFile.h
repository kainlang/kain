// llvm-kain stub — TextAPI deleted in Phase 3
#ifndef LLVM_OBJECT_TAPIFILE_H
#define LLVM_OBJECT_TAPIFILE_H
#include "core/object/Binary.h"
namespace llvm { namespace object {
class TapiFile : public Binary {
public:
  static bool classof(const Binary *v) { return false; }
};
} }
#endif
