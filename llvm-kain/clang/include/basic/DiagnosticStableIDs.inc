// Stub: DiagnosticStableIDs.inc - minimized for C-only clang

// When GET_DIAG_STABLE_ID_ARRAYS is defined, this provides the stable ID tables.
// DiagStableIDs is a StringRef-compatible array of stable diagnostic IDs.
// DiagLegacyStableIDs indexes into DiagStableIDs for backward compatibility.
// Minimal stub with empty sentinel entries.
#ifdef GET_DIAG_STABLE_ID_ARRAYS
constexpr const char *DiagStableIDs[] = { "" };
constexpr unsigned DiagLegacyStableIDs[] = { 0 };
#endif
