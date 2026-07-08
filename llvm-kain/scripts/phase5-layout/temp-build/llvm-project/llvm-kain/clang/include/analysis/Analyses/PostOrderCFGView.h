//===- PostOrderCFGView.h - Post order view of CFG blocks -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements post order view of the blocks in a CFG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_POSTORDERCFGVIEW_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_POSTORDERCFGVIEW_H

#include "analysis/AnalysisDeclContext.h"
#include "analysis/CFG.h"
#include "basic/LLVM.h"
#include "support/adt/BitVector.h"
#include "support/adt/DenseMap.h"
#include "support/adt/PostOrderIterator.h"
#include <utility>
#include <vector>

namespace clang {

class PostOrderCFGView : public ManagedAnalysis {
  virtual void anchor();

public:
  /// Implements a set of CFGBlocks using a BitVector.
  class CFGBlockSet {
    llvm::BitVector VisitedBlockIDs;

  public:
    CFGBlockSet() = default;
    CFGBlockSet(const CFG *G) : VisitedBlockIDs(G->getNumBlockIDs(), false) {}

    /// Set the bit associated with a particular CFGBlock.
    std::pair<std::nullopt_t, bool> insert(const CFGBlock *Block) {
      if (VisitedBlockIDs.test(Block->getBlockID()))
        return std::make_pair(std::nullopt, false);
      VisitedBlockIDs.set(Block->getBlockID());
      return std::make_pair(std::nullopt, true);
    }

    /// Check if the bit for a CFGBlock has been already set.
    /// This method is for tracking visited blocks in the main threadsafety
    /// loop. Block must not be null.
    bool alreadySet(const CFGBlock *Block) {
      return VisitedBlockIDs.test(Block->getBlockID());
    }
  };

private:
  std::vector<const CFGBlock *> Blocks;

  using BlockOrderTy = llvm::DenseMap<const CFGBlock *, unsigned>;
  BlockOrderTy BlockOrder;

public:
  friend struct BlockOrderCompare;

  using iterator = std::vector<const CFGBlock *>::reverse_iterator;
  using const_iterator = std::vector<const CFGBlock *>::const_reverse_iterator;

  PostOrderCFGView(const CFG *cfg);

  iterator begin() { return Blocks.rbegin(); }
  iterator end() { return Blocks.rend(); }

  const_iterator begin() const { return Blocks.rbegin(); }
  const_iterator end() const { return Blocks.rend(); }

  bool empty() const { return begin() == end(); }

  struct BlockOrderCompare {
    const PostOrderCFGView &POV;

  public:
    BlockOrderCompare(const PostOrderCFGView &pov) : POV(pov) {}

    bool operator()(const CFGBlock *b1, const CFGBlock *b2) const;
  };

  BlockOrderCompare getComparator() const {
    return BlockOrderCompare(*this);
  }

  // Used by AnalyisContext to construct this object.
  static const void *getTag();

  static std::unique_ptr<PostOrderCFGView>
  create(AnalysisDeclContext &analysisContext);
};

} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_POSTORDERCFGVIEW_H
