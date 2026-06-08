/* Self-contained test for memory.h inline functions — no source deps */
#include "memory.h"
#include <errno.h>

static unsigned char g_buf[256];

void check_header_set_fields(void) {
    KainAllocHeader hdr;
    uint16_t slot_token;
    uint8_t  arena_id;
    uint8_t  memtype;
    uint8_t  flags;
    __CPROVER_havoc_object(&hdr);
    __CPROVER_havoc_object(&slot_token);
    __CPROVER_havoc_object(&arena_id);
    __CPROVER_havoc_object(&memtype);
    __CPROVER_havoc_object(&flags);

    __CPROVER_assume(arena_id < KAIN_ARENA_MAX);
    __CPROVER_assume(memtype < KAIN_MEMTYPE_COUNT);
    __CPROVER_assume(
        (flags & ~(KAIN_ALLOC_HEADER_FLAG_VIRTUAL |
                   KAIN_ALLOC_HEADER_FLAG_CACHED |
                   KAIN_ALLOC_HEADER_FLAG_ZEROED)) == 0u);

    __kain_alloc_header_set_fields(&hdr, slot_token, arena_id, memtype, flags);

    __CPROVER_assert(__kain_alloc_header_slot_token(&hdr) == slot_token,
        "slot_token roundtrip");
    __CPROVER_assert(__kain_alloc_header_arena_id(&hdr) == arena_id,
        "arena_id roundtrip");
    __CPROVER_assert(__kain_alloc_header_memtype(&hdr) == memtype,
        "memtype roundtrip");
    __CPROVER_assert(__kain_alloc_header_flags(&hdr) == flags,
        "flags roundtrip");
}

void check_header_set_magic_and_slot(void) {
    KainAllocHeader hdr;
    uint16_t slot_token;
    __CPROVER_havoc_object(&hdr);
    __CPROVER_havoc_object(&slot_token);

    __kain_alloc_header_set_magic_and_slot(&hdr, slot_token);

    __CPROVER_assert(__kain_alloc_header_slot_token(&hdr) == slot_token,
        "magic_and_slot: slot_token roundtrip");
    __CPROVER_assert(__kain_alloc_header_arena_id(&hdr) == KAIN_ARENA_MAIN,
        "magic_and_slot: arena_id defaults to MAIN");
    __CPROVER_assert(__kain_alloc_header_memtype(&hdr) == KAIN_MEMTYPE_DEFAULT,
        "magic_and_slot: memtype defaults to DEFAULT");
    __CPROVER_assert(__kain_alloc_header_flags(&hdr) == 0u,
        "magic_and_slot: flags defaults to 0");
}

void check_header_validity(void) {
    KainAllocHeader hdr;
    uint16_t slot_token;
    __CPROVER_havoc_object(&hdr);
    __CPROVER_havoc_object(&slot_token);
    __CPROVER_assume(slot_token != 0u);

    __kain_alloc_header_set_magic_and_slot(&hdr, slot_token);
    __CPROVER_assert(__kain_alloc_header_is_valid(&hdr) != 0,
        "valid: returns non-zero for valid header");

    /* Invalid magic */
    __CPROVER_assume(
        (hdr.metadata.magic_and_slot & KAIN_ALLOC_HEADER_MAGIC_MASK)
        != KAIN_ALLOC_HEADER_MAGIC_TAG);
    __CPROVER_assert(__kain_alloc_header_is_valid(&hdr) == 0,
        "valid: returns 0 for mismatched magic");
}

void check_header_null(void) {
    __CPROVER_assert(__kain_alloc_header_slot_token(NULL) == 0u,
        "slot_token(NULL) returns 0");
    __CPROVER_assert(__kain_alloc_header_arena_id(NULL) == KAIN_ARENA_MAIN,
        "arena_id(NULL) returns MAIN");
    __CPROVER_assert(__kain_alloc_header_memtype(NULL) == KAIN_MEMTYPE_DEFAULT,
        "memtype(NULL) returns DEFAULT");
    __CPROVER_assert(__kain_alloc_header_flags(NULL) == 0u,
        "flags(NULL) returns 0");
    __CPROVER_assert(__kain_alloc_header_is_valid(NULL) == 0,
        "is_valid(NULL) returns 0");
}

void check_header_payload_roundtrip(void) {
    KainAllocHeader hdr;
    __CPROVER_havoc_object(&hdr);

    void* payload = __kain_alloc_payload_from_header(&hdr);
    KainAllocHeader* back = __kain_alloc_header_from_payload(payload);
    __CPROVER_assert(back == &hdr,
        "header->payload->header roundtrip: identity");
    __CPROVER_assert(
        (size_t)((unsigned char*)payload - (unsigned char*)&hdr) == sizeof(KainAllocHeader),
        "payload offset == sizeof(KainAllocHeader)");
}

void check_header_has_flag(void) {
    KainAllocHeader hdr;
    __CPROVER_havoc_object(&hdr);

    __kain_alloc_header_set_fields(&hdr, 1, KAIN_ARENA_MAIN,
        KAIN_MEMTYPE_DEFAULT, KAIN_ALLOC_HEADER_FLAG_VIRTUAL);

    __CPROVER_assert(__kain_alloc_header_has_flag(&hdr, KAIN_ALLOC_HEADER_FLAG_VIRTUAL) != 0,
        "has_flag(VIRTUAL) returns non-zero");
    __CPROVER_assert(__kain_alloc_header_has_flag(&hdr, KAIN_ALLOC_HEADER_FLAG_CACHED) == 0,
        "has_flag(CACHED) returns 0 when not set");
}

void check_header_magic_fields(void) {
    uint16_t slot_token;
    uint8_t arena_id;
    uint8_t memtype;
    uint8_t flags;
    __CPROVER_havoc_object(&slot_token);
    __CPROVER_havoc_object(&arena_id);
    __CPROVER_havoc_object(&memtype);
    __CPROVER_havoc_object(&flags);
    __CPROVER_assume(arena_id < KAIN_ARENA_MAX);
    __CPROVER_assume(memtype < KAIN_MEMTYPE_COUNT);
    __CPROVER_assume(
        (flags & ~(KAIN_ALLOC_HEADER_FLAG_VIRTUAL |
                   KAIN_ALLOC_HEADER_FLAG_CACHED |
                   KAIN_ALLOC_HEADER_FLAG_ZEROED)) == 0u);

    uint64_t magic = __kain_alloc_header_magic_with_fields(
        slot_token, arena_id, memtype, flags);

    /* Magic tag must be present */
    __CPROVER_assert(
        (magic & KAIN_ALLOC_HEADER_MAGIC_MASK) == KAIN_ALLOC_HEADER_MAGIC_TAG,
        "magic_with_fields: has magic tag");

    /* Fields must be extractable */
    KainAllocHeader hdr;
    hdr.metadata.magic_and_slot = magic;
    __CPROVER_assert(__kain_alloc_header_slot_token(&hdr) == slot_token,
        "magic_with_fields: slot_token extractable");
    __CPROVER_assert(__kain_alloc_header_arena_id(&hdr) == arena_id,
        "magic_with_fields: arena_id extractable");
    __CPROVER_assert(__kain_alloc_header_memtype(&hdr) == memtype,
        "magic_with_fields: memtype extractable");
    __CPROVER_assert(__kain_alloc_header_flags(&hdr) == flags,
        "magic_with_fields: flags extractable");
}

int main(void) {
    check_header_set_fields();
    check_header_set_magic_and_slot();
    check_header_validity();
    check_header_null();
    check_header_payload_roundtrip();
    check_header_has_flag();
    check_header_magic_fields();
    return 0;
}
