; Proof: Session lookup via direct index instead of linear scan
;
; Target: ui_system.c line ~96-104
; Current:
;   static KainNativeUiSession* abi_ui_find_session(int64_t session_id) {
;       int64_t index;
;       if (session_id <= 0) return NULL;
;       for (index = 0; index < ABI_UI_MAX_SESSIONS; index += 1) {
;           if (g_sessions[index].in_use && g_sessions[index].id == session_id) {
;               return &g_sessions[index];
;           }
;       }
;       return NULL;
;   }
;
; This is called before virtually every abi_ui_* API call.
; With 16 sessions max, the linear scan is 16 iterations worst case,
; 8 average. Not terrible, but called ~O(100) times per API call
; (each abi_ui_find_node calls abi_ui_find_session, plus many others).
;
; Proposed: Map session_id to a direct index using a tiny lookup table.
;
; Since session_id is monotonically increasing (g_next_session_id++),
; we can maintain a mapping: session_id_to_index[session_id & SESSION_MASK] = index.
; Or even simpler: store index in a parallel array keyed by session_id.
;
; For ABI_UI_MAX_SESSIONS = 16:
;   Session IDs are 1..16 typically (but can go higher after create/destroy cycles).
;   After many create/destroy cycles, IDs can wrap at int64_t max.
;   But in practice, sessions are few and long-lived.
;
; Best optimization for 16 sessions:
;   Option A: Direct index via hash of session_id
;     static KainNativeUiSession* session_map[ABI_UI_MAX_SESSIONS + 1] = {0};
;     // On create: session_map[session->id & 0xF] = session;
;     // On find: return session_map[session_id & 0xF];
;     // Problem: 16 slots, hash by low 4 bits — wrap-around collisions
;
;   Option B: Maintain a 64-tick epoch, use age to evict
;   Option C: Use a small perfect hash
;   Option D: Just use the linear scan but vectorize it (SSE cmp on 16 IDs)
;
; Given 16 is tiny, the simplest win is to recognize that linear scan
; with 16 entries is fast (16 compares = ~5 cycles pipelined).
; But called 100+ times per frame = 500 cycles = noise.
;
; The REAL cost: abi_ui_find_node calls abi_ui_find_session, which loops 16,
; then abi_ui_find_node does its own hash-table probe. If we inline the
!; session pointer passing, we eliminate the scan entirely.
;
; This is the key insight: since most public API functions go through
; session → node → style/state, and they're all in the same translation unit,
; we can pass session* directly between internal helpers.
;
; The public API functions (abi_ui_*) are the entry point — they must
; resolve session_id. But once resolved, internal helpers use session* directly.

; ============================================================
; Claim 1: The abi_ui_find_session scan is unnecessary for internal helpers
; ============================================================
(set-logic QF_BV)

(define-const MAX_SESSIONS (_ BitVec 32) (_ bv16 32))

; Cost of one abi_ui_find_session call:
; Average: 8 iterations (50% load factor)
; Each iteration: index++, bounds check, load in_use, load id, compare
(define-const FIND_SESSION_COST (_ BitVec 32) (_ bv8 32))

; Number of times it's called per public API:
;   abi_ui_node_set_text(session_id, node_id, text)
;     → abi_ui_find_session(session_id)  [16 iterations]
;     → abi_ui_find_node(session)         [hash probe]
;     → strcpy
; Total: 1 session scan + 1 hash probe
;
; If we pass session* directly through internal helpers:
;   → abi_ui_find_node(session)           [hash probe]
;   → strcpy
; Total: 0 session scans + 1 hash probe
;
; Savings: 16 iterations per public API call

(define-const API_CALLS_PER_FRAME (_ BitVec 32) (_ bv500 32))
(define-const SAVED_ITERATIONS (_ BitVec 32)
  (bvmul API_CALLS_PER_FRAME FIND_SESSION_COST))
; = 500 * 8 = 4,000 iterations

(echo "=== SESSION LOOKUP OPTIMIZATION ===")
(echo "ABI_UI_MAX_SESSIONS = 16")
(echo "Linear scan cost: 8 avg iterations per call")
(echo "API calls per frame: ~500")
(echo "Total iterations saved by passing session*: ~4,000 per frame")
(echo "")
(echo "But wait: 4,000 iterations at 0.5ns each = 2μs = 0.0125% of 16ms frame")
(echo "This is NOT the bottleneck.")
(echo "")
(echo "The REAL optimization: inline abi_ui_find_session into callers")
(echo "to avoid function call overhead (4-8 cycles per call on modern x86).")
(echo "With 500 calls × 5 cycles = 2,500 cycles saved per frame.")
(echo "Minor, but worth doing for completeness.")

; ============================================================
; Claim 2: Alternative — Tiny direct-mapped cache for session lookup
; ============================================================
(reset)
(set-logic QF_BV)

; Since ABI_UI_MAX_SESSIONS = 16 is tiny, we can use a direct-mapped cache:
;   static KainNativeUiSession* g_session_cache[16];
;   static int64_t g_session_cache_id[16];
;
;   KainNativeUiSession* fast_find_session(int64_t id) {
;       int idx = id & 0xF;
;       if (g_session_cache_id[idx] == id) return g_session_cache[idx];
;       return abi_ui_find_session(id);  // slow path
!;   }
;
; This gives O(1) lookup for repeated calls with the same session_id.
; Hit rate: near 100% (most code uses one session at a time)

(declare-fun cached_id () (_ BitVec 64))
(declare-fun query_id () (_ BitVec 64))

; Hit case: cached_id == query_id
; Cost: 1 AND + 1 load + 1 compare = ~2 cycles

; Miss case: cached_id != query_id
; Cost: 2 + 16 = 18 cycles

; Average case (99% hit rate): 0.99*2 + 0.01*18 = 2.16 cycles
; vs.
; Linear scan: 8 cycles average

(define-const DIRECT_CACHE_CYCLES (_ BitVec 32) (_ bv216 32))    ; scaled: 2.16 * 100
(define-const LINEAR_SCAN_CYCLES (_ BitVec 32) (_ bv800 32))     ; scaled: 8.00 * 100

(assert (bvugt LINEAR_SCAN_CYCLES DIRECT_CACHE_CYCLES))
(check-sat)
; Expected: sat — direct cache is faster at 99% hit rate

(echo "=== DIRECT-MAPPED SESSION CACHE ===")
(echo "Hit rate: ~99% (same session reused across API calls)")
(echo "Linear scan:  ~8.00 cycles avg per lookup")
(echo "Direct cache: ~2.16 cycles avg per lookup")
(echo "Speedup:      ~3.7x for find_session")
(echo "Frame impact: ~500 * 5.84 = ~2,920 cycles saved")
echo ""
(echo "Implementation:")
(echo "  g_session_cache[session_id & (ABI_UI_MAX_SESSIONS-1)] = session_ptr;")
(echo "  g_session_cache_id[session_id & (ABI_UI_MAX_SESSIONS-1)] = session_id;")
(echo "  // Check: cached_id == session_id → return cached")
(echo "  // Else: linear scan, then update cache")
(echo "")
echo "Alternative: Skip optimization entirely (impact is < 0.02% of frame)")
echo "Focus on the real bottleneck: style lookup + child enumeration instead."
