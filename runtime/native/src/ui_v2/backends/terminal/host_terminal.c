// ============================================================================
//  host_terminal.c — ANSI terminal backend for Kaintana
//
//  Renders Kaintana draw commands as ANSI truecolor escape codes to stdout.
//  Works in any ANSI-capable terminal (Windows Terminal, xterm, gnome-terminal,
//  etc.). Input is polled via non-blocking stdin read.
//
//  4-function KaintanaBackendVTable + extended input API:
//    init()            — no-op (no terminal mode to switch)
//    shutdown()        — no-op (no state to restore)
//    new_frame()       — memset framebuffer to zero + poll input
//    render()          — fill framebuffer from cmds[], dump ANSI to stdout
//    term_poll_input() — non-blocking stdin → kt_input_* calls
//
//  USAGE:
//    kt_Session* s = kt_make("term", 80, 24);
//    kt_backend_register(s, "terminal", &kaintana_terminal_backend);
//    kt_backend_select(s, "terminal");
//    kt_terminal_set_session(s);  // enables auto-poll in new_frame()
//    // OR call term_poll_input(s) manually before kt_begin()
//
//  Exported: const KaintanaBackendVTable kaintana_terminal_backend
//            void kt_terminal_set_session(kt_Session*)
//            void term_poll_input(kt_Session*)
// ============================================================================
#include "kaintana.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// Platform detection for non-blocking stdin
#if defined(_WIN32) || defined(_WIN64)
#  define TERM_PLATFORM_WIN32 1
#  include <windows.h>
#else
#  define TERM_PLATFORM_POSIX 1
#  include <unistd.h>
#  include <sys/select.h>
#  include <termios.h>
#endif

// ── Config ──────────────────────────────────────
#define FB_WIDTH  80
#define FB_HEIGHT 24
#define CLIP_DEPTH 16
#define KEY_BUF_SIZE 32
#define ESC_TIMEOUT_MS 10     // ms to wait for full escape sequence

// ANSI escape sequences
#define ANSI_HOME      "\033[H"
#define ANSI_RESET     "\033[0m"
#define ANSI_CURSOR_UP "\033[A"
#define ANSI_SHOW_CURS "\033[?25h"
#define ANSI_HIDE_CURS "\033[?25l"
#define ANSI_ALT_BUF   "\033[?1049h"
#define ANSI_MAIN_BUF  "\033[?1049l"
#define ANSI_MOUSE_ON  "\033[?1000h\033[?1002h\033[?1006h"  // btn events + motion + SGR
#define ANSI_MOUSE_OFF "\033[?1006l\033[?1002l\033[?1000l"
#define ANSI_DSR       "\033[6n"  // Device Status Report: cursor position

// ── Terminal key codes (extended beyond ASCII) ──
#define KT_KEY_TAB       9
#define KT_KEY_ENTER     13
#define KT_KEY_ESCAPE    27
#define KT_KEY_BACKSPACE 127
#define KT_KEY_UP        256
#define KT_KEY_DOWN      257
#define KT_KEY_RIGHT     258
#define KT_KEY_LEFT      259
#define KT_KEY_DELETE    260
#define KT_KEY_HOME      261
#define KT_KEY_END       262
#define KT_KEY_PGUP      263
#define KT_KEY_PGDN      264
#define KT_KEY_INSERT    265
#define KT_KEY_F1        266
#define KT_KEY_F2        267
#define KT_KEY_F3        268
#define KT_KEY_F4        269
#define KT_KEY_F5        270
#define KT_KEY_F6        271
#define KT_KEY_F7        272
#define KT_KEY_F8        273
#define KT_KEY_F9        274
#define KT_KEY_F10       275
#define KT_KEY_F11       276
#define KT_KEY_F12       277
#define KT_KEY_CTRL_C    3
#define KT_KEY_CTRL_D    4
#define KT_KEY_CTRL_Z    26

// ── Static state ────────────────────────────────
static uint32_t fb[FB_WIDTH * FB_HEIGHT];
static kt_Rect clip_stack[CLIP_DEPTH];
static int clip_depth = 0;
static kt_Rect current_clip;
static kt_Session* term_session = NULL;  // set via kt_terminal_set_session()
static int term_init_done = 0;           // 1 after init, for input enable
static float mouse_x = -1.0f, mouse_y = -1.0f;  // last known mouse pos

// ── Helpers ─────────────────────────────────────
static void fill_rect(int x0, int y0, int w, int h, uint32_t color) {
    int x1 = x0 + w; if (x1 > FB_WIDTH)  x1 = FB_WIDTH;
    int y1 = y0 + h; if (y1 > FB_HEIGHT) y1 = FB_HEIGHT;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            fb[y * FB_WIDTH + x] = color;
}

