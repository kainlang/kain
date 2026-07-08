//===-- llvm/Constant.h - Constant class definition -------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the Constant class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_CONSTANT_H
#define LLVM_IR_CONSTANT_H

#include "core/ir/User.h"
#include "core/ir/Value.h"
#include "support/adt/Casting.h"
#include "support/adt/Compiler.h"

namespace llvm {

class ConstantRange;
class APInt;

class Constant : public User {
protected:
  enum {
    IsNullValue = (1 << 6),
  };

  static constexpr unsigned ConstantSubclassBits = IsNullValue;

  Constant(Type *ty, ValueTy vty, AllocInfo AllocInfo)
      : User(ty, vty, AllocInfo) {}

  ~Constant() = default;

public:
  void operator=(const Constant &) = delete;
  Constant(const Constant &) = delete;

  bool isNullValue() const { return SubclassOptionalData & IsNullValue; }

  LLVM_ABI bool isOneValue() const;
  LLVM_ABI bool isNotOneValue() const;
  LLVM_ABI bool isAllOnesValue() const;
  LLVM_ABI bool isNegativeZeroValue() const;
  LLVM_ABI bool isNotMinSignedValue() const;
  LLVM_ABI bool isMinSignedValue() const;
  LLVM_ABI bool isMaxSignedValue() const;
  LLVM_ABI bool isFiniteNonZeroFP() const;
  LLVM_ABI bool isNormalFP() const;
  LLVM_ABI bool hasExactInverseFP() const;
  LLVM_ABI bool isNaN() const;
  LLVM_ABI bool isElementWiseEqual(Value *Y) const;
  LLVM_ABI bool containsUndefOrPoisonElement() const;
  LLVM_ABI bool containsPoisonElement() const;
  LLVM_ABI bool containsUndefElement() const;
  LLVM_ABI bool containsConstantExpression() const;
  LLVM_ABI bool isThreadDependent() const;
  LLVM_ABI bool isDLLImportDependent() const;
  LLVM_ABI bool isConstantUsed() const;
  LLVM_ABI bool needsRelocation() const;
  LLVM_ABI bool needsDynamicRelocation() const;
  LLVM_ABI Constant *getAggregateElement(unsigned Elt) const;
  LLVM_ABI Constant *getAggregateElement(Constant *Elt) const;
  LLVM_ABI Constant *getSplatValue(bool AllowPoison = false) const;
  LLVM_ABI const APInt &getUniqueInteger() const;
  LLVM_ABI ConstantRange toConstantRange() const;
  LLVM_ABI void destroyConstant();

  static bool classof(const Value *V) {
    static_assert(ConstantFirstVal == 0, "V->getValueID() >= ConstantFirstVal always succeeds");
    return V->getValueID() <= ConstantLastVal;
  }

  LLVM_ABI void handleOperandChange(Value *, Value *);
  LLVM_ABI static Constant *getNullValue(Type *Ty);
  LLVM_ABI static Constant *getAllOnesValue(Type *Ty);
  LLVM_ABI static Constant *getIntegerValue(Type *Ty, const APInt &V);
  LLVM_ABI void removeDeadConstantUsers() const;
  LLVM_ABI bool hasOneLiveUse() const;
  LLVM_ABI bool hasZeroLiveUses() const;

  const Constant *stripPointerCasts() const {
    return cast<Constant>(Value::stripPointerCasts());
  }

  Constant *stripPointerCasts() {
    return const_cast<Constant*>(
                      static_cast<const Constant *>(this)->stripPointerCasts());
  }

  LLVM_ABI static Constant *replaceUndefsWith(Constant *C,
                                              Constant *Replacement);
  LLVM_ABI static Constant *mergeUndefsWith(Constant *C, Constant *Other);
  LLVM_ABI bool isManifestConstant() const;

private:
  enum PossibleRelocationsTy {
    NoRelocation = 0,
    LocalRelocation = 1,
    GlobalRelocation = 2,
  };

  PossibleRelocationsTy getRelocationInfo() const;
  bool hasNLiveUses(unsigned N) const;
};

} // end namespace llvm

#endif // LLVM_IR_CONSTANT_H
