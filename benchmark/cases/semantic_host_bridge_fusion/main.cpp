#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr std::int64_t kModulus = 1'000'000'007LL;
constexpr std::int64_t kRounds = 2'400;
constexpr std::int64_t kCellCount = 96;
constexpr std::int64_t kExpected = 786677225;
constexpr const char* kPath = "semantic_host_bridge_fusion_bridge.txt";

struct Frame {
    std::int64_t bias;
    std::int64_t salt;
    std::int64_t route;
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
    std::int64_t bias = 17;
    std::int64_t turns = 0;

    std::int64_t fold(std::int64_t request) {
        turns += 1;
        return ((request * 13) + bias + 17) % kModulus;
    }
};

bool bridge_valid(std::int64_t value) {
    return value >= 0 && value < kModulus;
}

std::int64_t bridge_mix(std::int64_t value) {
    return ((value * 29) + 31) % kModulus;
}

std::int64_t bridge_stage(std::int64_t value) {
    return (value + 23) % kModulus;
}

std::int64_t bridge_pipeline(std::int64_t value) {
    return bridge_stage(bridge_mix(value));
}

std::int64_t commit_bridge(Authority& authority, Mirror& mirror, std::int64_t value, std::int64_t delta) {
    authority.signal = value;
    authority.epoch += 1;
    authority.ledger = (authority.ledger + delta + authority.epoch + 5) % kModulus;
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

std::string read_text(const char* path) {
    std::ifstream input(path, std::ios::binary);
    std::string content;
    content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return content;
}

}  // namespace

int main() {
    Authority authority{};
    Mirror mirror{};
    Relay relay{};
    const std::int64_t warm = relay.fold(0);
    (void)warm;

    const std::array<Frame, 6> frames = {{
        {5, 19, 7, true},
        {11, 23, 13, false},
        {17, 29, 17, true},
        {23, 31, 19, true},
        {29, 37, 23, false},
        {31, 41, 29, true},
    }};

    std::vector<std::int64_t> cells(static_cast<std::size_t>(kCellCount), 0);
    std::int64_t checksum = 0;
    std::int64_t process_spec_count = 0;

    for (std::int64_t i = 0; i < kRounds; ++i) {
        const std::int64_t lane = i % 6;
        const std::int64_t slot = ((i * 7) + lane) % kCellCount;
        const Frame moved = frames[static_cast<std::size_t>(lane)];
        const std::int64_t old_cell = cells[static_cast<std::size_t>(slot)];

        const std::string payload =
            "bridge-" + std::to_string(i % 97) + "-" + std::to_string(moved.route);
        {
            std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
            out << payload;
        }
        {
            std::ofstream out(kPath, std::ios::binary | std::ios::app);
            out << "|" << moved.salt;
        }
        const std::string readback = read_text(kPath);
        if (readback.size() <= payload.size()) {
            std::remove(kPath);
            return 6;
        }

        const std::int64_t protocol_score = 8 + 6;

        process_spec_count += 1;
        process_spec_count -= 1;
        const std::int64_t process_score = 11;

        const std::int64_t mixed_input =
            (checksum + old_cell + static_cast<std::int64_t>(readback.size()) + protocol_score +
             process_score + moved.bias + moved.route + i) %
            kModulus;
        const std::int64_t staged = bridge_pipeline(mixed_input);
        const std::int64_t committed = commit_bridge(authority, mirror, staged, moved.salt + lane + process_score);
        const std::int64_t legal = bridge_valid(committed) ? 0 : -1;
        const std::int64_t reply =
            relay.fold((committed + mirror.ledger_copy + protocol_score + process_score + legal) % kModulus);
        const std::int64_t next_cell =
            (reply + old_cell + mirror.signal_copy + mirror.epoch_copy + mirror.ledger_copy + slot) %
            kModulus;
        cells[static_cast<std::size_t>(slot)] = next_cell;
        checksum =
            (checksum + reply + committed + protocol_score + process_score + moved.route + moved.salt + legal) %
            kModulus;
    }

    std::remove(kPath);

    const std::int64_t observed = fold_cells(cells);
    const std::int64_t final_score =
        (checksum + observed + mirror.signal_copy + mirror.epoch_copy + mirror.ledger_copy) % kModulus;
    return final_score == kExpected ? 0 : 1;
}
