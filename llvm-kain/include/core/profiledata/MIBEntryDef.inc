//===- MIBEntryDef.inc - MemInfoBlock entry definitions -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Hand-written stub for llvm-kain flat layout.
// 
// Supported macro redefinitions at include time:
//   MIBEntryDef(NameTag, Name, Type) — meminfo block entry definitions
//
// Macro signature: MIBEntryDef(NameTag, field_name, CppType)
//===----------------------------------------------------------------------===//

#ifdef MIBEntryDef
MIBEntryDef(AllocCount, AllocCount, uint64_t)
MIBEntryDef(TotalAccessCount, TotalAccessCount, uint64_t)
MIBEntryDef(TotalSize, TotalSize, uint64_t)
MIBEntryDef(MinSize, MinSize, uint64_t)
MIBEntryDef(MaxSize, MaxSize, uint64_t)
MIBEntryDef(NumMigrated, NumMigrated, uint64_t)
MIBEntryDef(NumTlbMapped, NumTlbMapped, uint64_t)
MIBEntryDef(NumSurviving, NumSurviving, uint64_t)
MIBEntryDef(NumFrees, NumFrees, uint64_t)
MIBEntryDef(NumColdBytes, NumColdBytes, uint64_t)
MIBEntryDef(NumHotBytes, NumHotBytes, uint64_t)
MIBEntryDef(NumWarmBytes, NumWarmBytes, uint64_t)
MIBEntryDef(TotalLifetime, TotalLifetime, uint64_t)
MIBEntryDef(MinLifetime, MinLifetime, uint64_t)
MIBEntryDef(MaxLifetime, MaxLifetime, uint64_t)
MIBEntryDef(AllocTimestamp, AllocTimestamp, uint64_t)
MIBEntryDef(FreeTimestamp, FreeTimestamp, uint64_t)
MIBEntryDef(AccessCount, AccessCount, uint64_t)
MIBEntryDef(TotalFaultCount, TotalFaultCount, uint64_t)
MIBEntryDef(TotalMapCount, TotalMapCount, uint64_t)
MIBEntryDef(TotalUnmapCount, TotalUnmapCount, uint64_t)
MIBEntryDef(SoftFaultCount, SoftFaultCount, uint64_t)
MIBEntryDef(HardFaultCount, HardFaultCount, uint64_t)
MIBEntryDef(PagesPlacedOnZswap, PagesPlacedOnZswap, uint64_t)
MIBEntryDef(PagesZswappedIn, PagesZswappedIn, uint64_t)
MIBEntryDef(DirtyShared, DirtyShared, uint64_t)
MIBEntryDef(DirtyExclusive, DirtyExclusive, uint64_t)
MIBEntryDef(ReadOnlyShared, ReadOnlyShared, uint64_t)
MIBEntryDef(ReadOnlyExclusive, ReadOnlyExclusive, uint64_t)
MIBEntryDef(ReadOnly, ReadOnly, uint64_t)
MIBEntryDef(ReadMostly, ReadMostly, uint64_t)
MIBEntryDef(ReadWrite, ReadWrite, uint64_t)
MIBEntryDef(NonvolatileRmw, NonvolatileRmw, uint64_t)
MIBEntryDef(VolatileRmw, VolatileRmw, uint64_t)
MIBEntryDef(NonvolatileRmwCmpXchg, NonvolatileRmwCmpXchg, uint64_t)
MIBEntryDef(VolatileRmwCmpXchg, VolatileRmwCmpXchg, uint64_t)
MIBEntryDef(NonvolatileRead, NonvolatileRead, uint64_t)
MIBEntryDef(VolatileRead, VolatileRead, uint64_t)
MIBEntryDef(NonvolatileWrite, NonvolatileWrite, uint64_t)
MIBEntryDef(VolatileWrite, VolatileWrite, uint64_t)
MIBEntryDef(NonvolatileAtomic, NonvolatileAtomic, uint64_t)
MIBEntryDef(VolatileAtomic, VolatileAtomic, uint64_t)
MIBEntryDef(Dirty, Dirty, uint64_t)
MIBEntryDef(NumPages, NumPages, uint64_t)
MIBEntryDef(Unmapped, Unmapped, uint64_t)
MIBEntryDef(Pinned, Pinned, uint64_t)
MIBEntryDef(NumBlocks, NumBlocks, uint64_t)
MIBEntryDef(NumSites, NumSites, uint64_t)
MIBEntryDef(AccessFrequency, AccessFrequency, uint64_t)
MIBEntryDef(DirtyAccessFrequency, DirtyAccessFrequency, uint64_t)
#endif
