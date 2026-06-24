// pi_con.h — Minimal Win32 console declarations for UTF-8 init
// These are resolved from kernel32.lib at link time
void SetConsoleOutputCP(unsigned int cp);
void SetConsoleCP(unsigned int cp);
void* GetStdHandle(unsigned long nStdHandle);
int SetConsoleMode(void* hConsole, unsigned long dwMode);
