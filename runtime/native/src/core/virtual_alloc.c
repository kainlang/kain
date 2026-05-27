#include "../../include/virtual_alloc.h"
#include "../../include/cpu.h"

static int kain_virtual_alignment_is_power_of_two(size_t alignment) {
    return alignment != 0u && (alignment & (alignment - 1u)) == 0u;
}

size_t kain_virtual_page_size(void) {
    int64_t page_size = abi_vm_page_size();
    return page_size > 0 ? (size_t)page_size : 4096u;
}

size_t kain_virtual_align_up(size_t value, size_t alignment) {
    if (alignment == 0u) {
        alignment = 1u;
    }
    if (!kain_virtual_alignment_is_power_of_two(alignment)) {
        return 0u;
    }
    size_t mask = alignment - 1u;
    if (value > SIZE_MAX - mask) {
        return 0u;
    }
    return (value + mask) & ~mask;
}

static size_t kain_virtual_rounded_byte_count(size_t byte_count) {
    return kain_virtual_align_up(byte_count, kain_virtual_page_size());
}

void* kain_virtual_reserve(size_t byte_count, size_t alignment, KainMemType memtype) {
    (void)memtype;
    size_t rounded_byte_count = kain_virtual_rounded_byte_count(byte_count);
    size_t effective_alignment = alignment;
    size_t page_size = kain_virtual_page_size();
    if (rounded_byte_count == 0u) {
        return NULL;
    }
    if (effective_alignment == 0u || effective_alignment < page_size) {
        effective_alignment = page_size;
    }
    if (!kain_virtual_alignment_is_power_of_two(effective_alignment)) {
        return NULL;
    }

    void* base = abi_vm_reserve((int64_t)rounded_byte_count);
    if (base == NULL) {
        return NULL;
    }
    if (effective_alignment > page_size &&
        (((uintptr_t)base) & (uintptr_t)(effective_alignment - 1u)) != 0u) {
        (void)abi_vm_release(base, (int64_t)rounded_byte_count);
        return NULL;
    }
    return base;
}

int kain_virtual_commit(void* base, size_t byte_count, KainMemType memtype) {
    (void)memtype;
    size_t rounded_byte_count = kain_virtual_rounded_byte_count(byte_count);
    if (base == NULL || rounded_byte_count == 0u) {
        return -1;
    }
    return abi_vm_commit(base, (int64_t)rounded_byte_count) == 0 ? 0 : -1;
}

void* kain_virtual_reserve_and_commit(size_t byte_count, size_t alignment, KainMemType memtype) {
    size_t rounded_byte_count = kain_virtual_rounded_byte_count(byte_count);
    void* base = kain_virtual_reserve(byte_count, alignment, memtype);
    if (base == NULL) {
        return NULL;
    }
    if (kain_virtual_commit(base, rounded_byte_count, memtype) != 0) {
        kain_virtual_release(base, rounded_byte_count);
        return NULL;
    }
    return base;
}

void kain_virtual_decommit(void* base, size_t byte_count) {
    size_t rounded_byte_count = kain_virtual_rounded_byte_count(byte_count);
    if (base == NULL || rounded_byte_count == 0u) {
        return;
    }
    (void)abi_vm_decommit(base, (int64_t)rounded_byte_count);
}

void kain_virtual_release(void* base, size_t byte_count) {
    size_t rounded_byte_count = kain_virtual_rounded_byte_count(byte_count);
    if (base == NULL) {
        return;
    }
    (void)abi_vm_release(base, (int64_t)rounded_byte_count);
}

int kain_virtual_batch_map(KainVirtualBatchMapping* mappings, size_t count) {
    if (mappings == NULL && count != 0u) {
        return -1;
    }

    for (size_t index = 0u; index < count; ++index) {
        KainVirtualBatchMapping* mapping = &mappings[index];
        if (mapping->base == NULL || mapping->byte_count == 0u) {
            return -1;
        }
        if (kain_virtual_commit(mapping->base, mapping->byte_count, mapping->memtype) != 0) {
            return -1;
        }
        if (!mapping->writable &&
            abi_vm_protect(
                mapping->base,
                (int64_t)kain_virtual_rounded_byte_count(mapping->byte_count),
                1) != 0) {
            return -1;
        }
    }
    return 0;
}
