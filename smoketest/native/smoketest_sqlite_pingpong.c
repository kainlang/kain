/*
 * This translation unit is the SQLite tennis ball for smoketest:
 *
 * - Kain owns the album flow and imports `smoketest_sqlite_pingpong.h`
 *   through `include ... as ping`.
 * - SQLite owns the real SQL work inside an in-memory rally table.
 * - The bridge returns compact scalar probes plus one signature string so the
 *   album can bounce the same native state through both low-level and
 *   higher-level Kain tracks without giant glue layers.
 */
#include "smoketest_sqlite_pingpong.h"

#include "sqlite3.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    SMOKETEST_SQLITE_MODULUS = 1000000007,
    SMOKETEST_SQLITE_MAX_ROUNDS = 24,
    SMOKETEST_SQLITE_MIN_ROUNDS = 4,
    SMOKETEST_SQLITE_NOTE_CAP = 96,
    SMOKETEST_SQLITE_SIGNATURE_CAP = 512
};

typedef struct smoke_sqlite_metrics {
    int score;
    int row_count;
    int tail_value;
    int text_bytes;
    int total_changes;
    int bounce;
    bool hot;
    char signature[SMOKETEST_SQLITE_SIGNATURE_CAP];
} smoke_sqlite_metrics;

static int smoke_sqlite_clamp_rounds(int rounds) {
    if (rounds < SMOKETEST_SQLITE_MIN_ROUNDS) {
        return SMOKETEST_SQLITE_MIN_ROUNDS;
    }
    if (rounds > SMOKETEST_SQLITE_MAX_ROUNDS) {
        return SMOKETEST_SQLITE_MAX_ROUNDS;
    }
    return rounds;
}

static int smoke_sqlite_mix(int seed, int round) {
    long long value = (long long)(seed + 19) * (long long)(round + 7) * 37LL;
    value ^= (long long)(seed << ((round % 5) + 1));
    value += (long long)(round * 131) + 17LL;
    value %= SMOKETEST_SQLITE_MODULUS;
    if (value < 0) {
        value += SMOKETEST_SQLITE_MODULUS;
    }
    return (int)value;
}

static void smoke_sqlite_fail(smoke_sqlite_metrics* metrics, int rc, const char* stage) {
    if (!metrics) {
        return;
    }
    metrics->score = -rc;
    metrics->row_count = 0;
    metrics->tail_value = 0;
    metrics->text_bytes = 0;
    metrics->total_changes = 0;
    metrics->bounce = 0;
    metrics->hot = false;
    snprintf(metrics->signature, sizeof(metrics->signature), "sqlite-fail:%s:%d", stage ? stage : "unknown", rc);
}

static int smoke_sqlite_exec(sqlite3* db, const char* sql, smoke_sqlite_metrics* metrics, const char* stage) {
    char* error_message = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error_message);
    if (rc != SQLITE_OK) {
        smoke_sqlite_fail(metrics, rc, stage);
        if (error_message) {
            sqlite3_free(error_message);
        }
        return rc;
    }
    if (error_message) {
        sqlite3_free(error_message);
    }
    return SQLITE_OK;
}

