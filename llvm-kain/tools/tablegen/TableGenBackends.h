// Stub for LLVM 18 compatibility
#ifndef LLVM_UTILS_TABLEGEN_TABLEGENBACKENDS_H
#define LLVM_UTILS_TABLEGEN_TABLEGENBACKENDS_H
namespace llvm {
class raw_ostream;
class RecordKeeper;
void EmitMapTable(const RecordKeeper &RK, raw_ostream &OS);
void EmitDecoder(const RecordKeeper &RK, raw_ostream &OS);
}
#endif
