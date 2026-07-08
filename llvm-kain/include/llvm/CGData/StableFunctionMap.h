// llvm-kain stub: StableFunctionMap.h (CGData deleted in Phase 3)
#ifndef LLVM_CGDATA_STABLEFUNCTIONMAP_H
#define LLVM_CGDATA_STABLEFUNCTIONMAP_H

#include <cstdint>
#include <utility>
#include <vector>

namespace llvm {
namespace cgdata {

using IndexPair = std::pair<uint32_t, uint32_t>;
using IndexOperandHashVecType = std::vector<uint64_t>;

class StableFunctionMap {
public:
  struct StableFunctionEntry {
    uint32_t FunctionIndex;
    uint32_t ModuleIndex;
    uint64_t Hash;
  };

  StableFunctionMap() = default;
  bool empty() const { return true; }
  void clear() {}
  void insert(const StableFunctionEntry &) {}
  StableFunctionEntry &emplace_back() {
    static StableFunctionEntry dummy;
    return dummy;
  }
};

} // namespace cgdata

using cgdata::IndexPair;
using cgdata::StableFunctionMap;
using cgdata::IndexOperandHashVecType;

} // namespace llvm

#endif