// ── Input: Non-blocking stdin read ──────────────
//  Returns the byte read, or -1 if no input available.
static int stdin_read_byte(void) {
#if TERM_PLATFORM_WIN32
    // Win32: _kbhit + _getch via console API
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) return -1;
    DWORD events = 0;
    GetNumberOfConsoleInputEvents(hIn, &events);
    if (events == 0) return -1;
    INPUT_RECORD buf[1];
    DWORD read = 0;
    ReadConsoleInputA(hIn, buf, 1, &read);
    if (read == 0) return -1;
    if (buf[0].EventType == KEY_EVENT && buf[0].Event.KeyEvent.bKeyDown) {
        KEY_EVENT_RECORD* ke = &buf[0].Event.KeyEvent;
        char ascii = ke->uChar.AsciiChar;
        if (ascii != 0) return (int)(unsigned char)ascii;
        // Virtual key codes for arrows
        switch (ke->wVirtualKeyCode) {
            case VK_UP:     return KT_KEY_UP;
            case VK_DOWN:   return KT_KEY_DOWN;
            case VK_RIGHT:  return KT_KEY_RIGHT;
            case VK_LEFT:   return KT_KEY_LEFT;
            case VK_DELETE: return KT_KEY_DELETE;
            case VK_HOME:   return KT_KEY_HOME;
            case VK_END:    return KT_KEY_END;
            case VK_PRIOR:  return KT_KEY_PGUP;
            case VK_NEXT:   return KT_KEY_PGDN;
            case VK_INSERT: return KT_KEY_INSERT;
            case VK_F1:     return KT_KEY_F1;
            case VK_F2:     return KT_KEY_F2;
            case VK_F3:     return KT_KEY_F3;
            case VK_F4:     return KT_KEY_F4;
            case VK_F5:     return KT_KEY_F5;
            case VK_F6:     return KT_KEY_F6;
            case VK_F7:     return KT_KEY_F7;
            case VK_F8:     return KT_KEY_F8;
            case VK_F9:     return KT_KEY_F9;
            case VK_F10:    return KT_KEY_F10;
            case VK_F11:    return KT_KEY_F11;
            case VK_F12:    return KT_KEY_F12;
            default: return -1;
        }
    }
    return -1;
#else
    // POSIX: select(0) for non-blocking check, then read(0)
    // NOTE: Terminal should be set to raw mode externally or via the
    // consuming application. We don't call tcgetattr/tcsetattr here
    // to keep the file self-contained. The select() timeout=0 gives
    // non-blocking behavior.
    struct timeval tv = { 0, 0 };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (ret <= 0) return -1;
    unsigned char c = 0;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    return (int)c;
#endif
}

// ── Input: Parse ANSI escape sequence ─────────────
//  Reads subsequent bytes after ESC to build a key code.
//  Returns the translated key code or -1 for unknown sequences.
static int read_escape_sequence(void) {
    int c = stdin_read_byte();
    if (c < 0) return KT_KEY_ESCAPE;  // standalone ESC

    // CSI sequences: ESC [ ...
    if (c == '[') {
        c = stdin_read_byte();
        if (c < 0) return KT_KEY_ESCAPE;

        // Arrow keys: [A, [B, [C, [D
        if (c >= 'A' && c <= 'D') {
            switch (c) {
                case 'A': return KT_KEY_UP;
                case 'B': return KT_KEY_DOWN;
                case 'C': return KT_KEY_RIGHT;
                case 'D': return KT_KEY_LEFT;
            }
        }
        // Home/End: [H, [F
        if (c == 'H') return KT_KEY_HOME;
        if (c == 'F') return KT_KEY_END;

        // F-keys with ~: [1~ .. [12~  or [15~ [17~ etc.
        // We just read digits up to ~
        int fn = 0;
        while (c >= '0' && c <= '9') {
            fn = fn * 10 + (c - '0');
            c = stdin_read_byte();
            if (c < 0) return KT_KEY_ESCAPE;
        }
        if (c == '~') {
            switch (fn) {
                case 1: case 7: return KT_KEY_HOME;
                case 4: case 8: return KT_KEY_END;
                case 2: return KT_KEY_INSERT;
                case 3: return KT_KEY_DELETE;
                case 5: return KT_KEY_PGUP;
                case 6: return KT_KEY_PGDN;
                case 11: return KT_KEY_F1;
                case 12: return KT_KEY_F2;
                case 13: return KT_KEY_F3;
                case 14: return KT_KEY_F4;
                case 15: return KT_KEY_F5;
                case 17: return KT_KEY_F6;
                case 18: return KT_KEY_F7;
                case 19: return KT_KEY_F8;
                case 20: return KT_KEY_F9;
                case 21: return KT_KEY_F10;
                case 23: return KT_KEY_F11;
                case 24: return KT_KEY_F12;
                default: return -1;
            }
        }
        if (c == 'M') {
            // SGR mouse: [<...M or [M (old X10)
            // Not fully parsed; treated as unknown
            return -1;
        }
        // Mouse SGR release: [<...m or [m
        if (c == 'm') return -1;
        return -1;
    }

    // OSC sequences: ESC ]
    if (c == ']') {
        // Consume up to BEL (\a) or ST (\033\\)
        while (1) {
            c = stdin_read_byte();
            if (c < 0 || c == '\a') return -1;
            if (c == '\033') {
                int c2 = stdin_read_byte();
                if (c2 == '\\') return -1;
            }
        }
    }

    return -1;
}

