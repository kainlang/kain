//===- AttributesEmitter.cpp - Generate attributes ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// llvm-kain: adapted from LLVM 19 utils/TableGen/Attributes.cpp
// API fixes: const RecordKeeper, explicit SmallVector for ArrayRef init
//
//===----------------------------------------------------------------------===//

#include "tools/tablegen/Record.h"
#include "tools/tablegen/TableGenBackend.h"
#include "support/adt/SmallVector.h"
#include <vector>
using namespace llvm;

#define DEBUG_TYPE "attr-enum"

namespace {

class AttributesEmitter {
public:
  AttributesEmitter(const RecordKeeper &R) : Records(R) {}
  void run(raw_ostream &OS);

private:
  void emitTargetIndependentNames(raw_ostream &OS);
  void emitFnAttrCompatCheck(raw_ostream &OS);
  void emitAttributeProperties(raw_ostream &OS);

  const RecordKeeper &Records;
};

} // anonymous namespace

void AttributesEmitter::emitTargetIndependentNames(raw_ostream &OS) {
  OS << "#ifdef GET_ATTR_NAMES\n";
  OS << "#undef GET_ATTR_NAMES\n";

  OS << "#ifndef ATTRIBUTE_ALL\n";
  OS << "#define ATTRIBUTE_ALL(FIRST, SECOND)\n";
  OS << "#endif\n\n";

  SmallVector<StringRef, 5> EnumKindNames = {
    "EnumAttr", "TypeAttr", "IntAttr", "ConstantRangeAttr", "ConstantRangeListAttr"
  };
  SmallVector<StringRef, 1> StrBoolNames = {"StrBoolAttr"};
  SmallVector<StringRef, 1> ComplexStrNames = {"ComplexStrAttr"};

  auto Emit = [&](ArrayRef<StringRef> KindNames, StringRef MacroName) {
    OS << "#ifndef " << MacroName << "\n";
    OS << "#define " << MacroName
       << "(FIRST, SECOND) ATTRIBUTE_ALL(FIRST, SECOND)\n";
    OS << "#endif\n\n";
    for (StringRef KindName : KindNames) {
      for (auto *A : Records.getAllDerivedDefinitions(KindName)) {
        OS << MacroName << "(" << A->getName() << ","
           << A->getValueAsString("AttrString") << ")\n";
      }
    }
    OS << "#undef " << MacroName << "\n\n";
  };

  Emit(EnumKindNames, "ATTRIBUTE_ENUM");
  Emit(StrBoolNames, "ATTRIBUTE_STRBOOL");
  Emit(ComplexStrNames, "ATTRIBUTE_COMPLEXSTR");

  OS << "#undef ATTRIBUTE_ALL\n";
  OS << "#endif\n\n";

  OS << "#ifdef GET_ATTR_ENUM\n";
  OS << "#undef GET_ATTR_ENUM\n";
  unsigned Value = 1;
  SmallVector<StringRef, 5> EnumKinds = {
    "EnumAttr", "TypeAttr", "IntAttr", "ConstantRangeAttr", "ConstantRangeListAttr"
  };
  for (StringRef KindName : EnumKinds) {
    OS << "First" << KindName << " = " << Value << ",\n";
    for (auto *A : Records.getAllDerivedDefinitions(KindName)) {
      OS << A->getName() << " = " << Value << ",\n";
      Value++;
    }
    OS << "Last" << KindName << " = " << (Value - 1) << ",\n";
  }
  OS << "#endif\n\n";
}

void AttributesEmitter::emitFnAttrCompatCheck(raw_ostream &OS) {
  OS << "#ifdef GET_ATTR_COMPAT_FUNC\n";
  OS << "#undef GET_ATTR_COMPAT_FUNC\n";

  OS << "static inline bool hasCompatibleFnAttrs(const Function &Caller,\n"
     << "                                        const Function &Callee) {\n";
  OS << "  bool Ret = true;\n\n";

  auto CompatRules = Records.getAllDerivedDefinitions("CompatRule");
  for (auto *Rule : CompatRules) {
    StringRef FuncName = Rule->getValueAsString("CompatFunc");
    OS << "  Ret &= " << FuncName << "(Caller, Callee";
    StringRef AttrName = Rule->getValueAsString("AttrName");
    if (!AttrName.empty())
      OS << ", \"" << AttrName << "\"";
    OS << ");\n";
  }

  OS << "\n  return Ret;\n}\n\n";

  auto MergeRules = Records.getAllDerivedDefinitions("MergeRule");
  OS << "static inline void mergeFnAttrs(Function &Caller,\n"
     << "                                const Function &Callee) {\n";
  for (auto *Rule : MergeRules) {
    StringRef FuncName = Rule->getValueAsString("MergeFunc");
    OS << "  " << FuncName << "(Caller, Callee);\n";
  }
  OS << "}\n\n";

  OS << "#endif\n";
}

void AttributesEmitter::emitAttributeProperties(raw_ostream &OS) {
  OS << "#ifdef GET_ATTR_PROP_TABLE\n";
  OS << "#undef GET_ATTR_PROP_TABLE\n";
  OS << "static const uint8_t AttrPropTable[] = {\n";

  SmallVector<StringRef, 5> KindNames = {
    "EnumAttr", "TypeAttr", "IntAttr", "ConstantRangeAttr", "ConstantRangeListAttr"
  };
  for (StringRef KindName : KindNames) {
    for (auto *A : Records.getAllDerivedDefinitions(KindName)) {
      OS << "0";
      for (const Init *P : *A->getValueAsListInit("Properties"))
        OS << " | AttributeProperty::" << cast<DefInit>(P)->getDef()->getName();
      OS << ",\n";
    }
  }
  OS << "};\n";
  OS << "#endif\n";
}

void AttributesEmitter::run(raw_ostream &OS) {
  emitTargetIndependentNames(OS);
  emitFnAttrCompatCheck(OS);
  emitAttributeProperties(OS);
}

static TableGen::Emitter::OptClass<AttributesEmitter>
    X("gen-attrs", "Generate attributes");
