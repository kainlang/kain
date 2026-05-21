#include <array>
#include <cstdint>
#include <vector>

namespace {

constexpr std::int64_t kModulus = 1'000'000'007LL;
constexpr std::int64_t kRounds = 180'000;
constexpr std::int64_t kCellCount = 192;
constexpr std::int64_t kExpected = 474502230;

struct Packet {
    std::int64_t bias;
    std::int64_t phase;
    std::int64_t salt;
    bool hot;
};

struct Authority {
    std::int64_t signal = 1;
    std::int64_t epoch = 0;
    std::int64_t credit = 0;
};

struct Mirror {
    std::int64_t signal_copy = 1;
    std::int64_t epoch_copy = 0;
    std::int64_t credit_copy = 0;
};

struct Relay {
    std::int64_t bias = 7;
    std::int64_t turns = 0;
    std::int64_t lag = 0;

    std::int64_t fold(std::int64_t request) {
        turns += 1;
        lag = (lag + (request % 17) + turns) % kModulus;
        return ((request * 19) + bias + 31) % kModulus;
    }
};

bool backpressure_valid(std::int64_t value) {
    return value >= 0 && value < kModulus;
}

std::int64_t backpressure_mix(std::int64_t value) {
    return ((value * 37) + 11) % kModulus;
}

std::int64_t backpressure_stage(std::int64_t value) {
    return (value + 23) % kModulus;
}

std::int64_t backpressure_pipeline(std::int64_t value) {
    return backpressure_stage(backpressure_mix(value));
}

std::int64_t commit_backpressure(Authority& authority, Mirror& mirror, std::int64_t value, std::int64_t delta) {
    authority.signal = value;
    authority.epoch += 1;
    authority.credit = (authority.credit + delta + authority.epoch + 13) % kModulus;
    mirror.signal_copy = authority.signal;
    mirror.epoch_copy = authority.epoch;
    mirror.credit_copy = authority.credit;
    return authority.signal;
}

std::int64_t ask_worker(std::array<Relay, 8>& workers, std::int64_t slot, std::int64_t request) {
    return workers[static_cast<std::size_t>(slot)].fold(request);
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
    std::array<Relay, 8> workers = {{
        {5, 0, 0}, {7, 0, 0}, {11, 0, 0}, {13, 0, 0},
        {17, 0, 0}, {19, 0, 0}, {23, 0, 0}, {29, 0, 0},
    }};
    for (auto& worker : workers) {
        const std::int64_t warm = worker.fold(0);
        (void)warm;
    }

    const std::array<Packet, 8> packets = {{
        {3, 5, 17, true},
        {7, 11, 23, false},
        {13, 17, 29, true},
        {19, 23, 31, true},
        {23, 29, 37, false},
        {31, 37, 41, true},
        {41, 43, 47, false},
        {47, 53, 59, true},
    }};

    std::vector<std::int64_t> cells(static_cast<std::size_t>(kCellCount), 0);
    std::int64_t checksum = 0;
    for (std::int64_t i = 0; i < kRounds; ++i) {
        const std::int64_t lane = i % 8;
        const std::int64_t slot = ((i * 5) + lane) % kCellCount;
        const Packet moved = packets[static_cast<std::size_t>(lane)];
        const std::int64_t old_cell = cells[static_cast<std::size_t>(slot)];
        const std::int64_t mixed_input =
            (checksum + old_cell + moved.bias + moved.phase + mirror.credit_copy + i) % kModulus;
        const std::int64_t staged = backpressure_pipeline(mixed_input);
        const std::int64_t committed = commit_backpressure(authority, mirror, staged, moved.salt + lane);
        const std::int64_t legal = backpressure_valid(committed) ? 0 : -1;
        const std::int64_t burst = ((i / 9) % 3) + 1;
        std::int64_t lane_acc = 0;
        for (std::int64_t burst_idx = 0; burst_idx < burst; ++burst_idx) {
            const std::int64_t request =
                (committed + old_cell + lane_acc + moved.phase + burst_idx + slot + legal) % kModulus;
            const std::int64_t reply = ask_worker(workers, lane, request);
            lane_acc = (lane_acc + reply + burst_idx + lane) % kModulus;
        }
        const std::int64_t next_cell =
            (lane_acc + mirror.signal_copy + mirror.epoch_copy + mirror.credit_copy + slot) % kModulus;
        cells[static_cast<std::size_t>(slot)] = next_cell;
        checksum = (checksum + next_cell + lane_acc + burst + legal) % kModulus;
    }

    const std::int64_t observed = fold_cells(cells);
    const std::int64_t final_score =
        (checksum + observed + mirror.signal_copy + mirror.epoch_copy + mirror.credit_copy) % kModulus;
    return final_score == kExpected ? 0 : 1;
}
