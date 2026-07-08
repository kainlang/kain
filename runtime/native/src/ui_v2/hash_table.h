// ============================================================================
//  hash_table.h — FNV-1a open-addressing hash table (stable key → node index)
//
//  Replaces the inline hash functions in tree.c (lines 22-48) which had a
//  critical bug: hardcoded probe limit of 8 silently returning -1, causing
//  duplicate node creation. This implementation has NO probe limit.
//
//  Architecture:
//    - FNV-1a 64-bit (same as input_system.c for Z3 proof reuse)
//    - Open-addressing with linear probing
//    - Fixed 4096 slots embedded in session struct (no resize)
//    - Max load 256 entries (alpha = 0.0625) enforced by caller
//    - Tombstone sentinel for deletions (probe-chain safe)
//
//  z3: kt-fnv1a-fast.smt2, hash_table_proofs.yaml
// ============================================================================
#ifndef KAINTANA_HASH_TABLE_H
#define KAINTANA_HASH_TABLE_H

#include "kaintana.h"
#include <stdint.h>
#include <stdbool.h>

// ── FNV-1a 64-bit hash of a null-terminated string ─────────────────────────
// Offset: 0xCBF29CE484222325ULL, Prime: 0x100000001B3ULL
// Z3-proven UNSAT: kt-fnv1a-fast.smt2
// NOTE: FNV-1a is NOT collision-resistant. This is for UI attribute names
//       and stable keys, not security.
uint64_t kaintana_hash_fnv1a(const char* key);

// ── Look up a hash in the open-addressing table ────────────────────────────
// Returns the stored value (node index), or -1 if not found.
// UNLIKE tree.c's version, this does NOT have a probe limit of 8.
// It probes until finding the hash or an empty slot (hash_slots[i] == 0).
// Tombstone slots are skipped (treated as occupied for lookup purposes).
int kaintana_hash_lookup(kt_Session* s, uint64_t hash);

// ── Insert a hash → value mapping ──────────────────────────────────────────
// Overwrites if hash already exists (updates value in place).
// Otherwise probes for the first empty or tombstone slot and inserts there.
// The first tombstone slot is preferred over later empty slots to avoid
// breaking probe chains for existing entries.
//
// WARNING: If the table is full (all 4096 slots occupied), behavior is
// undefined. Caller must ensure load factor stays under
// KAINTANA_HASH_MAX_LOAD (256 entries).
void kaintana_hash_insert(kt_Session* s, uint64_t hash, int value);

// ── Remove a hash from the table (tombstone via sentinel) ──────────────────
// Marks the slot as tombstone (0xFFFFFFFFFFFFFFFFULL) so probe chains for
// other entries are not broken. Returns true if removed, false if not found.
bool kaintana_hash_remove(kt_Session* s, uint64_t hash);

// ── Clear all entries from the hash table ──────────────────────────────────
// Zeroes both hash_slots and hash_values arrays (O(n) memset).
void kaintana_hash_clear(kt_Session* s);

// ── Get current load (number of occupied slots) ────────────────────────────
// Scans all 4096 slots counting non-zero, non-tombstone entries.
// Intended for debug assertions. O(n) call — not in hot path.
int kaintana_hash_load(kt_Session* s);

#endif // KAINTANA_HASH_TABLE_H
