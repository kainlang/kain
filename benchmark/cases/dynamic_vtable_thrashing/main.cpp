#include <cstdint>
#include <memory>
#include <vector>

namespace {
constexpr std::int64_t kKernelCount = 64;
constexpr std::int64_t kIterations = 1800000;
constexpr std::int64_t kModulus = 1000000007;
constexpr std::int64_t kExpected = 185456717;

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

struct Kernel {
    virtual ~Kernel() = default;
    virtual std::int64_t score(std::int64_t value) const = 0;
};

struct AddKernel final : Kernel {
    explicit AddKernel(std::int64_t bias_in) : bias(bias_in) {}
    BENCH_NOINLINE std::int64_t score(std::int64_t value) const override { return value + (bias * 3) + 7; }
    std::int64_t bias;
};

struct MultiplyKernel final : Kernel {
    explicit MultiplyKernel(std::int64_t bias_in) : bias(bias_in) {}
    BENCH_NOINLINE std::int64_t score(std::int64_t value) const override { return (value * (bias + 5)) + 11; }
    std::int64_t bias;
};

struct ModKernel final : Kernel {
    explicit ModKernel(std::int64_t bias_in) : bias(bias_in) {}
    BENCH_NOINLINE std::int64_t score(std::int64_t value) const override { return ((value + bias) % 257) + (bias * 13); }
    std::int64_t bias;
};

struct SquareKernel final : Kernel {
    explicit SquareKernel(std::int64_t bias_in) : bias(bias_in) {}
    BENCH_NOINLINE std::int64_t score(std::int64_t value) const override { return (value * value) + (bias * 17) + 3; }
    std::int64_t bias;
};

struct BiasSquareKernel final : Kernel {
    explicit BiasSquareKernel(std::int64_t bias_in) : bias(bias_in) {}
    BENCH_NOINLINE std::int64_t score(std::int64_t value) const override { return (value * 9) + (bias * bias) + 19; }
    std::int64_t bias;
};

struct FoldKernel final : Kernel {
    explicit FoldKernel(std::int64_t bias_in) : bias(bias_in) {}
    BENCH_NOINLINE std::int64_t score(std::int64_t value) const override { return (((value + 31) * (bias + 7)) % 4099) + 23; }
    std::int64_t bias;
};

struct ExpandKernel final : Kernel {
    explicit ExpandKernel(std::int64_t bias_in) : bias(bias_in) {}
    BENCH_NOINLINE std::int64_t score(std::int64_t value) const override { return (value * 5) + ((bias + 1) * 29); }
    std::int64_t bias;
};

struct XorKernel final : Kernel {
    explicit XorKernel(std::int64_t bias_in) : bias(bias_in) {}
    BENCH_NOINLINE std::int64_t score(std::int64_t value) const override { return ((value * 7) ^ (bias * 41)) + 37; }
    std::int64_t bias;
};

std::unique_ptr<Kernel> make_kernel(std::int64_t kind, std::int64_t bias) {
    switch (kind) {
    case 0:
        return std::make_unique<AddKernel>(bias);
    case 1:
        return std::make_unique<MultiplyKernel>(bias);
    case 2:
        return std::make_unique<ModKernel>(bias);
    case 3:
        return std::make_unique<SquareKernel>(bias);
    case 4:
        return std::make_unique<BiasSquareKernel>(bias);
    case 5:
        return std::make_unique<FoldKernel>(bias);
    case 6:
        return std::make_unique<ExpandKernel>(bias);
    default:
        return std::make_unique<XorKernel>(bias);
    }
}
} // namespace

int main() {
    std::vector<std::unique_ptr<Kernel>> kernels;
    kernels.reserve(static_cast<std::size_t>(kKernelCount));
    for (std::int64_t slot = 0; slot < kKernelCount; ++slot) {
        const std::int64_t kind = ((slot * 5) + 3) % 8;
        const std::int64_t bias = ((slot * 17) % 23) + 1;
        kernels.push_back(make_kernel(kind, bias));
    }

    std::int64_t acc = 0;
    for (std::int64_t index = 0; index < kIterations; ++index) {
        const std::int64_t slot = index % kKernelCount;
        const std::int64_t value = ((index * 13) + 7) % 1009;
        const std::int64_t score = kernels[static_cast<std::size_t>(slot)]->score(value);
        acc = (acc + score + slot) % kModulus;
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == kExpected ? 0 : 1;
}
