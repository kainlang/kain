// ============================================================================
//  CASES_V3 C++ God-File — Reference Implementation
//  ============================================================================
//  Compile (Unix):  clang++ -std=c++20 -O3 -march=native -DNDEBUG bench.cpp -o bench
//  Compile (Win):   clang++ -std=c++20 -O3 -march=native -DNDEBUG bench.cpp -o bench -lws2_32
//  Run:             ./bench <benchmark_name>
//  Compute all:     ./bench --compute-all
//  ============================================================================
//  This file implements 30 benchmark functions matching the CASES_V3 contract
//  (v3_contract.md).  Every function produces a deterministic checksum and
//  compares it against an EXPECTED constant.  All 30 use the same RANDOM_SEED
//  (42) and MODULUS (1000000007).
//
//  EXPECTED values are set to 0 initially.  Run `./bench --compute-all` after
//  the first correct compilation to record every checksum, then copy the
//  printed values into the EXPECTED constants below.
//  ============================================================================

#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ============================================================================
//  OS-Specific Headers & Types
//  ============================================================================

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
using SOCKET_T = SOCKET;
#define CLOSE_SOCKET(s)  closesocket(s)
#define GET_ERRNO()      WSAGetLastError()
#define IS_INVALID(s)    ((s) == INVALID_SOCKET)
#define POPEN(c,m)       _popen(c,m)
#define PCLOSE(p)        _pclose(p)
#define FSYNC(f)         _commit(_fileno(f))
#define TEMP_FILE_PAT    '\\'
#define POPEN_READ       "r"
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
using SOCKET_T = int;
#define CLOSE_SOCKET(s)  close(s)
#define GET_ERRNO()      errno
#define IS_INVALID(s)    ((s) < 0)
#define POPEN(c,m)       popen(c,m)
#define PCLOSE(p)        pclose(p)
#define FSYNC(fd)        fsync(fileno(fd))
#define TEMP_FILE_PAT    '/'
#define POPEN_READ       "r"
#endif

// ============================================================================
//  CONSTANTS
//  ============================================================================

constexpr int64_t RANDOM_SEED  = 42;
constexpr int64_t MODULUS      = 1000000007LL;

// ============================================================================
//  EXPECTED VALUES — fill after first `--compute-all` run
//  ============================================================================

constexpr int64_t BINARY_TREES_EXPECTED        = 33204912;
constexpr int64_t NBODY_EXPECTED               = 870484;
constexpr int64_t SPECTRAL_NORM_EXPECTED       = 123046366;
constexpr int64_t MANDELBROT_EXPECTED          = 24366700;
constexpr int64_t FASTAR_EXPECTED              = 192725270;
constexpr int64_t REGEX_REDUX_EXPECTED         = 0;
constexpr int64_t PIDIGITS_EXPECTED            = 909268399;
constexpr int64_t HASHMAP_HEAVY_EXPECTED       = 650540109;
constexpr int64_t BTREE_SCAN_EXPECTED          = 806426008;
constexpr int64_t SORT_GAUNTLET_EXPECTED       = 596679945;
constexpr int64_t VECTOR_GROWTH_EXPECTED       = 457147467;
constexpr int64_t GRAPH_BFS_EXPECTED           = 4815078;
constexpr int64_t ALLOC_SMALL_CHURN_EXPECTED   = 697560860;
constexpr int64_t ALLOC_LARGE_OBJECTS_EXPECTED = 559928756;
constexpr int64_t ARENA_VS_MALLOC_EXPECTED     = 195418520;
constexpr int64_t CACHE_MARCH_EXPECTED         = 468572388;
constexpr int64_t RC_VS_GC_TRACE_EXPECTED      = 186139778;
constexpr int64_t PARALLEL_REDUCE_EXPECTED     = 54663625;
constexpr int64_t MUTEX_CONTENTION_EXPECTED    = 16000000;
constexpr int64_t SPSC_QUEUE_EXPECTED          = 994650007;
constexpr int64_t MPMC_QUEUE_EXPECTED          = 994650007;
constexpr int64_t ACTOR_SPAM_EXPECTED          = 330998915;
constexpr int64_t ASYNC_READY_EXPECTED         = 964999762;
constexpr int64_t FILE_READ_EXPECTED           = 857998427;
constexpr int64_t FILE_WRITE_EXPECTED          = 772372166;
constexpr int64_t TCP_ECHO_EXPECTED            = 38556712;
constexpr int64_t PROCESS_SPAWN_EXPECTED       = 691508967;
constexpr int64_t C_FFI_HOTLOOP_EXPECTED       = 59593236;
constexpr int64_t C_BUFFER_HANDOFF_EXPECTED    = 25385936;
constexpr int64_t BUILD_SELF_STRESS_EXPECTED   = 471552;

// ============================================================================
//  HELPERS
//  ============================================================================

// --- Deterministic LCG (same constants as v3_contract.md) ---

struct LCG {
    int64_t state;
    LCG() : state(RANDOM_SEED) {}
    explicit LCG(int64_t seed) : state(seed) {}
    int64_t next() {
        state = (state * 1103515245LL + 12345) & 0x7fffffff;
        return state;
    }
};

// --- djb2 hash (for string-key benchmarks) ---

int64_t hash_string(const std::string& s) {
    int64_t h = 5381;
    for (unsigned char c : s) {
        h = ((h << 5) + h) + static_cast<int64_t>(c);
    }
    return h;
}

// --- Thread count helper ---

int num_hardware_threads() {
    int n = static_cast<int>(std::thread::hardware_concurrency());
    return n > 0 ? n : 4;
}

// --- Random string of length 8–16 ---

