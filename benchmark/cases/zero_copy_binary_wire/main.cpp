#include <array>
#include <cstdint>

namespace {
constexpr std::int64_t kPacketCount = 64;
constexpr std::int64_t kWordsPerPacket = 4;
constexpr std::int64_t kIterations = 200000;
constexpr std::int64_t kModulus = 1000000007;
constexpr std::int64_t kExpected = 924829641;
} // namespace

int main() {
    std::array<std::int64_t, kPacketCount * kWordsPerPacket> buffer{};
    std::int64_t acc = 0;
    for (std::int64_t round = 0; round < kIterations; ++round) {
        for (std::int64_t packet = 0; packet < kPacketCount; ++packet) {
            const std::int64_t seq = (round * kPacketCount) + packet;
            const std::int64_t version = (packet % 4) + 1;
            const std::int64_t kind = ((packet * 3) + round) % 8;
            const std::int64_t flags = (round + packet) % 16;
            const std::int64_t route = ((packet * 5) + 7) % 64;
            const std::int64_t payload = ((seq * 13) + (route * 17) + 19) % 4096;
            const std::int64_t word0 = (seq * 4096) + (kind * 256) + (flags * 16) + version;
            const std::int64_t word1 = (payload * 128) + route;
            const std::int64_t word2 = ((seq % 97) * 2048) + ((payload % 127) * 16) + flags;
            const std::int64_t word3 = (word0 + word1 + word2 + 97) % 1000003;
            const std::size_t base = static_cast<std::size_t>(packet * kWordsPerPacket);
            buffer[base + 0] = word0;
            buffer[base + 1] = word1;
            buffer[base + 2] = word2;
            buffer[base + 3] = word3;

            const std::int64_t observed0 = buffer[base + 0];
            const std::int64_t observed1 = buffer[base + 1];
            const std::int64_t observed2 = buffer[base + 2];
            const std::int64_t observed3 = buffer[base + 3];
            const std::int64_t observed_version = observed0 % 16;
            const std::int64_t observed_flags = (observed0 / 16) % 16;
            const std::int64_t observed_kind = (observed0 / 256) % 16;
            const std::int64_t observed_seq = observed0 / 4096;
            const std::int64_t observed_route = observed1 % 128;
            const std::int64_t observed_payload = observed1 / 128;
            const std::int64_t observed_epoch = observed2 / 2048;
            acc = (acc + observed_version + observed_flags + observed_kind + (observed_seq % 97) +
                   observed_route + observed_payload + observed_epoch + observed3) %
                  kModulus;
        }
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == kExpected ? 0 : 1;
}
