// Minimal Win32 API declarations — clean C, no __stdcall/declspec/SAL
//
// CRITICAL: Handle/pointer params use 'unsigned long long' NOT 'void*'
// to avoid Kain's tagged-int vs pointer-type confusion in the C-FFI codegen.
// String params stay 'const char*' so Kain auto-converts String to i8*.
// Symbols resolve from user32.dll at link time.

int   MessageBoxA(unsigned long long hwnd, const char* text, const char* caption, unsigned int type);
unsigned long long GetModuleHandleA(const char* module_name);
unsigned long long LoadCursorA(unsigned long long hInstance, const char* lpCursorName);

// --- window class + creation + message pump ---
typedef unsigned short ATOM;

ATOM  RegisterClassA(unsigned long long lpWndClass);
unsigned long long CreateWindowExA(
    unsigned long long dwExStyle,
    const char* lpClassName,
    const char* lpWindowName,
    unsigned long long dwStyle,
    int x, int y, int nWidth, int nHeight,
    unsigned long long hWndParent,
    unsigned long long hMenu,
    unsigned long long hInstance,
    unsigned long long lpParam);
int   ShowWindow(unsigned long long hWnd, int nCmdShow);
int   UpdateWindow(unsigned long long hWnd);
int   PeekMessageA(unsigned long long lpMsg, unsigned long long hWnd,
                    unsigned int wMsgFilterMin, unsigned int wMsgFilterMax,
                    unsigned int wRemoveMsg);
int   TranslateMessage(unsigned long long lpMsg);
unsigned long long DispatchMessageA(unsigned long long lpMsg);
unsigned long long DefWindowProcA(unsigned long long hWnd, unsigned int Msg,
                                    unsigned long long wParam, unsigned long long lParam);
int   DestroyWindow(unsigned long long hWnd);
void  PostQuitMessage(int nExitCode);
