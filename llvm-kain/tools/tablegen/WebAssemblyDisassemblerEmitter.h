// Stub - WebAssembly target dropped
#ifndef LLVM_UTILS_TABLEGEN_WEBASSEMBLYDISASSEMBLEREMITTER_H
#define LLVM_UTILS_TABLEGEN_WEBASSEMBLYDISASSEMBLEREMITTER_H

#include "Common/CodeGenTarget.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

// Match the calling convention in DisassemblerEmitter.cpp
inline void emitWebAssemblyDisassemblerTables(raw_ostream &OS,
                                              ArrayRef<const CodeGenInstruction *> Insn) {
  // Stub - WebAssembly target dropped
  (void)OS;
  (void)Insn;
}

}
#endif