// ── Input: Convert a key code to printable text ────
//  Writes up to 4 UTF-8 bytes representing the key (for text input).
//  Returns number of bytes written (0 = no text).
static int key_to_text(int key, char* buf, int buf_size) {
    if (buf_size < 5) return 0;
    if (key >= 32 && key <= 126) {
        // Printable ASCII
        buf[0] = (char)key;
        buf[1] = '\0';
        return 1;
    }
    if (key == KT_KEY_TAB) {
        buf[0] = '\t'; buf[1] = '\0';
        return 1;
    }
    if (key == KT_KEY_ENTER) {
        buf[0] = '\n'; buf[1] = '\0';
        return 1;
    }
    return 0;  // Not printable
}

// ============================================================================
//  term_poll_input — Non-blocking stdin → kt_input_* funnel
//
//  Reads all available bytes from stdin, parses escape sequences, and
//  dispatches to kt_input_mouse_move/key_down/key_up/text.
//  Call this before kt_begin() each frame.
//
//  If session is NULL, returns immediately.
// ============================================================================
void term_poll_input(kt_Session* s) {
    if (!s) return;
    if (!term_init_done) return;

    int key_buf[KEY_BUF_SIZE];
    int key_count = 0;

    // Drain all available input (non-blocking)
    while (key_count < KEY_BUF_SIZE) {
        int c = stdin_read_byte();
        if (c < 0) break;  // No more input

        // Escape sequence
        if (c == 27) {  // ESC
            int seq_key = read_escape_sequence();
            if (seq_key >= 0) {
                if (key_count < KEY_BUF_SIZE)
                    key_buf[key_count++] = seq_key;
            }
        } else {
            if (key_count < KEY_BUF_SIZE)
                key_buf[key_count++] = c;
        }
    }

    // Process each key event
    for (int i = 0; i < key_count; i++) {
        int key = key_buf[i];

        // Handle text input (printable keys)
        char text_buf[5];
        int text_len = key_to_text(key, text_buf, sizeof(text_buf));
        if (text_len > 0) {
            kt_input_key_down(s, key);
            kt_input_key_up(s, key);
            kt_input_text(s, text_buf);
            continue;
        }

        // Arrow keys and navigation
        switch (key) {
            case KT_KEY_UP:
            case KT_KEY_DOWN:
            case KT_KEY_LEFT:
            case KT_KEY_RIGHT:
            case KT_KEY_HOME:
            case KT_KEY_END:
            case KT_KEY_PGUP:
            case KT_KEY_PGDN:
            case KT_KEY_DELETE:
            case KT_KEY_INSERT:
            case KT_KEY_ENTER:
            case KT_KEY_TAB:
            case KT_KEY_BACKSPACE:
            case KT_KEY_ESCAPE:
                kt_input_key_down(s, key);
                kt_input_key_up(s, key);
                break;

            // Control codes for exit
            case KT_KEY_CTRL_C:
            case KT_KEY_CTRL_D:
            case KT_KEY_CTRL_Z:
                kt_input_key_down(s, key);
                kt_input_key_up(s, key);
                break;

            default:
                // Unrecognized — skip
                break;
        }
    }
}

// ============================================================================
//  kt_terminal_set_session — Set the session for auto-polling
//
//  When set, term_new_frame() automatically calls term_poll_input().
//  Optional — the application can call term_poll_input() directly instead.
// ============================================================================
void kt_terminal_set_session(kt_Session* s) {
    term_session = s;
}

