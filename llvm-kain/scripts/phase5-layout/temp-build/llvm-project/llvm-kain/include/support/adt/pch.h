//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// Precompiled header for LLVMSupport.
///
//===----------------------------------------------------------------------===//

#include "support/adt/ADL.h"
#include "support/adt/APFloat.h"
#include "support/adt/APInt.h"
#include "support/adt/APSInt.h"
#include "support/adt/ArrayRef.h"
#include "support/adt/BitVector.h"
#include "support/adt/DenseMap.h"
#include "support/adt/Hashing.h"
#include "support/adt/STLExtras.h"
#include "support/adt/SetVector.h"
#include "support/adt/SmallString.h"
#include "support/adt/SmallVector.h"
#include "support/adt/Statistic.h"
#include "support/adt/StringExtras.h"
#include "support/adt/StringRef.h"
#include "support/adt/Twine.h"
#include "support/adt/Casting.h"
#include "support/adt/CommandLine.h"
#include "support/adt/Error.h"
#include "support/adt/FileSystem.h"
#include "support/adt/FormatVariadic.h"
#include "support/adt/JSON.h"
#include "support/adt/SourceMgr.h"
#include "support/adt/VersionTuple.h"
#include "support/adt/YAMLTraits.h"
#include "support/adt/raw_ostream.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
