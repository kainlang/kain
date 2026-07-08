//===- lib/MC/MCInst.cpp - MCInst implementation --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "core/mc/MCInst.h"
#include "core/config/llvm-config.h"
#include "core/mc/MCAsmInfo.h"
#include "core/mc/MCContext.h"
#include "core/mc/MCExpr.h"
#include "core/mc/MCInstPrinter.h"
#include "core/mc/MCRegisterInfo.h"
#include "support/adt/Casting.h"
#include "support/adt/Compiler.h"
#include "support/adt/Debug.h"
#include "support/adt/raw_ostream.h"

using namespace llvm;

void MCOperand::print(raw_ostream &OS, const MCContext *Ctx) const {
  OS << "<MCOperand ";
  if (!isValid())
    OS << "INVALID";
  else if (isReg()) {
    OS << "Reg:";
    if (Ctx && Ctx->getRegisterInfo())
      OS << Ctx->getRegisterInfo()->getName(getReg());
    else
      OS << getReg().id();
  } else if (isImm())
    OS << "Imm:" << getImm();
  else if (isSFPImm())
    OS << "SFPImm:" << bit_cast<float>(getSFPImm());
  else if (isDFPImm())
    OS << "DFPImm:" << bit_cast<double>(getDFPImm());
  else if (isExpr()) {
    OS << "Expr:";
    if (Ctx)
      Ctx->getAsmInfo().printExpr(OS, *getExpr());
    else
      getExpr()->print(OS, nullptr);
  } else if (isInst()) {
    OS << "Inst:(";
    if (const auto *Inst = getInst())
      Inst->print(OS, Ctx);
    else
      OS << "NULL";
    OS << ")";
  } else
    OS << "UNDEFINED";
  OS << ">";
}

bool MCOperand::evaluateAsConstantImm(int64_t &Imm) const {
  if (isImm()) {
    Imm = getImm();
    return true;
  }
  return false;
}

bool MCOperand::isBareSymbolRef() const {
  assert(isExpr() &&
         "isBareSymbolRef expects only expressions");
  const MCExpr *Expr = getExpr();
  MCExpr::ExprKind Kind = getExpr()->getKind();
  return Kind == MCExpr::SymbolRef &&
         cast<MCSymbolRefExpr>(Expr)->getSpecifier() == 0;
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void MCOperand::dump() const {
  print(dbgs());
  dbgs() << "\n";
}
#endif

void MCInst::print(raw_ostream &OS, const MCContext *Ctx) const {
  OS << "<MCInst " << getOpcode();
  for (unsigned i = 0, e = getNumOperands(); i != e; ++i) {
    OS << " ";
    getOperand(i).print(OS, Ctx);
  }
  OS << ">";
}

void MCInst::dump_pretty(raw_ostream &OS, const MCInstPrinter *Printer,
                         StringRef Separator, const MCContext *Ctx) const {
  StringRef InstName = Printer ? Printer->getOpcodeName(getOpcode()) : "";
  dump_pretty(OS, InstName, Separator, Ctx);
}

void MCInst::dump_pretty(raw_ostream &OS, StringRef Name, StringRef Separator,
                         const MCContext *Ctx) const {
  OS << "<MCInst #" << getOpcode();

  // Show the instruction opcode name if we have it.
  if (!Name.empty())
    OS << ' ' << Name;

  for (unsigned i = 0, e = getNumOperands(); i != e; ++i) {
    OS << Separator;
    getOperand(i).print(OS, Ctx);
  }
  OS << ">";
}

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
LLVM_DUMP_METHOD void MCInst::dump() const {
  print(dbgs());
  dbgs() << "\n";
}
#endif
