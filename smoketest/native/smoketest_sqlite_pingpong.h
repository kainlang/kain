#ifndef SMOKETEST_SQLITE_PINGPONG_H
#define SMOKETEST_SQLITE_PINGPONG_H

#if defined(_WIN32)
#define SMOKETEST_SQLITE_EXPORT __declspec(dllexport)
#else
#define SMOKETEST_SQLITE_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Kain imports this header with:
 *
 *   include ../../native/smoketest_sqlite_pingpong.h as ping
 *
 * The include lane keeps `ping` as alias provenance, discovers the sibling
 * `smoketest_sqlite_pingpong.c` translation unit, and exposes a natural
 * `ping_*` surface to Kain while we keep the full SQLite contract in C.
 */

SMOKETEST_SQLITE_EXPORT int smoketest_sqlite_pingpong_score(int seed, int rounds);
SMOKETEST_SQLITE_EXPORT int smoketest_sqlite_pingpong_row_count(int seed, int rounds);
SMOKETEST_SQLITE_EXPORT int smoketest_sqlite_pingpong_tail_value(int seed, int rounds);
SMOKETEST_SQLITE_EXPORT int smoketest_sqlite_pingpong_text_bytes(int seed, int rounds);
SMOKETEST_SQLITE_EXPORT int smoketest_sqlite_pingpong_total_changes(int seed, int rounds);
SMOKETEST_SQLITE_EXPORT int smoketest_sqlite_pingpong_bounce(int seed, int rounds);
SMOKETEST_SQLITE_EXPORT const char* smoketest_sqlite_pingpong_signature(int seed, int rounds);
SMOKETEST_SQLITE_EXPORT _Bool smoketest_sqlite_pingpong_hot(int seed, int rounds);

#ifdef __cplusplus
}
#endif

#endif
