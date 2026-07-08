#include "support/adt/LLVMLocale.h"
#include "support/adt/StringRef.h"
#include "support/adt/Unicode.h"

namespace llvm {
namespace sys {
namespace locale {

int columnWidth(StringRef Text) {
  return llvm::sys::unicode::columnWidthUTF8(Text);
}

bool isPrint(int UCS) {
  return llvm::sys::unicode::isPrintable(UCS);
}

} // namespace locale
} // namespace sys
} // namespace llvm