static smoke_sqlite_metrics smoke_sqlite_collect(int seed, int rounds) {
    smoke_sqlite_metrics metrics;
    sqlite3* db = NULL;
    sqlite3_stmt* insert_stmt = NULL;
    sqlite3_stmt* aggregate_stmt = NULL;
    sqlite3_stmt* tail_stmt = NULL;
    sqlite3_stmt* bounce_stmt = NULL;
    char tail_note[SMOKETEST_SQLITE_NOTE_CAP];
    int rc = SQLITE_OK;
    int round = 0;

    memset(&metrics, 0, sizeof(metrics));
    memset(tail_note, 0, sizeof(tail_note));
    snprintf(metrics.signature, sizeof(metrics.signature), "sqlite-uninitialized");
    rounds = smoke_sqlite_clamp_rounds(rounds);

    rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK || !db) {
        smoke_sqlite_fail(&metrics, rc != SQLITE_OK ? rc : SQLITE_NOMEM, "open");
        if (db) {
            sqlite3_close(db);
        }
        return metrics;
    }

    if (smoke_sqlite_exec(db, "PRAGMA temp_store = MEMORY;", &metrics, "pragma.temp_store") != SQLITE_OK) {
        goto cleanup;
    }
    if (smoke_sqlite_exec(db, "PRAGMA synchronous = OFF;", &metrics, "pragma.synchronous") != SQLITE_OK) {
        goto cleanup;
    }
    if (smoke_sqlite_exec(
            db,
            "CREATE TABLE rally (round_id INTEGER PRIMARY KEY, volley INTEGER NOT NULL, note TEXT NOT NULL);",
            &metrics,
            "schema") != SQLITE_OK) {
        goto cleanup;
    }

    rc = sqlite3_prepare_v2(
        db,
        "INSERT INTO rally(round_id, volley, note) VALUES (?, ?, ?);",
        -1,
        &insert_stmt,
        NULL
    );
    if (rc != SQLITE_OK || !insert_stmt) {
        smoke_sqlite_fail(&metrics, rc, "prepare.insert");
        goto cleanup;
    }

    for (round = 0; round < rounds; ++round) {
        char note[SMOKETEST_SQLITE_NOTE_CAP];
        int volley = smoke_sqlite_mix(seed + metrics.bounce + (round * 11), round);
        int round_id = round + 1;

        snprintf(
            note,
            sizeof(note),
            "r%02d-v%05d-s%04d",
            round_id,
            volley % 100000,
            (seed + round * 7) % 10000
        );

        rc = sqlite3_bind_int(insert_stmt, 1, round_id);
        if (rc != SQLITE_OK) {
            smoke_sqlite_fail(&metrics, rc, "bind.round_id");
            goto cleanup;
        }

        rc = sqlite3_bind_int(insert_stmt, 2, volley);
        if (rc != SQLITE_OK) {
            smoke_sqlite_fail(&metrics, rc, "bind.volley");
            goto cleanup;
        }

        rc = sqlite3_bind_text(insert_stmt, 3, note, -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            smoke_sqlite_fail(&metrics, rc, "bind.note");
            goto cleanup;
        }

        rc = sqlite3_step(insert_stmt);
        if (rc != SQLITE_DONE) {
            smoke_sqlite_fail(&metrics, rc, "step.insert");
            goto cleanup;
        }

        metrics.bounce = (metrics.bounce * 37 + round_id * 13 + volley) % SMOKETEST_SQLITE_MODULUS;
        metrics.tail_value = volley;

        rc = sqlite3_reset(insert_stmt);
        if (rc != SQLITE_OK) {
            smoke_sqlite_fail(&metrics, rc, "reset.insert");
            goto cleanup;
        }

        rc = sqlite3_clear_bindings(insert_stmt);
        if (rc != SQLITE_OK) {
            smoke_sqlite_fail(&metrics, rc, "clear.insert");
            goto cleanup;
        }
    }

    metrics.total_changes = sqlite3_total_changes(db);

    rc = sqlite3_prepare_v2(
        db,
        "SELECT COUNT(*), COALESCE(SUM(volley), 0), COALESCE(SUM(length(note)), 0) FROM rally;",
        -1,
        &aggregate_stmt,
        NULL
    );
    if (rc != SQLITE_OK || !aggregate_stmt) {
        smoke_sqlite_fail(&metrics, rc, "prepare.aggregate");
        goto cleanup;
    }

    rc = sqlite3_step(aggregate_stmt);
    if (rc != SQLITE_ROW) {
        smoke_sqlite_fail(&metrics, rc, "step.aggregate");
        goto cleanup;
    }

    metrics.row_count = sqlite3_column_int(aggregate_stmt, 0);
    metrics.score = sqlite3_column_int(aggregate_stmt, 1);
    metrics.text_bytes = sqlite3_column_int(aggregate_stmt, 2);

    rc = sqlite3_prepare_v2(
        db,
        "SELECT note, volley FROM rally ORDER BY round_id DESC LIMIT 1;",
        -1,
        &tail_stmt,
        NULL
    );
    if (rc != SQLITE_OK || !tail_stmt) {
        smoke_sqlite_fail(&metrics, rc, "prepare.tail");
        goto cleanup;
    }

    rc = sqlite3_step(tail_stmt);
    if (rc != SQLITE_ROW) {
        smoke_sqlite_fail(&metrics, rc, "step.tail");
        goto cleanup;
    }

    {
        const unsigned char* text = sqlite3_column_text(tail_stmt, 0);
        int volley = sqlite3_column_int(tail_stmt, 1);
        if (text) {
            snprintf(tail_note, sizeof(tail_note), "%s", (const char*)text);
        }
        metrics.tail_value = volley;
    }

    rc = sqlite3_prepare_v2(
        db,
        "SELECT round_id, volley FROM rally ORDER BY round_id ASC;",
        -1,
        &bounce_stmt,
        NULL
    );
    if (rc != SQLITE_OK || !bounce_stmt) {
        smoke_sqlite_fail(&metrics, rc, "prepare.bounce");
        goto cleanup;
    }

    while ((rc = sqlite3_step(bounce_stmt)) == SQLITE_ROW) {
        int round_id = sqlite3_column_int(bounce_stmt, 0);
        int volley = sqlite3_column_int(bounce_stmt, 1);
        metrics.bounce = (metrics.bounce + round_id * 17 + volley * 3) % SMOKETEST_SQLITE_MODULUS;
    }
    if (rc != SQLITE_DONE) {
        smoke_sqlite_fail(&metrics, rc, "step.bounce");
        goto cleanup;
    }

    metrics.hot = metrics.row_count == rounds
        && metrics.total_changes == rounds
        && metrics.text_bytes > (rounds * 8)
        && metrics.tail_value > 0;

    metrics.score = (int)(
        ((long long)metrics.score * 3LL
        + (long long)metrics.text_bytes * 5LL
        + (long long)metrics.total_changes * 7LL
        + (long long)metrics.bounce
        + sqlite3_libversion_number()) % SMOKETEST_SQLITE_MODULUS
    );

    snprintf(
        metrics.signature,
        sizeof(metrics.signature),
        "sqlite-v%d.rows=%d.tail=%d.bytes=%d.total=%d.bounce=%d.last=%s",
        sqlite3_libversion_number(),
        metrics.row_count,
        metrics.tail_value,
        metrics.text_bytes,
        metrics.total_changes,
        metrics.bounce,
        tail_note[0] ? tail_note : "none"
    );

