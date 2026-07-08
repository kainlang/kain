// Stub: AttrParsedAttrKinds.inc
#ifndef ATTR_PARSED_ATTR_KINDS_INC
#define ATTR_PARSED_ATTR_KINDS_INC
#include "basic/AttributeCommonInfo.h"
static inline clang::AttributeCommonInfo::Kind
getAttrKind(llvm::StringRef Name, 
            clang::AttributeCommonInfo::Syntax SyntaxUsed) {
  return clang::AttributeCommonInfo::Kind::UnknownAttribute;
}
#endif