// ============================================================================
//  kt_terminal_mouse_move — Set terminal mouse position from ANSI DSR
//
//  Not intended for direct use; tracks internal cursor position for
//  mouse input simulation.
// ============================================================================
void kt_terminal_mouse_move(kt_Session* s, float x, float y) {
    if (!s) return;
    mouse_x = x;
    mouse_y = y;
    kt_input_mouse_move(s, x, y);
}

// ── 4-function backend vtable ───────────────────
static int term_init(const KaintanaBackendConfig* config) {
    // Store session pointer from config (set by kt_backend_select)
    // NOTE: term_session may also be set later via kt_terminal_set_session()
    term_session = (kt_Session*)config->platform_handle;
    memset(fb, 0, sizeof(fb));
    clip_depth = 0;
    current_clip.x = 0; current_clip.y = 0;
    current_clip.w = FB_WIDTH; current_clip.h = FB_HEIGHT;

    // Enable alternate buffer + hide cursor + enable mouse
    fputs(ANSI_HIDE_CURS, stdout);
    fputs(ANSI_ALT_BUF, stdout);
    fputs(ANSI_MOUSE_ON, stdout);
    fputs(ANSI_HOME, stdout);
    fflush(stdout);

    term_init_done = 1;

    // Report DPI scale to core (always 1.0 for terminal cell grid)
    if (term_session) {
        kt_set_native_scale(term_session, 1.0f, 1.0f);
    }
    return 0;
}

static void term_shutdown(void) {
    // Restore terminal state
    fputs(ANSI_MOUSE_OFF, stdout);
    fputs(ANSI_MAIN_BUF, stdout);
    fputs(ANSI_SHOW_CURS, stdout);
    fflush(stdout);
    term_init_done = 0;
    term_session = NULL;
}

static void term_new_frame(void) {
    memset(fb, 0, sizeof(fb));
    clip_depth = 0;
    current_clip.x = 0; current_clip.y = 0;
    current_clip.w = FB_WIDTH; current_clip.h = FB_HEIGHT;

    // Auto-poll input if session is set
    if (term_session) {
        term_poll_input(term_session);
    }
}

static void term_render(const kt_DrawData* draw_data) {
    if (!draw_data) return;

    // Phase 1: fill framebuffer from draw commands
    for (int i = 0; i < draw_data->cmd_count; i++) {
        kt_Cmd cmd = draw_data->cmds[i];
        switch (cmd.type) {
            case KT_CMD_FILL:
                fill_rect((int)cmd.bounds.x, (int)cmd.bounds.y,
                          (int)cmd.bounds.w, (int)cmd.bounds.h, cmd.color);
                break;
            case KT_CMD_CLIP: {
                if (clip_depth >= CLIP_DEPTH) break;
                clip_stack[clip_depth++] = current_clip;
                float r2 = fminf(current_clip.x + current_clip.w, cmd.bounds.x + cmd.bounds.w);
                float b2 = fminf(current_clip.y + current_clip.h, cmd.bounds.y + cmd.bounds.h);
                current_clip.x = fmaxf(current_clip.x, cmd.bounds.x);
                current_clip.y = fmaxf(current_clip.y, cmd.bounds.y);
                current_clip.w = fmaxf(r2 - current_clip.x, 0);
                current_clip.h = fmaxf(b2 - current_clip.y, 0);
                break;
            }
            case KT_CMD_UNCLIP:
                if (clip_depth > 0) current_clip = clip_stack[--clip_depth];
                break;
            default: break;
        }
    }

    // Phase 2: dump framebuffer as ANSI truecolor
    fputs(ANSI_HOME, stdout);  // home cursor
    for (int y = 0; y < FB_HEIGHT; y++) {
        printf("\033[%dH", y + 1);  // per-row cursor (no auto-wrap bug)
        for (int x = 0; x < FB_WIDTH; x++) {
            uint32_t c = fb[y * FB_WIDTH + x];
            if (c == 0) {
                putchar(' ');
            } else {
                printf("\033[48;2;%d;%d;%dm ", (int)((c>>16)&0xFF), (int)((c>>8)&0xFF), (int)(c&0xFF));
                putchar(' ');
                fputs(ANSI_RESET, stdout);
            }
        }
    }
    fflush(stdout);
}

// ── Exported vtable ─────────────────────────────
const KaintanaBackendVTable kaintana_terminal_backend = {
    .init = term_init,
    .shutdown = term_shutdown,
    .new_frame = term_new_frame,
    .render = term_render
};
