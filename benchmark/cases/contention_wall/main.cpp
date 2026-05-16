#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

constexpr int WORKER_COUNT = 100;
constexpr std::int64_t ITERATIONS_PER_WORKER = 1'000'000;
constexpr std::int64_t EXPECTED = 100'000'000;

int main() {
    std::atomic<std::int64_t> counter{0};
    std::vector<std::thread> workers;
    workers.reserve(WORKER_COUNT);

    for (int worker = 0; worker < WORKER_COUNT; ++worker) {
        workers.emplace_back([&counter]() {
            std::int64_t i = 0;
            while (i < ITERATIONS_PER_WORKER) {
                counter.fetch_add(1, std::memory_order_seq_cst);
                ++i;
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    return counter.load(std::memory_order_seq_cst) == EXPECTED ? 0 : 1;
}