cleanup:
    if (bounce_stmt) {
        sqlite3_finalize(bounce_stmt);
    }
    if (tail_stmt) {
        sqlite3_finalize(tail_stmt);
    }
    if (aggregate_stmt) {
        sqlite3_finalize(aggregate_stmt);
    }
    if (insert_stmt) {
        sqlite3_finalize(insert_stmt);
    }
    if (db) {
        sqlite3_close(db);
    }
    return metrics;
}

int smoketest_sqlite_pingpong_score(int seed, int rounds) {
    smoke_sqlite_metrics metrics = smoke_sqlite_collect(seed, rounds);
    return metrics.score;
}

int smoketest_sqlite_pingpong_row_count(int seed, int rounds) {
    smoke_sqlite_metrics metrics = smoke_sqlite_collect(seed, rounds);
    return metrics.row_count;
}

int smoketest_sqlite_pingpong_tail_value(int seed, int rounds) {
    smoke_sqlite_metrics metrics = smoke_sqlite_collect(seed, rounds);
    return metrics.tail_value;
}

int smoketest_sqlite_pingpong_text_bytes(int seed, int rounds) {
    smoke_sqlite_metrics metrics = smoke_sqlite_collect(seed, rounds);
    return metrics.text_bytes;
}

int smoketest_sqlite_pingpong_total_changes(int seed, int rounds) {
    smoke_sqlite_metrics metrics = smoke_sqlite_collect(seed, rounds);
    return metrics.total_changes;
}

int smoketest_sqlite_pingpong_bounce(int seed, int rounds) {
    smoke_sqlite_metrics metrics = smoke_sqlite_collect(seed, rounds);
    return metrics.bounce;
}

const char* smoketest_sqlite_pingpong_signature(int seed, int rounds) {
    static char signature[SMOKETEST_SQLITE_SIGNATURE_CAP];
    smoke_sqlite_metrics metrics = smoke_sqlite_collect(seed, rounds);
    snprintf(signature, sizeof(signature), "%s", metrics.signature);
    return signature;
}

_Bool smoketest_sqlite_pingpong_hot(int seed, int rounds) {
    smoke_sqlite_metrics metrics = smoke_sqlite_collect(seed, rounds);
    return metrics.hot ? 1 : 0;
}
