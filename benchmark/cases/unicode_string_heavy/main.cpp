#include <cstdint>
#include <string>

const std::string TEXT_A = "orbit-世界-кисть-مرحبا-🙂-flux";
const std::string NEEDLE_A1 = "世界";
const std::string NEEDLE_A2 = "🙂";
const std::string TEXT_B = "lattice-猫-данные-سلام-🚀-field";
const std::string NEEDLE_B1 = "данные";
const std::string NEEDLE_B2 = "🚀";
constexpr std::int64_t ITERATIONS = 150'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 15'524'994;

bool starts_with_at(const std::string& text, std::size_t index, const std::string& needle) {
    if (index + needle.size() > text.size()) {
        return false;
    }
    std::size_t offset = 0;
    while (offset < needle.size()) {
        if (text[index + offset] != needle[offset]) {
            return false;
        }
        ++offset;
    }
    return true;
}

std::int64_t find_substring(const std::string& text, const std::string& needle, std::size_t start) {
    if (needle.empty()) {
        return static_cast<std::int64_t>(start);
    }
    std::size_t index = start;
    while (index + needle.size() <= text.size()) {
        if (starts_with_at(text, index, needle)) {
            return static_cast<std::int64_t>(index);
        }
        ++index;
    }
    return -1;
}

std::int64_t score_text(const std::string& text, const std::string& needle_a, const std::string& needle_b) {
    return static_cast<std::int64_t>(text.size()) + find_substring(text, needle_a, 0) +
           find_substring(text, needle_b, 0) + static_cast<std::int64_t>(needle_a.size()) +
           static_cast<std::int64_t>(needle_b.size());
}

int main() {
    const std::int64_t score_a = score_text(TEXT_A, NEEDLE_A1, NEEDLE_A2);
    const std::int64_t score_b = score_text(TEXT_B, NEEDLE_B1, NEEDLE_B2);
    std::int64_t acc = 0;
    for (std::int64_t index = 0; index < ITERATIONS; ++index) {
        const std::int64_t score = (index & 1) == 0 ? score_a : score_b;
        acc = (acc + score + (index % 7)) % MODULUS;
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
