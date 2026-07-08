//===- MemProfData.inc - MemProf data structure definitions ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Hand-written stub for llvm-kain flat layout.
//===----------------------------------------------------------------------===//

#ifndef MEMPROF_DATA_INC_STANDALONE
#define MEMPROF_DATA_INC_STANDALONE

// MemProf raw version constant (used by MemProfReader.h)
#define MEMPROF_RAW_VERSION 2ULL

#endif // MEMPROF_DATA_INC_STANDALONE

// Supported macro redefinitions at include time:

#ifdef MEMPROF_DATA
// MEMPROF_DATA(Type, LLVMType, Name, Init) — memprof indexed data fields
MEMPROF_DATA(uint64_t, uint64_t, Version, 0)
MEMPROF_DATA(uint64_t, uint64_t, TotalSize, 0)
#undef MEMPROF_DATA
#endif

#ifdef MEMPROF_RAW_HEADER
// MEMPROF_RAW_HEADER(Type, Name, Init) — memprof raw header fields
MEMPROF_RAW_HEADER(uint64_t, Magic, 0)
MEMPROF_RAW_HEADER(uint64_t, Version, 0)
#undef MEMPROF_RAW_HEADER
#endif
