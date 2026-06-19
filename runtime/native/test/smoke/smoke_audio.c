// Smoke test: audio_system ABI surface
//
// Verifies that the audio_system header compiles, links against the
// runtime, and the public API is callable. Exercises only the platform-
// agnostic paths (enumeration, diagnostics, info accessors) and uses
// null/invalid arguments so it works on every platform without requiring
// real audio hardware.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "audio_system.h"

static void test_enumeration_paths(void) {
    int64_t count = abi_audio_device_count();
    printf("  device count: %lld\n", (long long)count);
    /* count must be >= 0 even on platforms without devices. */
    assert(count >= 0);

    int64_t written = abi_audio_enumerate_devices(NULL, 0);
    printf("  enumerate with null buffer -> %lld (expected error)\n", (long long)written);
    assert(written <= 0);
    assert(abi_audio_last_status() != ABI_AUDIO_OK);

    KainNativeAudioDeviceInfo info;
    memset(&info, 0, sizeof(info));
    int64_t rc = abi_audio_default_output_device(NULL);
    printf("  default with null out -> %lld (expected error)\n", (long long)rc);
    assert(rc <= 0);
    assert(abi_audio_last_status() != ABI_AUDIO_OK);
    (void)info;
}

static void test_stream_info_invalid(void) {
    int32_t out = -1;
    int64_t rc;

    rc = abi_audio_stream_is_running(NULL, &out);
    printf("  stream_is_running(null) -> %lld (expected error)\n", (long long)rc);
    assert(rc < 0);

    rc = abi_audio_stream_sample_rate(NULL, &out);
    printf("  stream_sample_rate(null) -> %lld (expected error)\n", (long long)rc);
    assert(rc < 0);

    rc = abi_audio_stream_buffer_size(NULL, &out);
    printf("  stream_buffer_size(null) -> %lld (expected error)\n", (long long)rc);
    assert(rc < 0);

    rc = abi_audio_stream_channels(NULL, &out);
    printf("  stream_channels(null) -> %lld (expected error)\n", (long long)rc);
    assert(rc < 0);

    double load = 0.0;
    rc = abi_audio_stream_cpu_load(NULL, &load);
    printf("  stream_cpu_load(null) -> %lld (expected error)\n", (long long)rc);
    assert(rc < 0);
}

static void test_midi_paths(void) {
    int64_t count = abi_audio_midi_device_count();
    printf("  midi device count: %lld\n", (long long)count);
    assert(count >= 0);

    char name[ABI_AUDIO_MAX_MIDI_NAME] = {0};
    int64_t rc = abi_audio_midi_device_name(-1, name, sizeof(name));
    printf("  midi name for invalid id -> %lld (expected error or fill)\n", (long long)rc);
    /* Either no devices (stub) or invalid id rejected — both are valid. */

    rc = abi_audio_midi_open_input(-1, NULL, NULL, NULL);
    printf("  midi open with null callback -> %lld (expected error)\n", (long long)rc);
    assert(rc < 0);

    rc = abi_audio_midi_close_input(-1);
    printf("  midi close invalid handle -> %lld (expected error)\n", (long long)rc);
    assert(rc < 0);
}

static void test_stream_open_paths(void) {
    KainNativeAudioStream* s = NULL;
    int64_t rc = abi_audio_stream_open(0, 48000, 256, 2, 0, NULL, NULL, &s);
    printf("  stream_open with null callback -> %lld (expected error)\n", (long long)rc);
    assert(rc < 0);
    (void)s;
}

int main(void) {
    printf("=== smoke_audio ===\n");

    test_enumeration_paths();
    test_stream_info_invalid();
    test_midi_paths();
    test_stream_open_paths();

    /* Diagnostics — status reflects the last failure. */
    int64_t s = abi_audio_last_status();
    const char* kind = abi_audio_last_error_kind();
    const char* msg = abi_audio_last_error_message();
    printf("  last status: %lld, kind: %s, message: %s\n",
           (long long)s, kind ? kind : "(null)", msg ? msg : "(null)");

    /* On the stub build, kind should be one of the failure labels. */
#ifdef _WIN32
    /* On Windows real audio devices may exist; we don't assert specifics. */
    printf("  platform: windows\n");
#elif defined(__APPLE__)
    printf("  platform: macos\n");
#elif defined(__linux__)
    printf("  platform: linux\n");
#else
    printf("  platform: stub\n");
    assert(strcmp(kind, "no_device") == 0
        || strcmp(kind, "no_midi") == 0
        || strcmp(kind, "invalid_arg") == 0
        || strcmp(kind, "invalid_handle") == 0);
#endif

    printf("\nsmoke_audio: PASS\n");
    return 0;
}
