#include "dcc_suite_ops.h"

#include <stdio.h>

static char G_DCC_SUITE_SIGNATURE[160];
static char G_DCC_SUITE_REPORT[192];

const char* dcc_suite_sculpt_signature(int grid_resolution, int checksum, int accent) {
    snprintf(
        G_DCC_SUITE_SIGNATURE,
        sizeof(G_DCC_SUITE_SIGNATURE),
        "dcc-suite:gpu-sculpt:grid=%d:checksum=%d:accent=%d",
        grid_resolution,
        checksum,
        accent
    );
    return G_DCC_SUITE_SIGNATURE;
}

const char* dcc_suite_sculpt_report(int grid_resolution, int active_samples, int checksum, int accent) {
    snprintf(
        G_DCC_SUITE_REPORT,
        sizeof(G_DCC_SUITE_REPORT),
        "sculpt-report:grid=%d:active=%d:checksum=%d:accent=%d",
        grid_resolution,
        active_samples,
        checksum,
        accent
    );
    return G_DCC_SUITE_REPORT;
}
