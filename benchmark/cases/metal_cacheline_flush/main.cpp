#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <immintrin.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
constexpr std::int64_t MODULUS = 1'000'000'007LL;
constexpr std::int64_t EXPECTED = 150'626'402LL;
constexpr std::int64_t LINE_WORDS = 8;
constexpr std::int64_t LINE_COUNT = 256;
constexpr std::int64_t ROUNDS = 1024;

std::int64_t metal_word(std::int64_t line, std::int64_t round, std::int64_t salt) {
    const std::int64_t line_term = ((line + 1) * 1'315'423'911LL) % MODULUS;
    const std::int64_t round_term = ((round + 3) * 265'443'576LL) % MODULUS;
    return (line_term + round_term + salt) % MODULUS;
}

std::size_t vm_page_size() {
#if defined(_WIN32)
    SYSTEM_INFO info{};
    ::GetSystemInfo(&info);
    return static_cast<std::size_t>(info.dwPageSize);
#else
    const long page_size = ::sysconf(_SC_PAGESIZE);
    return page_size > 0 ? static_cast<std::size_t>(page_size) : 4096u;
#endif
}

void* vm_map(std::size_t bytes) {
#if defined(_WIN32)
    return ::VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void* mapped = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return mapped == MAP_FAILED ? nullptr : mapped;
#endif
}

int vm_unmap(void* address, std::size_t bytes) {
#if defined(_WIN32)
    return ::VirtualFree(address, 0, MEM_RELEASE) ? 0 : 1;
#else
    return ::munmap(address, bytes) == 0 ? 0 : 1;
#endif
}
}  // namespace

int main() {
    const std::size_t requested_bytes = static_cast<std::size_t>(LINE_COUNT * LINE_WORDS * 8);
    const std::size_t page_bytes = vm_page_size();
    const std::size_t map_bytes = page_bytes > requested_bytes ? page_bytes : requested_bytes;
    void* region = vm_map(map_bytes);
    if (region == nullptr) {
        return 11;
    }

    std::int64_t checksum = 0;
    for (std::int64_t round = 0; round < ROUNDS; ++round) {
        for (std::int64_t line = 0; line < LINE_COUNT; ++line) {
            auto* head = static_cast<std::int64_t*>(region) + (line * LINE_WORDS);
            const auto address_bits = reinterpret_cast<std::uintptr_t>(head);
            auto* alias = reinterpret_cast<volatile std::int64_t*>(address_bits);
            const std::int64_t lane_token = static_cast<std::int64_t>((address_bits >> 6u) & 63u);
            const std::int64_t tagged =
                (metal_word(line, round, checksum) + (line * 17) + round) % MODULUS;
            __builtin_prefetch(reinterpret_cast<const void*>(head), 1, 3);
            *alias = tagged;
            _mm_sfence();
            _mm_clflush(reinterpret_cast<const void*>(head));
            _mm_lfence();
            const std::int64_t seen = *alias;
            checksum = (checksum + seen + lane_token) % MODULUS;
            if ((line & 7) == 0) {
                _mm_mfence();
                _mm_pause();
                _mm_pause();
            }
        }
    }

    const int unmap_status = vm_unmap(region, map_bytes);
    if (unmap_status != 0) {
        return 21;
    }
    return checksum == EXPECTED ? 0 : 31;
}
