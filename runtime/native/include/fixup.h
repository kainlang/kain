#ifndef FIXUP_H
#define FIXUP_H

#include <stddef.h>
#include <stdint.h>

#include "handle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    KainRuntimeHandle handle;
    void* base;
    size_t size;
} KainFixupTrackedView;

void kain_fixup_init(void);

KainRuntimeHandle kain_fixup_track_allocation(void* base, size_t size);
KainRuntimeHandle kain_fixup_handle_for_pointer(const void* ptr);
void* kain_fixup_resolve_handle(KainRuntimeHandle handle);
size_t kain_fixup_handle_size(KainRuntimeHandle handle);

int kain_fixup_view(KainRuntimeHandle handle, KainFixupTrackedView* out_view);
int kain_fixup_register_known_ref(void** location);
int kain_fixup_unregister_known_ref(void** location);
int kain_fixup_update_known_ref(void** location, void* value);

int kain_fixup_relocate_allocation(
    KainRuntimeHandle handle,
    void* old_base,
    void* new_base,
    size_t size
);

int kain_fixup_unregister_allocation(KainRuntimeHandle handle);

uint64_t kain_fixup_known_ref_count(void);
uint64_t kain_fixup_relocation_count(void);
KainRuntimeHandle kain_fixup_last_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* FIXUP_H */
