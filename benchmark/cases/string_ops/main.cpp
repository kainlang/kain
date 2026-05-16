#include <cstddef>
#include <cstdint>
#include <string_view>

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

constexpr std::string_view STRING_TEXT = "ka0in0be0nch";
constexpr std::string_view STRING_NEEDLE = "in";
constexpr std::string_view STRING_TAIL = "ch";
constexpr std::int64_t ITERATIONS = 100'000;
constexpr std::int64_t EXPECTED = 2'050'000;

BENCH_NOINLINE bool starts_with_at(std::string_view text, std::size_t index, std::string_view needle) {
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

BENCH_NOINLINE std::size_t find_substring(std::string_view text, std::string_view needle, std::size_t start) {
    const std::size_t needle_length = needle.size();
    if (needle_length == 0) {
        return start;
    }
    std::size_t index = start;
    while (index + needle_length <= text.size()) {
        if (starts_with_at(text, index, needle)) {
            return index;
        }
        ++index;
    }
    return text.size();
}

int main() {
    std::int64_t acc = 0;
    std::int64_t i = 0;
    bool use_needle = true;

    while (i < ITERATIONS) {
        if (use_needle) {
            acc = acc + static_cast<std::int64_t>(STRING_TEXT.size()) +
                static_cast<std::int64_t>(find_substring(STRING_TEXT, STRING_NEEDLE, 0)) +
                static_cast<std::int64_t>(STRING_NEEDLE.size());
        } else {
            acc = acc + static_cast<std::int64_t>(STRING_TEXT.size()) +
                static_cast<std::int64_t>(find_substring(STRING_TEXT, STRING_TAIL, 0)) +
                static_cast<std::int64_t>(STRING_TAIL.size());
        }
        use_needle = !use_needle;
        ++i;
    }

    return acc == EXPECTED ? 0 : 1;
}
