#include <array>
#include <cstdint>
#include <vector>

namespace {

constexpr std::int64_t kModulus = 1'000'000'007LL;
constexpr std::int64_t kRounds = 60'000;
constexpr std::int64_t kCellCount = 64;
constexpr std::int64_t kExpected = 237804827;

struct Packet {
    std::int64_t bias;
    std::int64_t phase;
    std::int64_t salt;
    bool hot;
};

struct Authority {
    std::int64_t signal = 1;
    std::int64_t epoch = 0;
    std::int64_t ledger = 0;
};

struct Mirror {
    std::int64_t signal_copy = 1;
    std::int64_t epoch_copy = 0;
    std::int64_t ledger_copy = 0;
};

struct Relay {
    std::int64_t bias = 11;
    std::int64_t turns = 0;

    std::int64_t fold(std::int64_t request) {
        turns += 1;
        return ((request * 17) + bias + 29) % kModulus;
    }
};

std::int64_t fabric_mix(std::int64_t value) {
    return ((value * 31) + 7) % kModulus;
}

std::int64_t fabric_stage(std::int64_t value) {
    return (value + 19) % kModulus;
}

std::int64_t fabric_pipeline(std::int64_t value) {
    return fabric_stage(fabric_mix(value));
}

bool fabric_in_bounds(std::int64_t value) {
    return value >= 0 && value < kModulus;
}

std::int64_t packet_branch(const Packet& packet, std::int64_t lane) {
    if (packet.hot) {
        return packet.phase + packet.salt + lane;
    }
    return packet.salt + lane + 3;
}

std::int64_t commit_fabric(Authority& authority, Mirror& mirror, std::int64_t value, std::int64_t ledger_delta) {
    authority.signal = value;
    authority.epoch += 1;
    authority.ledger = (authority.ledger + ledger_delta + authority.epoch + 13) % kModulus;
    mirror.signal_copy = authority.signal;
    mirror.epoch_copy = authority.epoch;
    mirror.ledger_copy = authority.ledger;
    return authority.signal;
}

std::int64_t fold_cells(const std::vector<std::int64_t>& cells) {
    std::int64_t acc = 0;
    for (std::int64_t value : cells) {
        acc = (acc + value) % kModulus;
    }
    return acc;
}

}  // namespace

int main() {
    Authority authority{};
    Mirror mirror{};
    Relay relay{};
    const std::int64_t warm = relay.fold(0);
    (void)warm;
    const std::array<Packet, 6> packets = {{
        {5, 7, 19, true},
        {11, 13, 23, false},
        {17, 19, 29, true},
        {23, 31, 37, true},
        {29, 41, 43, false},
        {37, 47, 53, true},
    }};
    std::vector<std::int64_t> cells(static_cast<std::size_t>(kCellCount), 0);

    std::int64_t checksum = 0;
    for (std::int64_t i = 0; i < kRounds; ++i) {
        const std::int64_t lane = i % static_cast<std::int64_t>(packets.size());
        const std::int64_t slot = ((i * 3) + lane) % kCellCount;
        const Packet moved = packets[static_cast<std::size_t>(lane)];
        const std::int64_t old_cell = cells[static_cast<std::size_t>(slot)];
        const std::int64_t mixed_input = (checksum + old_cell + moved.bias + moved.phase + i) % kModulus;
        const std::int64_t staged = fabric_pipeline(mixed_input);
        const std::int64_t committed = commit_fabric(authority, mirror, staged, moved.salt + lane);
        const std::int64_t legal = fabric_in_bounds(committed) ? 0 : -1;
        const std::int64_t request =
            (committed + old_cell + mirror.ledger_copy + packet_branch(moved, lane) + legal) % kModulus;
        const std::int64_t reply = relay.fold(request);
        const std::int64_t next_cell =
            (reply + mirror.signal_copy + mirror.epoch_copy + mirror.ledger_copy + slot) % kModulus;
        cells[static_cast<std::size_t>(slot)] = next_cell;
        checksum = (checksum + next_cell + moved.phase + legal) % kModulus;
    }

    const std::int64_t observed = fold_cells(cells);
    const std::int64_t final_score =
        (checksum + observed + mirror.signal_copy + mirror.epoch_copy + mirror.ledger_copy) % kModulus;
    return final_score == kExpected ? 0 : 1;
}
