// Companion C source for win32_shim.h
// Symbols (MessageBoxA, CreateWindowExA, etc.) resolve from user32.dll
// at link time.  This file exists solely to satisfy the inline tier.
// clang-cl on Windows supports #pragma comment(lib, ...)
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