std::string random_key(LCG& rng) {
    static const char chars[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int len = 8 + static_cast<int>(rng.next() % 9);      // 8 .. 16
    std::string s;
    s.reserve(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i) {
        s.push_back(chars[rng.next() % 62]);
    }
    return s;
}

// --- Temp file path ---

std::string temp_file_path() {
#ifdef _WIN32
    char dir[MAX_PATH + 1];
    DWORD dr = GetTempPathA(MAX_PATH + 1, dir);
    if (dr == 0 || dr > MAX_PATH) dir[0] = 0;
    char path[MAX_PATH + 1];
    if (GetTempFileNameA(dir, "bnc", 0, path) == 0) {
        std::snprintf(path, sizeof(path), "bench_temp_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    }
    return std::string(path);
#else
    char path[] = "/tmp/bench_cpp_XXXXXX";
    int fd = mkstemp(path);
    if (fd >= 0) close(fd);
    return std::string(path);
#endif
}

// ============================================================================
//  SUPPORTING DATA STRUCTURES
//  ============================================================================

// --- SPSC lock-free ring buffer (power-of-two capacity) ---

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");
    static constexpr size_t MASK = Capacity - 1;

    std::array<T, Capacity> buffer_{};
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

public:
    bool try_push(T value) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if ((h - t) >= Capacity) return false;
        buffer_[h & MASK] = value;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    bool try_pop(T& value) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (t >= h) return false;
        value = buffer_[t & MASK];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    // Blocking push (for producer thread)
    void push(T value) {
        while (!try_push(value)) {
            std::this_thread::yield();
        }
    }

    // Blocking pop (for consumer thread)
    T pop() {
        T value{};
        while (!try_pop(value)) {
            std::this_thread::yield();
        }
        return value;
    }
};

// --- MPMC queue (mutex-based) ---

template <typename T>
class MPMCQueue {
    std::deque<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    size_t capacity_;

public:
    explicit MPMCQueue(size_t cap) : capacity_(cap) {}

    void push(T value) {
        std::unique_lock<std::mutex> lock(mtx_);
        not_full_.wait(lock, [this]() { return queue_.size() < capacity_; });
        queue_.push_back(std::move(value));
        lock.unlock();
        not_empty_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        not_empty_.wait(lock, [this]() { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        not_full_.notify_one();
        return value;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop_front();
        not_full_.notify_one();
        return true;
    }
};

// --- Simple Arena Allocator (bump pointer) ---

class Arena {
    char* buffer_;
    size_t capacity_;
    size_t offset_;

public:
    explicit Arena(size_t cap)
        : capacity_(cap), offset_(0) {
        buffer_ = new char[cap];
    }

    ~Arena() { delete[] buffer_; }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void* alloc(size_t size) {
        constexpr size_t ALIGN = alignof(std::max_align_t);
        size_t aligned = (offset_ + ALIGN - 1) & ~(ALIGN - 1);
        if (aligned + size > capacity_) {
            return nullptr;
        }
        void* ptr = buffer_ + aligned;
        offset_ = aligned + size;
        std::memset(ptr, 0, size);
        return ptr;
    }

    void reset() { offset_ = 0; }
};

// --- TreeNode for binary_trees ---

struct TreeNode {
    int64_t value;
    TreeNode* left;
    TreeNode* right;

    TreeNode() : value(1), left(nullptr), right(nullptr) {}
};

static TreeNode* alloc_tree(int depth) {
    if (depth <= 0) return nullptr;
    auto* n      = new TreeNode();
    n->left      = alloc_tree(depth - 1);
    n->right     = alloc_tree(depth - 1);
    return n;
}

static int64_t tree_sum(TreeNode* n) {
    if (!n) return 0;
    return n->value + tree_sum(n->left) + tree_sum(n->right);
}

static void free_tree(TreeNode* n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    delete n;
}

// --- Body for nbody ---

struct Body {
    double x, y, z;
    double vx, vy, vz;
    double mass;
};

// --- Internal compute for c_ffi_call_hotloop ---

extern "C" int64_t c_add(int64_t a, int64_t b) {
    return a + b;
}

// ============================================================================
//  BENCHMARK 1 — binary_trees
//  ============================================================================

static int64_t bench_binary_trees() {
    constexpr int MIN_DEPTH = 4;
    constexpr int MAX_DEPTH = 18;

    int64_t checksum = 0;
    for (int d = MIN_DEPTH; d <= MAX_DEPTH; d += 2) {
        int64_t iterations = 1LL << (MAX_DEPTH - d + MIN_DEPTH);
        for (int64_t i = 0; i < iterations; ++i) {
            TreeNode* tree = alloc_tree(d);
            checksum = (checksum + tree_sum(tree)) % MODULUS;
            free_tree(tree);
        }
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 2 — nbody
//  ============================================================================

static int64_t bench_nbody() {
    constexpr int    N_BODIES  = 500;
    constexpr int    TIMESTEPS = 100;
    constexpr double DT        = 0.01;
    constexpr double SOFTENING = 1e-9;

    LCG rng(RANDOM_SEED);
    std::vector<Body> bodies(N_BODIES);

    for (int i = 0; i < N_BODIES; ++i) {
        bodies[i].x    = (static_cast<double>(rng.next()) / 1e6) - 500.0;
        bodies[i].y    = (static_cast<double>(rng.next()) / 1e6) - 500.0;
        bodies[i].z    = (static_cast<double>(rng.next()) / 1e6) - 500.0;
        bodies[i].vx   = (static_cast<double>(rng.next()) / 1e9) - 0.5;
        bodies[i].vy   = (static_cast<double>(rng.next()) / 1e9) - 0.5;
        bodies[i].vz   = (static_cast<double>(rng.next()) / 1e9) - 0.5;
        bodies[i].mass = 1.0 + (static_cast<double>(rng.next()) / 1e9);
    }

    for (int t = 0; t < TIMESTEPS; ++t) {
        // Accelerations
        for (int i = 0; i < N_BODIES; ++i) {
            double fx = 0.0, fy = 0.0, fz = 0.0;
            for (int j = 0; j < N_BODIES; ++j) {
                if (i == j) continue;
                double dx = bodies[i].x - bodies[j].x;
                double dy = bodies[i].y - bodies[j].y;
                double dz = bodies[i].z - bodies[j].z;
                double dist = std::sqrt(dx * dx + dy * dy + dz * dz + SOFTENING);
                double inv_dist3 = 1.0 / (dist * dist * dist);
                fx -= dx * bodies[j].mass * inv_dist3;
                fy -= dy * bodies[j].mass * inv_dist3;
                fz -= dz * bodies[j].mass * inv_dist3;
            }
            bodies[i].vx += fx * DT;
            bodies[i].vy += fy * DT;
            bodies[i].vz += fz * DT;
        }
        // Position update
        for (int i = 0; i < N_BODIES; ++i) {
            bodies[i].x += bodies[i].vx * DT;
            bodies[i].y += bodies[i].vy * DT;
            bodies[i].z += bodies[i].vz * DT;
        }
    }

    double total = 0.0;
    for (int i = 0; i < N_BODIES; ++i) {
        total += bodies[i].x + bodies[i].y + bodies[i].z;
    }
    int64_t checksum = static_cast<int64_t>(std::floor(total)) % MODULUS;

    return checksum;
}

// ============================================================================
//  BENCHMARK 3 — spectral_norm
//  ============================================================================

static int64_t bench_spectral_norm() {
    constexpr int N = 2000;

    auto a = [](int i, int j) -> double {
        int64_t s = static_cast<int64_t>(i + j) * (i + j + 1) / 2 + i + 1;
        return 1.0 / static_cast<double>(s);
    };

    std::vector<double> u(N, 1.0);
    std::vector<double> v(N, 0.0);

    for (int iter = 0; iter < 10; ++iter) {
        // v = A * u
        for (int i = 0; i < N; ++i) {
            double sum = 0.0;
            for (int j = 0; j < N; ++j) {
                sum += u[j] * a(i, j);
            }
            v[i] = sum;
        }
        // u = A^T * v
        for (int i = 0; i < N; ++i) {
            double sum = 0.0;
            for (int j = 0; j < N; ++j) {
                sum += v[j] * a(j, i);
            }
            u[i] = sum;
        }
    }

    double vBv = 0.0, vv = 0.0;
    for (int i = 0; i < N; ++i) {
        vBv += u[i] * v[i];
        vv += v[i] * v[i];
    }

    int64_t checksum = static_cast<int64_t>(
        std::floor(std::sqrt(vBv / vv) * 1e9)) % MODULUS;

    return checksum;
}

// ============================================================================
//  BENCHMARK 4 — mandelbrot
//  ============================================================================

static int64_t bench_mandelbrot() {
    constexpr int    WIDTH   = 800;
    constexpr int    HEIGHT  = 800;
    constexpr int    MAX_ITER = 200;
    constexpr double XMIN    = -2.0;
    constexpr double XMAX    = 1.0;
    constexpr double YMIN    = -1.5;
    constexpr double YMAX    = 1.5;

    int64_t checksum = 0;

    for (int y = 0; y < HEIGHT; ++y) {
        double ci = YMIN + (YMAX - YMIN) * static_cast<double>(y) / HEIGHT;
        for (int x = 0; x < WIDTH; ++x) {
            double cr = XMIN + (XMAX - XMIN) * static_cast<double>(x) / WIDTH;
            double zr = 0.0, zi = 0.0;
            int iter = 0;
            while (zr * zr + zi * zi <= 4.0 && iter < MAX_ITER) {
                double nzr = zr * zr - zi * zi + cr;
                double nzi = 2.0 * zr * zi + ci;
                zr = nzr;
                zi = nzi;
                ++iter;
            }
            checksum += static_cast<int64_t>(iter);
        }
    }
    checksum %= MODULUS;

    return checksum;
}

// ============================================================================
//  BENCHMARK 5 — fasta
//  ============================================================================

static int64_t bench_fasta() {
    constexpr int N = 250000;

    const std::string ALU =
        "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTG"
        "AGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAAT"
        "TAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTT"
        "GAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAG"
        "CGAGACTCCGTCTCAAAAA";

    // Count nucleotide frequencies in ALU
    int64_t freq[256] = {0};
    for (unsigned char c : ALU) {
        freq[c]++;
    }
    int64_t total_weight =
        freq['A'] + freq['C'] + freq['G'] + freq['T'];

    LCG rng(RANDOM_SEED);
    int64_t checksum = 0;

    for (int i = 0; i < N; ++i) {
        int64_t r = rng.next() % total_weight;
        unsigned char c;
        if (r < freq['A']) {
            c = 'A';
        } else if (r < freq['A'] + freq['C']) {
            c = 'C';
        } else if (r < freq['A'] + freq['C'] + freq['G']) {
            c = 'G';
        } else {
            c = 'T';
        }
        checksum = (checksum * 31 + static_cast<int64_t>(c)) % MODULUS;
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 6 — regex_redux
//  ============================================================================

static int64_t bench_regex_redux() {
    constexpr int N = 5000;

    LCG rng(RANDOM_SEED);

    // Generate DNA string of length N
    std::string dna;
    dna.reserve(static_cast<size_t>(N));
    static const char bases[] = {'A', 'C', 'G', 'T'};
    for (int i = 0; i < N; ++i) {
        dna.push_back(bases[rng.next() % 4]);
    }

    // 1. Count occurrences of pattern
    std::regex pat1("agggtaaa|tttaccct");
    auto count_begin = std::sregex_iterator(dna.begin(), dna.end(), pat1);
    auto count_end   = std::sregex_iterator();
    int64_t count1 = static_cast<int64_t>(std::distance(count_begin, count_end));

    // 2. Replace: tHa[Nt] → <4>
    std::regex pat2("tHa[Nt]");
    std::string replaced = std::regex_replace(dna, pat2, "<4>");
    int64_t len = static_cast<int64_t>(replaced.size());

    // 3. Checksum = (count1 * len) % MODULUS
    int64_t checksum = (count1 * len) % MODULUS;

    return checksum;
}

// ============================================================================
//  BENCHMARK 7 — pidigits
//  ============================================================================

static int64_t bench_pidigits() {
    constexpr int N = 5000;

    // Compute approximate pi via Machin-like formula (double precision)
    // arctan(x) = x - x^3/3 + x^5/5 - x^7/7 + ...
    // pi = 16*arctan(1/5) - 4*arctan(1/239)

    auto arctan_series = [](double x_inv, int terms) -> double {
        double x = 1.0 / x_inv;
        double x2 = x * x;
        double sum = 0.0;
        double term = x;
        for (int i = 0; i < terms; ++i) {
            double divisor = static_cast<double>(2 * i + 1);
            if (i % 2 == 0) {
                sum += term / divisor;
            } else {
                sum -= term / divisor;
            }
            term *= x2;
            // Guard against underflow
            if (std::abs(term) < 1e-300) break;
        }
        return sum;
    };

    double pi = 16.0 * arctan_series(5.0, 1000000)
              - 4.0 * arctan_series(239.0, 1000000);

    int64_t checksum = 0;
    for (int i = 0; i < N; ++i) {
        pi *= 10.0;
        int digit = static_cast<int>(pi);
        pi -= static_cast<double>(digit);
        checksum = (checksum * 31 + digit) % MODULUS;
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 8 — hashmap_heavy
//  ============================================================================

static int64_t bench_hashmap_heavy() {
    constexpr int64_t N_KEYS    = 100000;
    constexpr int64_t N_LOOKUPS = 5000000;

    LCG rng(RANDOM_SEED);

    // Generate keys
    std::vector<std::string> keys;
    keys.reserve(static_cast<size_t>(N_KEYS));
    for (int64_t i = 0; i < N_KEYS; ++i) {
        keys.push_back(random_key(rng));
    }

    // Insert into unordered_map
    std::unordered_map<std::string, int64_t> map;
    for (int64_t i = 0; i < N_KEYS; ++i) {
        map[keys[static_cast<size_t>(i)]] = i;
    }

    // Lookup storm
    int64_t checksum = 0;
    for (int64_t i = 0; i < N_LOOKUPS; ++i) {
        size_t idx = static_cast<size_t>(rng.next() % N_KEYS);
        auto it = map.find(keys[idx]);
        if (it != map.end()) {
            checksum = (checksum * 31 + it->second) % MODULUS;
        }
    }

    // Delete every 4th key
    for (int64_t i = 0; i < N_KEYS; i += 4) {
        map.erase(keys[static_cast<size_t>(i)]);
    }

    // Re-lookup remaining
    for (int64_t i = 0; i < N_KEYS; ++i) {
        auto it = map.find(keys[static_cast<size_t>(i)]);
        if (it != map.end()) {
            checksum = (checksum * 31 + it->second) % MODULUS;
        }
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 9 — btree_scan
//  ============================================================================

static int64_t bench_btree_scan() {
    constexpr int64_t N_KEYS = 500000;

    LCG rng(RANDOM_SEED);

    // Insert random integers into ordered map (key=random, value=index)
    std::map<int64_t, int64_t> map;
    for (int64_t i = 0; i < N_KEYS; ++i) {
        int64_t key = rng.next();
        map[key] = i;
    }

    int64_t checksum = 0;

    // Forward scan
    for (auto& kv : map) {
        checksum = (checksum + (kv.first * kv.second) % MODULUS) % MODULUS;
    }

    // Reverse scan
    for (auto it = map.rbegin(); it != map.rend(); ++it) {
        checksum = (checksum + (it->first * it->second) % MODULUS) % MODULUS;
    }

    // Delete every 3rd key
    int64_t idx = 0;
    for (auto it = map.begin(); it != map.end(); ) {
        if (idx % 3 == 0) {
            it = map.erase(it);
        } else {
            ++it;
        }
        ++idx;
    }

    // Re-iterate
    for (auto& kv : map) {
        checksum = (checksum + (kv.first * kv.second) % MODULUS) % MODULUS;
    }

    checksum %= MODULUS;

    return checksum;
}

// ============================================================================
//  BENCHMARK 10 — sort_gauntlet
//  ============================================================================

static int64_t bench_sort_gauntlet() {
    constexpr int64_t N = 1000000;

    LCG rng(RANDOM_SEED);
    std::vector<int64_t> arr(static_cast<size_t>(N));

    auto accumulate = [&]() -> int64_t {
        int64_t cs = 0;
        for (size_t i = 0; i < static_cast<size_t>(N); ++i) {
            cs = (cs * 31 + arr[i]) % MODULUS;
        }
        return cs;
    };

    int64_t checksum = 0;

    // Pass 1: random array
    for (size_t i = 0; i < static_cast<size_t>(N); ++i) {
        arr[i] = rng.next();
    }
    std::sort(arr.begin(), arr.end());
    checksum = (checksum + accumulate()) % MODULUS;

    // Pass 2: nearly-sorted (perturb 1%)
    for (size_t i = 0; i < static_cast<size_t>(N); ++i) {
        if (rng.next() % 100 == 0) {
            arr[i] = rng.next();
        }
    }
    std::sort(arr.begin(), arr.end());
    checksum = (checksum + accumulate()) % MODULUS;

    // Pass 3: reversed
    std::reverse(arr.begin(), arr.end());
    std::sort(arr.begin(), arr.end());
    checksum = (checksum + accumulate()) % MODULUS;

    checksum %= MODULUS;

    return checksum;
}

// ============================================================================
//  BENCHMARK 11 — vector_growth
//  ============================================================================

static int64_t bench_vector_growth() {
    constexpr int64_t N = 10000000;
    constexpr int64_t CHECKPOINT_INTERVAL = 100000;

    std::vector<int64_t> vec;
    int64_t checksum = 0;

    for (int64_t i = 0; i < N; ++i) {
        vec.push_back(i);
        if ((i + 1) % CHECKPOINT_INTERVAL == 0) {
            int64_t partial = 0;
            size_t start = vec.size() >= 100 ? vec.size() - 100 : 0;
            for (size_t j = start; j < vec.size(); ++j) {
                partial = (partial + vec[j]) % MODULUS;
            }
            checksum = (checksum * 31 + partial) % MODULUS;
        }
    }

    // Pop all
    while (!vec.empty()) {
        vec.pop_back();
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 12 — graph_bfs
//  ============================================================================

static int64_t bench_graph_bfs() {
    constexpr int64_t N_NODES = 100000;
    constexpr int64_t N_EDGES = 1000000;

    LCG rng(RANDOM_SEED);

    // Build adjacency list
    std::vector<std::vector<int64_t>> adj(static_cast<size_t>(N_NODES));
    for (int64_t e = 0; e < N_EDGES; ++e) {
        int64_t src = rng.next() % N_NODES;
        int64_t dst = rng.next() % N_NODES;
        if (src != dst) {
            adj[static_cast<size_t>(src)].push_back(dst);
        }
    }

    auto bfs = [&](int64_t start) -> int64_t {
        std::vector<int64_t> dist(static_cast<size_t>(N_NODES), -1);
        std::queue<int64_t> q;
        dist[static_cast<size_t>(start)] = 0;
        q.push(start);
        while (!q.empty()) {
            int64_t u = q.front();
            q.pop();
            for (int64_t v : adj[static_cast<size_t>(u)]) {
                if (dist[static_cast<size_t>(v)] < 0) {
                    dist[static_cast<size_t>(v)] = dist[static_cast<size_t>(u)] + 1;
                    q.push(v);
                }
            }
        }
        int64_t cs = 0;
        for (int64_t i = 0; i < N_NODES; ++i) {
            int64_t d = dist[static_cast<size_t>(i)];
            if (d >= 0) {
                cs = (cs + (i * d) % MODULUS) % MODULUS;
            }
        }
        return cs;
    };

    int64_t checksum = bfs(0);

    // BFS from 10 random start nodes
    for (int i = 0; i < 10; ++i) {
        int64_t start = rng.next() % N_NODES;
        checksum = (checksum + bfs(start)) % MODULUS;
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 13 — alloc_small_churn
//  ============================================================================

static int64_t bench_alloc_small_churn() {
    constexpr int64_t N_ALLOCS = 1000000;

    LCG rng(RANDOM_SEED);
    int64_t checksum = 0;

    for (int64_t i = 0; i < N_ALLOCS; ++i) {
        size_t size = 16 + static_cast<size_t>(rng.next() % 240);   // 16..256
        void* ptr = std::malloc(size);
        if (!ptr) continue;
        // Fill first 16 bytes with pattern
        std::memset(ptr, static_cast<int>(i & 0xFF), size < 16 ? size : 16);
        int64_t val = *static_cast<const int64_t*>(ptr);
        checksum = (checksum + val) % MODULUS;
        std::free(ptr);
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 14 — alloc_large_objects
//  ============================================================================

static int64_t bench_alloc_large_objects() {
    constexpr int64_t N_LARGE = 1000;
    constexpr int64_t N_SMALL = 100000;

    LCG rng(RANDOM_SEED);
    int64_t checksum = 0;

    for (int64_t i = 0; i < N_LARGE; ++i) {
        // Large allocation: 1MB + random(0..64MB)
        size_t large_size =
            (1ULL * 1024 * 1024) +
            static_cast<size_t>(rng.next() % (64 * 1024 * 1024));
        void* large_ptr = std::malloc(large_size);
        if (!large_ptr) continue;

        // Touch every page
        auto* bytes = static_cast<volatile char*>(large_ptr);
        size_t first_256_bytes = (256 * sizeof(int64_t)) < large_size
                                 ? (256 * sizeof(int64_t)) : large_size;
        for (size_t off = 0; off < large_size; off += 4096) {
            bytes[off] = static_cast<char>(i & 0x7F);
        }
        // Fill first 256 ints deterministically
        for (size_t off = 0; off < first_256_bytes; ++off) {
            const_cast<char*>(static_cast<const volatile char*>(large_ptr))[off]
                = static_cast<char>((static_cast<int64_t>(off) + i) & 0xFF);
        }

        // Read first 256 ints
        auto* large_ints = static_cast<const int64_t*>(large_ptr);
        int64_t local_sum = 0;
        size_t n_ints = large_size / sizeof(int64_t) < 256
                        ? large_size / sizeof(int64_t) : 256;
        for (size_t j = 0; j < n_ints; ++j) {
            local_sum = (local_sum + large_ints[j]) % MODULUS;
        }
        checksum = (checksum + local_sum) % MODULUS;

        // Small interleaved allocs
        int64_t small_count = N_SMALL / N_LARGE;
        for (int64_t s = 0; s < small_count; ++s) {
            void* small_ptr = std::malloc(64);
            if (small_ptr) {
                // Write a deterministic value so read is well-defined
                std::memset(small_ptr, static_cast<int>(s & 0xFF), 8);
                auto* sp = static_cast<const int64_t*>(small_ptr);
                checksum = (checksum + *sp) % MODULUS;
                std::free(small_ptr);
            }
        }

        std::free(large_ptr);
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 15 — arena_vs_malloc
//  ============================================================================

struct ArenaObject {
    int64_t id;
    int64_t value;
    double  score;
};

static int64_t bench_arena_vs_malloc() {
    constexpr int64_t N_OBJECTS = 100000;
    constexpr int64_t N_ROUNDS  = 10;

    LCG rng(RANDOM_SEED);
    constexpr size_t ARENA_SIZE = static_cast<size_t>(N_OBJECTS) * sizeof(ArenaObject) + 4096;
    Arena arena(ARENA_SIZE);

    int64_t arena_checksum = 0;
    int64_t malloc_checksum = 0;

    for (int64_t round = 0; round < N_ROUNDS; ++round) {
        // --- Arena path ---
        arena.reset();
        for (int64_t i = 0; i < N_OBJECTS; ++i) {
            auto* obj = static_cast<ArenaObject*>(arena.alloc(sizeof(ArenaObject)));
            obj->id    = i;
            obj->value = rng.next();
            obj->score = static_cast<double>(rng.next()) / 1e6;
            arena_checksum = (arena_checksum + obj->value) % MODULUS;
        }

        // --- Malloc path ---
        std::vector<ArenaObject*> mallocs;
        mallocs.reserve(static_cast<size_t>(N_OBJECTS));
        for (int64_t i = 0; i < N_OBJECTS; ++i) {
            auto* obj = static_cast<ArenaObject*>(std::malloc(sizeof(ArenaObject)));
            obj->id    = i;
            obj->value = rng.next();
            obj->score = static_cast<double>(rng.next()) / 1e6;
            mallocs.push_back(obj);
            malloc_checksum = (malloc_checksum + obj->value) % MODULUS;
        }
        for (auto* obj : mallocs) {
            std::free(obj);
        }
    }

    int64_t checksum = (arena_checksum + malloc_checksum) % MODULUS;

    return checksum;
}

// ============================================================================
//  BENCHMARK 16 — cache_march
//  ============================================================================

static int64_t bench_cache_march() {
    constexpr int64_t BUFFER_BYTES = 128LL * 1024 * 1024;   // 128 MiB
    constexpr int64_t N_INTS = BUFFER_BYTES / 4;             // 33554432 int32_t
    constexpr int64_t N_INTS_64 = BUFFER_BYTES / 8;          // 16777216 int64_t

    // Use int32_t for 128 MiB of 4-byte integers
    size_t count = static_cast<size_t>(N_INTS);
    auto* buffer = new int32_t[count];

    LCG rng(RANDOM_SEED);
    for (size_t i = 0; i < count; ++i) {
        buffer[i] = static_cast<int32_t>(rng.next());
    }

    int64_t total_sum = 0;

    // Pass 1: sequential
    for (size_t i = 0; i < count; ++i) {
        total_sum += buffer[i];
    }

    // Pass 2: stride-8
    for (size_t i = 0; i < count; i += 8) {
        total_sum += buffer[i];
    }

    // Pass 3: stride-64
    for (size_t i = 0; i < count; i += 64) {
        total_sum += buffer[i];
    }

    // Pass 4: random access
    for (int64_t i = 0; i < N_INTS / 100; ++i) {
        size_t idx = static_cast<size_t>(rng.next() % N_INTS);
        total_sum += buffer[idx];
    }

    delete[] buffer;

    // Modulo for wrapped int64_t accumulation
    // total_sum could be negative from overflow, but deterministic
    int64_t checksum = static_cast<int64_t>(
        static_cast<uint64_t>(total_sum) % static_cast<uint64_t>(MODULUS));

    return checksum;
}

// ============================================================================
//  BENCHMARK 17 — rc_vs_gc_trace
//  ============================================================================

struct GraphNode {
    int64_t id;
    int64_t value;
    std::shared_ptr<GraphNode> ref;
};

static int64_t bench_rc_vs_gc_trace() {
    constexpr int64_t N_NODES         = 100000;
    constexpr double  EDGE_PROBABILITY = 0.01;

    LCG rng(RANDOM_SEED);

    std::vector<std::shared_ptr<GraphNode>> nodes;
    nodes.reserve(static_cast<size_t>(N_NODES));
    for (int64_t i = 0; i < N_NODES; ++i) {
        auto n = std::make_shared<GraphNode>();
        n->id    = i;
        n->value = rng.next();
        nodes.push_back(std::move(n));
    }

    // Create edges: for each i, for each j > i, 1% chance edge i->j
    for (int64_t i = 0; i < N_NODES; ++i) {
        for (int64_t j = i + 1; j < N_NODES; ++j) {
            if (rng.next() % 100 < 1) {   // 1%
                nodes[static_cast<size_t>(i)]->ref = nodes[static_cast<size_t>(j)];
            }
        }
    }

    // Walk graph from all roots, accumulate values
    int64_t checksum = 0;
    for (auto& root : nodes) {
        auto cur = root;
        while (cur) {
            checksum = (checksum + cur->value) % MODULUS;
            cur = cur->ref;
        }
    }

    // Drop all roots — RC decrements cascade
    nodes.clear();

    return checksum;
}

// ============================================================================
//  BENCHMARK 18 — parallel_reduce
//  ============================================================================

static int64_t bench_parallel_reduce() {
    constexpr int64_t N = 100000000;

    LCG rng(RANDOM_SEED);
    auto* data = new int64_t[static_cast<size_t>(N)];
    for (int64_t i = 0; i < N; ++i) {
        data[static_cast<size_t>(i)] = rng.next();
    }

    int n_threads = num_hardware_threads();
    int64_t chunk = N / n_threads;
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(n_threads));
    std::vector<int64_t> partials(static_cast<size_t>(n_threads), 0);

    for (int t = 0; t < n_threads; ++t) {
        threads.emplace_back([t, chunk, n_threads, data, &partials]() {
            int64_t sum = 0;
            int64_t start = t * chunk;
            int64_t end = (t == n_threads - 1) ? N : start + chunk;
            for (int64_t i = start; i < end; ++i) {
                sum = (sum + data[static_cast<size_t>(i)]) % MODULUS;
            }
            partials[static_cast<size_t>(t)] = sum;
        });
    }

    for (auto& th : threads) th.join();

    int64_t checksum = 0;
    for (auto p : partials) {
        checksum = (checksum + p) % MODULUS;
    }

    delete[] data;

    return checksum;
}

// ============================================================================
//  BENCHMARK 19 — mutex_contention
//  ============================================================================

static int64_t bench_mutex_contention() {
    constexpr int64_t N_INCREMENTS = 1000000;
    int n_threads = num_hardware_threads();
    std::atomic<int64_t> counter{0};

    {
        std::vector<std::thread> threads;
        threads.reserve(static_cast<size_t>(n_threads));
        for (int t = 0; t < n_threads; ++t) {
            threads.emplace_back([&counter]() {
                for (int64_t i = 0; i < N_INCREMENTS; ++i) {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& th : threads) th.join();
    }

    int64_t expected = static_cast<int64_t>(n_threads) * N_INCREMENTS;
    int64_t checksum = counter.load();

    if (checksum != expected) {
        // Counter mismatch indicates data race — still return value for diagnostics
    }
    (void)expected;

    return checksum;
}

// ============================================================================
//  BENCHMARK 20 — spsc_queue
//  ============================================================================

static int64_t bench_spsc_queue() {
    constexpr int64_t N_ITEMS = 10000000;
    constexpr size_t  CAPACITY = 1024;

    SPSCQueue<int64_t, CAPACITY> queue;
    std::atomic<bool> producer_done{false};
    int64_t checksum = 0;

    std::thread producer([&]() {
        for (int64_t i = 0; i < N_ITEMS; ++i) {
            queue.push(i);
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        int64_t cs = 0;
        int64_t consumed = 0;
        while (consumed < N_ITEMS) {
            int64_t val;
            while (!queue.try_pop(val)) {
                if (producer_done.load(std::memory_order_acquire) &&
                    consumed >= N_ITEMS) break;
                std::this_thread::yield();
            }
            cs = (cs + val) % MODULUS;
            ++consumed;
        }
        checksum = cs;
    });

    producer.join();
    consumer.join();

    return checksum;
}

// ============================================================================
//  BENCHMARK 21 — mpmc_queue
//  ============================================================================

static int64_t bench_mpmc_queue() {
    constexpr int64_t N_PRODUCERS = 4;
    constexpr int64_t N_CONSUMERS = 4;
    constexpr int64_t N_ITEMS     = 10000000;

    MPMCQueue<int64_t> queue(4096);
    std::atomic<int64_t> items_produced{0};
    std::atomic<int64_t> items_consumed{0};
    int64_t checksum = 0;

    std::vector<std::thread> producers;
    producers.reserve(static_cast<size_t>(N_PRODUCERS));
    for (int p = 0; p < N_PRODUCERS; ++p) {
        producers.emplace_back([&, p]() {
            int64_t start = p * (N_ITEMS / N_PRODUCERS);
            int64_t end   = (p == N_PRODUCERS - 1) ? N_ITEMS : start + (N_ITEMS / N_PRODUCERS);
            for (int64_t i = start; i < end; ++i) {
                queue.push(i);
            }
            items_produced.fetch_add(end - start, std::memory_order_relaxed);
        });
    }

    std::mutex cs_mutex;
    std::vector<std::thread> consumers;
    consumers.reserve(static_cast<size_t>(N_CONSUMERS));
    for (int c = 0; c < N_CONSUMERS; ++c) {
        consumers.emplace_back([&]() {
            int64_t cs = 0;
            int64_t local_count = 0;
            while (local_count < N_ITEMS / N_CONSUMERS) {
                int64_t val;
                if (queue.try_pop(val)) {
                    cs = (cs + val) % MODULUS;
                    ++local_count;
                } else {
                    std::this_thread::yield();
                }
            }
            {
                std::lock_guard<std::mutex> lock(cs_mutex);
                checksum = (checksum + cs) % MODULUS;
            }
            items_consumed.fetch_add(local_count, std::memory_order_relaxed);
        });
    }

    for (auto& th : producers) th.join();
    for (auto& th : consumers) th.join();

    return checksum;
}

// ============================================================================
//  BENCHMARK 22 — actor_spam
//  ============================================================================

static int64_t bench_actor_spam() {
    constexpr int64_t N_ACTORS           = 10000;
    constexpr int64_t N_MESSAGES_PER_ACTOR = 100;

    int n_threads = num_hardware_threads();

    // Distribute actors across threads
    int64_t actors_per_thread = N_ACTORS / n_threads;
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(n_threads));
    std::vector<int64_t> results(static_cast<size_t>(n_threads), 0);

    for (int t = 0; t < n_threads; ++t) {
        threads.emplace_back([t, actors_per_thread, n_threads, &results]() {
            int64_t start = t * actors_per_thread;
            int64_t end   = (t == n_threads - 1)
                              ? N_ACTORS
                              : start + actors_per_thread;
            int64_t local_checksum = 0;

            for (int64_t actor_id = start; actor_id < end; ++actor_id) {
                int64_t sum = 0;
                for (int m = 0; m < N_MESSAGES_PER_ACTOR; ++m) {
                    int64_t msg = (actor_id * 31 + m * 7) % MODULUS;
                    sum = (sum + msg) % MODULUS;
                }
                local_checksum = (local_checksum + sum) % MODULUS;
            }
            results[static_cast<size_t>(t)] = local_checksum;
        });
    }

    int64_t checksum = 0;
    for (auto& th : threads) {
        th.join();
    }
    for (auto r : results) {
        checksum = (checksum + r) % MODULUS;
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 23 — async_ready_pipeline
//  ============================================================================

static int64_t bench_async_ready_pipeline() {
    constexpr int64_t N_FUTURES = 1000;
    constexpr int64_t N_ROUNDS  = 10000;

    int64_t checksum = 0;

    for (int64_t round = 0; round < N_ROUNDS; ++round) {
        std::vector<std::future<int64_t>> futures;
        futures.reserve(static_cast<size_t>(N_FUTURES));
        for (int i = 0; i < N_FUTURES; ++i) {
            futures.push_back(
                std::async(std::launch::async, [i]() -> int64_t {
                    return (static_cast<int64_t>(i) * 7LL) % MODULUS;
                }));
        }
        for (auto& f : futures) {
            int64_t val = f.get();
            checksum = (checksum + val) % MODULUS;
        }
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 24 — file_read_streaming
//  ============================================================================

static int64_t bench_file_read_streaming() {
    constexpr int64_t FILE_SIZE   = 1LL * 1024 * 1024 * 1024;   // 1 GiB
    constexpr size_t  CHUNK_SIZE  = 65536;

    std::string path = temp_file_path();

    // Create file with deterministic data
    {
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) return 1;
        LCG rng(RANDOM_SEED);
        std::vector<char> buf(CHUNK_SIZE);
        int64_t written = 0;
        while (written < FILE_SIZE) {
            size_t to_write = CHUNK_SIZE;
            if (written + static_cast<int64_t>(CHUNK_SIZE) > FILE_SIZE) {
                to_write = static_cast<size_t>(FILE_SIZE - written);
            }
            for (size_t i = 0; i < to_write; ++i) {
                buf[i] = static_cast<char>(rng.next() & 0xFF);
            }
            ofs.write(buf.data(), static_cast<std::streamsize>(to_write));
            written += static_cast<int64_t>(to_write);
        }
        ofs.close();
    }

    // Read and compute rolling checksum
    int64_t checksum = 0;
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs) return 1;
        std::vector<char> buf(CHUNK_SIZE);
        while (ifs) {
            ifs.read(buf.data(), static_cast<std::streamsize>(CHUNK_SIZE));
            std::streamsize n_read = ifs.gcount();
            if (n_read <= 0) break;
            int64_t chunk_sum = 0;
            for (std::streamsize i = 0; i < n_read; ++i) {
                chunk_sum += static_cast<unsigned char>(buf[static_cast<size_t>(i)]);
            }
            checksum = (checksum * 31 + chunk_sum) % MODULUS;
        }
        ifs.close();
    }

    // Delete temp file
    std::error_code ec;
    std::filesystem::remove(path, ec);

    return checksum;
}

// ============================================================================
//  BENCHMARK 25 — file_write_streaming
//  ============================================================================

static int64_t bench_file_write_streaming() {
    constexpr int64_t FILE_SIZE       = 1LL * 1024 * 1024 * 1024;   // 1 GiB
    constexpr size_t  CHUNK_SIZE      = 65536;
    constexpr int64_t FSYNC_INTERVAL  = 16LL * 1024 * 1024;        // 16 MiB

    std::string path = temp_file_path();

    int64_t checksum = 0;
    LCG rng(RANDOM_SEED);

    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return 1;

        std::vector<char> buf(CHUNK_SIZE);
        int64_t total_written = 0;
        int64_t since_fsync = 0;

        while (total_written < FILE_SIZE) {
            // Generate deterministic chunk
            size_t to_write = CHUNK_SIZE;
            if (total_written + static_cast<int64_t>(CHUNK_SIZE) > FILE_SIZE) {
                to_write = static_cast<size_t>(FILE_SIZE - total_written);
            }
            for (size_t i = 0; i < to_write; ++i) {
                char byte = static_cast<char>(rng.next() & 0xFF);
                buf[i] = byte;
                checksum = (checksum * 31 + static_cast<unsigned char>(byte)) % MODULUS;
            }

            std::fwrite(buf.data(), 1, to_write, f);
            total_written += static_cast<int64_t>(to_write);
            since_fsync += static_cast<int64_t>(to_write);

            // Periodic fsync
            if (since_fsync >= FSYNC_INTERVAL) {
                std::fflush(f);
                FSYNC(f);
                since_fsync = 0;
            }
        }

        std::fflush(f);
        FSYNC(f);
        std::fclose(f);
    }

    // Delete temp file
    std::error_code ec;
    std::filesystem::remove(path, ec);

    return checksum;
}

// ============================================================================
//  BENCHMARK 26 — tcp_echo_throughput
//  ============================================================================

static int64_t bench_tcp_echo_throughput() {
    constexpr int64_t N_ROUNDTRIPS  = 5000;
    constexpr size_t  PAYLOAD_SIZE  = 65536;

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) return 1;
#endif

    // Create listening socket
    SOCKET_T listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (IS_INVALID(listen_fd)) { return 1; }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;   // ephemeral

    if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) < 0) {
        CLOSE_SOCKET(listen_fd);
        return 1;
    }

    socklen_t addr_len = sizeof(addr);
    if (getsockname(listen_fd, reinterpret_cast<struct sockaddr*>(&addr),
                    &addr_len) < 0) {
        CLOSE_SOCKET(listen_fd);
        return 1;
    }
    int port = ntohs(addr.sin_port);

    if (listen(listen_fd, 1) < 0) {
        CLOSE_SOCKET(listen_fd);
        return 1;
    }

    // Server thread
    int64_t checksum = 0;
    std::atomic<bool> server_done{false};

    std::thread server([&]() {
        SOCKET_T client_fd = accept(listen_fd, nullptr, nullptr);
        if (IS_INVALID(client_fd)) {
            server_done.store(true, std::memory_order_release);
            return;
        }

        std::vector<char> buf(PAYLOAD_SIZE);
        for (int64_t i = 0; i < N_ROUNDTRIPS; ++i) {
            int64_t total_recv = 0;
            while (total_recv < static_cast<int64_t>(PAYLOAD_SIZE)) {
                int n = static_cast<int>(
                    recv(client_fd, buf.data() + total_recv,
                         static_cast<int>(PAYLOAD_SIZE - static_cast<size_t>(total_recv)), 0));
                if (n <= 0) break;
                total_recv += n;
            }
            // Echo back
            int64_t total_sent = 0;
            while (total_sent < static_cast<int64_t>(PAYLOAD_SIZE)) {
                int n = static_cast<int>(
                    send(client_fd, buf.data() + total_sent,
                         static_cast<int>(PAYLOAD_SIZE - static_cast<size_t>(total_sent)), 0));
                if (n <= 0) break;
                total_sent += n;
            }
        }
        CLOSE_SOCKET(client_fd);
        server_done.store(true, std::memory_order_release);
    });

    // Client
    {
        SOCKET_T sock = socket(AF_INET, SOCK_STREAM, 0);
        if (IS_INVALID(sock)) {
            server.detach();
            return 1;
        }

        addr.sin_port = htons(static_cast<unsigned short>(port));
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                    sizeof(addr)) < 0) {
            CLOSE_SOCKET(sock);
            server.detach();
            return 1;
        }

        std::vector<char> send_buf(PAYLOAD_SIZE, 0);
        std::vector<char> recv_buf(PAYLOAD_SIZE, 0);

        for (int64_t i = 0; i < N_ROUNDTRIPS; ++i) {
            // Fill send buffer with deterministic pattern
            for (size_t j = 0; j < PAYLOAD_SIZE; ++j) {
                send_buf[j] = static_cast<char>((i * 31 + static_cast<int64_t>(j)) & 0xFF);
            }

            // Send
            int64_t total_sent = 0;
            while (total_sent < static_cast<int64_t>(PAYLOAD_SIZE)) {
                int n = static_cast<int>(
                    send(sock, send_buf.data() + total_sent,
                         static_cast<int>(PAYLOAD_SIZE - static_cast<size_t>(total_sent)), 0));
                if (n <= 0) break;
                total_sent += n;
            }

            // Receive echo
            int64_t total_recv = 0;
            while (total_recv < static_cast<int64_t>(PAYLOAD_SIZE)) {
                int n = static_cast<int>(
                    recv(sock, recv_buf.data() + total_recv,
                         static_cast<int>(PAYLOAD_SIZE - static_cast<size_t>(total_recv)), 0));
                if (n <= 0) break;
                total_recv += n;
            }

            // Verify
            if (std::memcmp(send_buf.data(), recv_buf.data(), PAYLOAD_SIZE) == 0) {
                int64_t sum = 0;
                for (size_t j = 0; j < PAYLOAD_SIZE; ++j) {
                    sum += static_cast<unsigned char>(recv_buf[j]);
                }
                checksum = (checksum * 31 + sum) % MODULUS;
            }
        }

        CLOSE_SOCKET(sock);
    }

    server.join();
    CLOSE_SOCKET(listen_fd);

#ifdef _WIN32
    WSACleanup();
#endif

    return checksum;
}

// ============================================================================
//  BENCHMARK 27 — process_spawn_chain
//  ============================================================================

static int64_t bench_process_spawn_chain() {
    constexpr int64_t N_SPAWNS = 1000;

    int64_t checksum = 0;

    for (int64_t i = 0; i < N_SPAWNS; ++i) {
        std::string cmd =
#ifdef _WIN32
            "cmd /c echo " + std::to_string(i);
#else
            "echo " + std::to_string(i);
#endif

        FILE* pipe = POPEN(cmd.c_str(), POPEN_READ);
        if (!pipe) continue;

        char buf[64];
        if (std::fgets(buf, sizeof(buf), pipe)) {
            int64_t val = static_cast<int64_t>(std::atol(buf));
            checksum = (checksum * 31 + val) % MODULUS;
        }

        PCLOSE(pipe);
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 28 — c_ffi_call_hotloop
//  ============================================================================

// The extern "C" function is defined at top (c_add)
static int64_t bench_c_ffi_call_hotloop() {
    constexpr int64_t N_CALLS = 10000000;

    int64_t checksum = 0;
    for (int64_t i = 0; i < N_CALLS; ++i) {
        int64_t result = c_add(i, i + 1);
        checksum = (checksum * 31 + result) % MODULUS;
    }

    return checksum;
}

// ============================================================================
//  BENCHMARK 29 — c_buffer_handoff
//  ============================================================================

static int64_t bench_c_buffer_handoff() {
    constexpr int64_t N_ROUNDTRIPS = 100000;
    constexpr size_t  BUFFER_SIZE  = 4096;

    int64_t checksum = 0;

    // Allocate shared buffer
    auto* buffer = new unsigned char[BUFFER_SIZE];

    for (int64_t i = 0; i < N_ROUNDTRIPS; ++i) {
        // Fill with pattern
        for (size_t j = 0; j < BUFFER_SIZE; ++j) {
            buffer[j] = static_cast<unsigned char>((i * 31 + static_cast<int64_t>(j)) & 0xFF);
        }

        // Compute sum (simulate C-side handoff)
        int64_t sum = 0;
        for (size_t j = 0; j < BUFFER_SIZE; ++j) {
            sum += buffer[j];
        }

        // Verify against expected
        int64_t expected_sum = 0;
        for (size_t j = 0; j < BUFFER_SIZE; ++j) {
            expected_sum += static_cast<unsigned char>((i * 31 + static_cast<int64_t>(j)) & 0xFF);
        }

        if (sum == expected_sum) {
            checksum = (checksum * 31 + sum) % MODULUS;
        }
    }

    delete[] buffer;

    return checksum;
}

// ============================================================================
//  BENCHMARK 30 — build_self_stress
//  ============================================================================

static int64_t bench_build_self_stress() {
    // This benchmark measures compilation speed, not runtime.
    // The function validates that the binary exists and returns
    // its file size % MODULUS for the runner to record.
    std::error_code ec;
    auto self_path = std::filesystem::current_path() /
#ifdef _WIN32
        "bench.exe";
#else
        "bench";
#endif
    uintmax_t sz = std::filesystem::file_size(self_path, ec);
    if (ec) {
        // Try alternate path: argv[0] relative
        return 0;   // could not validate
    }
    return static_cast<int64_t>(sz) % MODULUS;
}

// ============================================================================
//  COMPUTE-ALL MODE
//  ============================================================================

// Each bench function returns its computed checksum directly.
// The dispatcher compares against EXPECTED and returns 0/1.
// --compute-all prints the raw values for recording into EXPECTED constants.

typedef int64_t (*BenchFn)();

struct BenchInfo {
    const char* name;
    BenchFn fn;
    int64_t expected;
};

static BenchInfo all_benches[] = {
    {"binary_trees",          bench_binary_trees,          BINARY_TREES_EXPECTED},
    {"nbody",                 bench_nbody,                 NBODY_EXPECTED},
    {"spectral_norm",         bench_spectral_norm,         SPECTRAL_NORM_EXPECTED},
    {"mandelbrot",            bench_mandelbrot,            MANDELBROT_EXPECTED},
    {"fasta",                 bench_fasta,                 FASTAR_EXPECTED},
    {"regex_redux",           bench_regex_redux,           REGEX_REDUX_EXPECTED},
    {"pidigits",              bench_pidigits,              PIDIGITS_EXPECTED},
    {"hashmap_heavy",         bench_hashmap_heavy,         HASHMAP_HEAVY_EXPECTED},
    {"btree_scan",            bench_btree_scan,            BTREE_SCAN_EXPECTED},
    {"sort_gauntlet",         bench_sort_gauntlet,         SORT_GAUNTLET_EXPECTED},
    {"vector_growth",         bench_vector_growth,         VECTOR_GROWTH_EXPECTED},
    {"graph_bfs",             bench_graph_bfs,             GRAPH_BFS_EXPECTED},
    {"alloc_small_churn",     bench_alloc_small_churn,     ALLOC_SMALL_CHURN_EXPECTED},
    {"alloc_large_objects",   bench_alloc_large_objects,   ALLOC_LARGE_OBJECTS_EXPECTED},
    {"arena_vs_malloc",       bench_arena_vs_malloc,       ARENA_VS_MALLOC_EXPECTED},
    {"cache_march",           bench_cache_march,           CACHE_MARCH_EXPECTED},
    {"rc_vs_gc_trace",        bench_rc_vs_gc_trace,        RC_VS_GC_TRACE_EXPECTED},
    {"parallel_reduce",       bench_parallel_reduce,       PARALLEL_REDUCE_EXPECTED},
    {"mutex_contention",      bench_mutex_contention,      MUTEX_CONTENTION_EXPECTED},
    {"spsc_queue",            bench_spsc_queue,            SPSC_QUEUE_EXPECTED},
    {"mpmc_queue",            bench_mpmc_queue,            MPMC_QUEUE_EXPECTED},
    {"actor_spam",            bench_actor_spam,            ACTOR_SPAM_EXPECTED},
    {"async_ready_pipeline",  bench_async_ready_pipeline,  ASYNC_READY_EXPECTED},
    {"file_read_streaming",   bench_file_read_streaming,   FILE_READ_EXPECTED},
    {"file_write_streaming",  bench_file_write_streaming,  FILE_WRITE_EXPECTED},
    {"tcp_echo_throughput",   bench_tcp_echo_throughput,   TCP_ECHO_EXPECTED},
    {"process_spawn_chain",   bench_process_spawn_chain,   PROCESS_SPAWN_EXPECTED},
    {"c_ffi_call_hotloop",    bench_c_ffi_call_hotloop,    C_FFI_HOTLOOP_EXPECTED},
    {"c_buffer_handoff",      bench_c_buffer_handoff,      C_BUFFER_HANDOFF_EXPECTED},
    {"build_self_stress",     bench_build_self_stress,     BUILD_SELF_STRESS_EXPECTED},
};

static constexpr int64_t NUM_BENCHES =
    sizeof(all_benches) / sizeof(all_benches[0]);

// ============================================================================
//  MAIN — Dispatcher
//  ============================================================================

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: bench <benchmark_name>\n");
        std::fprintf(stderr, "       bench --compute-all\n");
        return 1;
    }

    std::string name = argv[1];

    if (name == "--compute-all") {
        // Run every benchmark once; print the computed checksum for each.
        // The user copies these into the EXPECTED constants above.
        std::printf("=== CASES_V3 C++ Expected Values ===\n");
        for (int64_t i = 0; i < NUM_BENCHES; ++i) {
            auto& b = all_benches[static_cast<size_t>(i)];
            int64_t result = b.fn();
            std::printf("  %-30s => %lld\n", b.name,
                        static_cast<long long>(result));
        }
        std::printf("=== End ===\n");
        return 0;
    }

    for (int64_t i = 0; i < NUM_BENCHES; ++i) {
        auto& b = all_benches[static_cast<size_t>(i)];
        if (name == b.name) {
            int64_t result = b.fn();
            if (result != b.expected) {
                return 1;   // mismatch
            }
            return 0;       // match
        }
    }

    std::fprintf(stderr, "unknown benchmark: %s\n", name.c_str());
    return 1;
}
