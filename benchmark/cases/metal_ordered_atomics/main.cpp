#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace {
constexpr std::int64_t MODULUS = 1'000'000'007LL;
constexpr std::int64_t EXPECTED = 374'849'045LL;
constexpr std::size_t SLOTS = 64;
constexpr std::int64_t ROUNDS = 1'000'000LL;
constexpr std::int64_t VALUE_MASK = 1'048'575LL;
}  // namespace

int main() {
    auto cells = std::make_unique<std::atomic<std::int64_t>[]>(SLOTS);
    for (std::size_t slot = 0; slot < SLOTS; ++slot) {
        cells[slot].store(((static_cast<std::int64_t>(slot) * 97) + 13) & VALUE_MASK,
                          std::memory_order_release);
    }

    std::int64_t checksum = 0;
    for (std::int64_t i = 0; i < ROUNDS; ++i) {
        const std::size_t slot_index = static_cast<std::size_t>(i & 63LL);
        auto& cell = cells[slot_index];
        const std::int64_t add_prev = cell.fetch_add((i & 7LL) + 1LL, std::memory_order_acq_rel);
        const std::int64_t or_prev =
            cell.fetch_or(((i * 13LL) & 255LL) | 1LL, std::memory_order_acq_rel);
        const std::int64_t xor_prev =
            cell.fetch_xor((i * 17LL) & 1023LL, std::memory_order_acq_rel);
        const std::int64_t and_prev = cell.fetch_and(VALUE_MASK, std::memory_order_acq_rel);
        const std::int64_t current_after_and = and_prev & VALUE_MASK;
        std::int64_t current_state = current_after_and;
        std::int64_t exchange_prev = 0;
        if ((i & 15LL) == 0) {
            const std::int64_t desired = (current_state + static_cast<std::int64_t>(slot_index) + 53LL) & VALUE_MASK;
            exchange_prev = cell.exchange(desired, std::memory_order_acq_rel);
            current_state = desired;
        }
        std::int64_t swapped = 0;
        if ((i & 31LL) == 0) {
            const std::int64_t desired = ((current_state ^ 341LL) + i + 97LL) & VALUE_MASK;
            std::int64_t expected_value = current_state;
            if (cell.compare_exchange_strong(expected_value, desired, std::memory_order_seq_cst,
                                             std::memory_order_seq_cst)) {
                current_state = desired;
                swapped = 1;
            }
        }
        if ((i & 7LL) == 0) {
            std::atomic_thread_fence(std::memory_order_acq_rel);
        }
        const std::int64_t seen = cell.load(std::memory_order_acquire);
        checksum = (checksum + add_prev + or_prev + xor_prev + and_prev + exchange_prev + seen +
                    static_cast<std::int64_t>(slot_index) + swapped) %
                   MODULUS;
    }

    for (std::size_t slot = 0; slot < SLOTS; ++slot) {
        checksum = (checksum + cells[slot].load(std::memory_order_seq_cst)) % MODULUS;
    }

    return checksum == EXPECTED ? 0 : 41;
}
