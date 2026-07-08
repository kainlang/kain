// Stub: DiagnosticGroups.inc - minimized for C-only clang

#ifdef GET_CATEGORY_TABLE
CATEGORY("unused", DiagCat_Unused)
CATEGORY("deprecated", DiagCat_Deprecated)
CATEGORY("pedantic", DiagCat_Pedantic)
#endif

#ifdef GET_DIAG_ARRAYS
static constexpr int16_t DiagSubGroups[] = {
  -1  // terminator
};
static constexpr int16_t DiagArrays[] = {
  -1  // terminator for group 0 (unused) — no members
};
static constexpr StringRef DiagGroupNames[] = {
  "unused",
  "deprecated",
  "pedantic",
  "macro-redefined",
  "sign-compare",
  "unused-parameter",
  "dllexport-explicit-instantiation-decl",
  "deprecated-declarations",
};
#endif

#ifdef DIAG_ENTRY
DIAG_ENTRY(unused, 0, 0, 0, "unused")
DIAG_ENTRY(deprecated, 1, 0, 0, "deprecated")
DIAG_ENTRY(pedantic, 2, 0, 0, "pedantic")
DIAG_ENTRY(MacroRedefined, 3, 0, 0, "macro-redefined")
DIAG_ENTRY(SignCompare, 4, 0, 0, "sign-compare")
DIAG_ENTRY(UnusedParameter, 5, 0, 0, "unused-parameter")
DIAG_ENTRY(DllexportExplicitInstantiationDecl, 6, 0, 0, "dllexport-explicit-instantiation-decl")
DIAG_ENTRY(DeprecatedDeclarations, 7, 0, 0, "deprecated-declarations")
#endif
