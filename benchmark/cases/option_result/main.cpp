#include <cstdint>
#include <optional>
#include <variant>

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

constexpr std::int64_t ITERATIONS = 300'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 143'207'783;

BENCH_NOINLINE std::optional<std::int64_t> maybe_value(std::int64_t value) {
    if (value % 5 == 0) {
        return std::nullopt;
    }
    return value + 3;
}

BENCH_NOINLINE std::variant<std::int64_t, const char*> parse_value(std::int64_t value) {
    if (value % 7 == 0) {
        return "skip";
    }
    return value * 2;
}

int main() {
    std::int64_t acc = 0;
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        const std::int64_t maybe_component = maybe_value(i).value_or(1);
        const auto parsed = parse_value(i);
        const std::int64_t parsed_component =
            std::holds_alternative<const char*>(parsed) ? 2 : std::get<std::int64_t>(parsed);
        acc = (acc + maybe_component + parsed_component) % MODULUS;
        ++i;
    }
    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
