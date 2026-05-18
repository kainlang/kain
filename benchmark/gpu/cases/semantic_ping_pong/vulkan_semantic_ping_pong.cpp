#define _CRT_SECURE_NO_WARNINGS

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct TelemetryStat {
    std::string name;
    std::string description;
    std::string format;
    std::string value;
};

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct VerifyResult {
    uint64_t checksum = 1469598103934665603ull;
    uint32_t mismatch_count = 0;
    double max_abs_error = 0.0;
    double mean_abs_error = 0.0;
};

struct ExecutableTelemetry {
    bool extension_enabled = false;
    uint64_t register_count = 0;
    uint64_t binary_size = 0;
    uint64_t vgpr_count = 0;
    uint64_t sgpr_count = 0;
    uint64_t spill_count = 0;
    std::vector<TelemetryStat> stats;
};

[[noreturn]] void die(const std::string& message) {
    throw std::runtime_error(message);
}

void vk_check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        std::ostringstream out;
        out << what << " failed with VkResult " << static_cast<int>(result);
        die(out.str());
    }
}

std::string env_string(const char* name, const std::string& fallback = "") {
    const char* value = std::getenv(name);
    if (!value || !*value) {
        return fallback;
    }
    return value;
}

uint32_t env_u32(const char* name, uint32_t fallback) {
    const std::string value = env_string(name);
    if (value.empty()) {
        return fallback;
    }
    return static_cast<uint32_t>(std::stoul(value));
}

float env_f32(const char* name, float fallback) {
    const std::string value = env_string(name);
    if (value.empty()) {
        return fallback;
    }
    return std::stof(value);
}

std::vector<char> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        die("failed to open file: " + path);
    }
    const std::streamsize size = file.tellg();
    if (size <= 0 || (size % 4) != 0) {
        die("SPIR-V file is empty or not word-aligned: " + path);
    }
    std::vector<char> bytes(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(bytes.data(), size)) {
        die("failed to read file: " + path);
    }
    return bytes;
}

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch));
                } else {
                    out << ch;
                }
        }
    }
    return out.str();
}

bool has_extension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    return std::any_of(extensions.begin(), extensions.end(), [name](const auto& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

uint32_t find_memory_type(
    VkPhysicalDevice physical_device,
    uint32_t type_bits,
    VkMemoryPropertyFlags required_flags) {
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        const bool type_ok = (type_bits & (1u << i)) != 0;
        const bool flags_ok =
            (memory_properties.memoryTypes[i].propertyFlags & required_flags) == required_flags;
        if (type_ok && flags_ok) {
            return i;
        }
    }
    die("no compatible host-visible memory type found");
}

Buffer create_buffer(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkDeviceSize size,
    VkBufferUsageFlags usage) {
    Buffer out{};
    out.size = size;
    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_check(vkCreateBuffer(device, &buffer_info, nullptr, &out.buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, out.buffer, &requirements);

    VkMemoryAllocateInfo allocate_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(
        physical_device,
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vk_check(vkAllocateMemory(device, &allocate_info, nullptr, &out.memory), "vkAllocateMemory");
    vk_check(vkBindBufferMemory(device, out.buffer, out.memory, 0), "vkBindBufferMemory");
    return out;
}

void destroy_buffer(VkDevice device, Buffer& buffer) {
    if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
    }
    if (buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
    }
}

template <typename T>
void write_scalar_buffer(VkDevice device, const Buffer& buffer, const T& value) {
    void* mapped = nullptr;
    vk_check(vkMapMemory(device, buffer.memory, 0, sizeof(T), 0, &mapped), "vkMapMemory");
    std::memcpy(mapped, &value, sizeof(T));
    vkUnmapMemory(device, buffer.memory);
}

template <typename T>
void write_vector_buffer(VkDevice device, const Buffer& buffer, const std::vector<T>& values) {
    void* mapped = nullptr;
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(values.size() * sizeof(T));
    vk_check(vkMapMemory(device, buffer.memory, 0, bytes, 0, &mapped), "vkMapMemory");
    std::memcpy(mapped, values.data(), static_cast<size_t>(bytes));
    vkUnmapMemory(device, buffer.memory);
}

void clear_buffer(VkDevice device, const Buffer& buffer) {
    void* mapped = nullptr;
    vk_check(vkMapMemory(device, buffer.memory, 0, buffer.size, 0, &mapped), "vkMapMemory");
    std::memset(mapped, 0, static_cast<size_t>(buffer.size));
    vkUnmapMemory(device, buffer.memory);
}

uint64_t fnv_mix(uint64_t acc, uint32_t value) {
    acc ^= value;
    acc *= 1099511628211ull;
    return acc;
}

uint32_t float_bits(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float abs_f32(float value) {
    return std::fabs(value);
}

Vec3f vec3(float x, float y, float z) {
    return Vec3f{x, y, z};
}

Vec3f add(const Vec3f& a, const Vec3f& b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3f mul(const Vec3f& value, float scalar) {
    return vec3(value.x * scalar, value.y * scalar, value.z * scalar);
}

Vec3f mix_vec3(const Vec3f& a, const Vec3f& b, float t) {
    return vec3(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t);
}

Vec3f cross(const Vec3f& a, const Vec3f& b) {
    return vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

Vec3f normalize(const Vec3f& value) {
    const float length_sq = (value.x * value.x) + (value.y * value.y) + (value.z * value.z);
    const float inv = length_sq > 0.0f ? 1.0f / std::sqrt(length_sq) : 0.0f;
    return vec3(value.x * inv, value.y * inv, value.z * inv);
}

Vec4f make_vec4(float x, float y, float z, float w) {
    return Vec4f{x, y, z, w};
}

Vec3f xyz(const Vec4f& value) {
    return vec3(value.x, value.y, value.z);
}

std::vector<Vec4f> make_current_positions(uint32_t count) {
    std::vector<Vec4f> values(count);
    for (uint32_t i = 0; i < count; ++i) {
        const float ix = static_cast<float>((i * 3u) % 251u) * 0.03125f;
        const float iy = static_cast<float>((i * 5u) % 257u) * 0.0275f;
        const float iz = static_cast<float>((i * 7u) % 263u) * 0.02175f;
        values[i] = make_vec4(ix + 0.5f, iy + 1.0f, iz + 1.5f, 1.0f + static_cast<float>(i % 7u) * 0.125f);
    }
    return values;
}

std::vector<Vec4f> make_history_positions(uint32_t count) {
    std::vector<Vec4f> values(count);
    for (uint32_t i = 0; i < count; ++i) {
        const float ix = static_cast<float>((i * 11u) % 241u) * 0.0245f;
        const float iy = static_cast<float>((i * 13u) % 239u) * 0.0185f;
        const float iz = static_cast<float>((i * 17u) % 233u) * 0.0155f;
        values[i] = make_vec4(ix + 1.25f, iy + 0.75f, iz + 0.25f, 0.5f + static_cast<float>(i % 5u) * 0.2f);
    }
    return values;
}

std::vector<float> make_alpha_values(uint32_t count) {
    std::vector<float> values(count);
    for (uint32_t i = 0; i < count; ++i) {
        values[i] = static_cast<float>((i * 19u) % 97u) / 96.0f;
    }
    return values;
}

std::vector<int32_t> make_mode_values(uint32_t count) {
    std::vector<int32_t> values(count);
    for (uint32_t i = 0; i < count; ++i) {
        values[i] = static_cast<int32_t>(i % 6u);
    }
    return values;
}

std::vector<int32_t> make_parent_values(uint32_t count) {
    std::vector<int32_t> values(count);
    for (uint32_t i = 0; i < count; ++i) {
        values[i] = (i % 29u) == 0 ? -1 : static_cast<int32_t>((i * 7u) % 113u);
    }
    return values;
}

Vec4f kernel_step(
    const Vec4f& cur,
    const Vec4f& hist,
    float alpha,
    int32_t mode,
    int32_t parent,
    uint32_t idx,
    float time_base,
    float round_phase,
    float gain,
    float eps) {
    const float parent_gate = parent < 0 ? 0.0f : 1.0f;
    const float t = time_base + round_phase + static_cast<float>(idx) * 0.000173f;

    const Vec3f base = mix_vec3(xyz(cur), xyz(hist), 0.22f + alpha * 0.13f);
    const Vec3f normal = normalize(add(base, vec3(eps + parent_gate * 0.001f, 0.0f, 0.0f)));
    const Vec3f tangent = cross(vec3(0.0f, 1.0f, 0.0f), normal);
    const int32_t lane = mode % 6;

    Vec3f accum = base;
    if (lane == 0) {
        accum = add(accum, mul(normal, gain));
    } else if (lane == 1) {
        const Vec3f wave = vec3(std::sin(t), std::cos(t * 1.3f), std::sin(t * 0.7f));
        accum = add(accum, mul(wave, gain * (0.5f + alpha)));
    } else if (lane == 2) {
        const Vec3f flipped = vec3(hist.z, hist.x, hist.y);
        accum = mix_vec3(accum, flipped, 0.35f + alpha * 0.15f);
    } else if (lane == 3) {
        const float lift = std::max(0.0f, std::sin(t * 0.5f)) * gain;
        accum = vec3(accum.x + tangent.x * lift, accum.y + lift, accum.z + tangent.z * lift);
    } else if (lane == 4) {
        const float squash = 1.0f + std::cos(t * 0.9f) * 0.25f * gain;
        accum = vec3(
            accum.x * squash,
            accum.y / std::max(squash, eps),
            accum.z * squash);
    } else {
        const Vec3f cross_term = cross(normal, add(tangent, vec3(alpha, 0.0f, parent_gate)));
        accum = add(accum, mul(cross_term, 0.35f * gain));
    }

    int octave = 0;
    Vec3f wobble = vec3(0.0f, 0.0f, 0.0f);
    float freq = 1.0f;
    float amp = 0.18f * gain;
    while (octave < 3) {
        const float phase = t * freq + alpha * (static_cast<float>(octave) + 1.0f);
        const Vec3f dir = vec3(std::cos(phase), std::sin(phase * 1.7f), std::cos(phase * 0.5f));
        wobble = add(wobble, mul(normalize(add(add(dir, tangent), vec3(eps, 0.0f, 0.0f))), amp));
        freq *= 2.03f;
        amp *= 0.5f;
        ++octave;
    }

    accum = add(accum, wobble);

    float pulse = 0.0f;
    for (int k = 1; k < 4; ++k) {
        pulse += std::sin(t * static_cast<float>(k)) * (0.03f * alpha);
    }

    const Vec3f hist_pulse = add(xyz(hist), mul(normal, pulse));
    accum = mix_vec3(accum, hist_pulse, 0.2f + parent_gate * 0.05f);
    return make_vec4(
        accum.x,
        accum.y,
        accum.z,
        cur.w + 0.25f + alpha * 0.5f + round_phase * 0.01f);
}

std::vector<Vec4f> simulate_rounds(
    std::vector<Vec4f> current,
    std::vector<Vec4f> history,
    const std::vector<float>& alpha,
    const std::vector<int32_t>& mode,
    const std::vector<int32_t>& parent,
    uint32_t rounds,
    float time_step,
    float gain,
    float eps) {
    std::vector<Vec4f> next(current.size(), make_vec4(0.0f, 0.0f, 0.0f, 0.0f));
    for (uint32_t round = 0; round < rounds; ++round) {
        const float time_base = time_step * static_cast<float>(round + 1u);
        const float round_phase = static_cast<float>(round) * 0.25f;
        for (uint32_t i = 0; i < current.size(); ++i) {
            next[i] = kernel_step(
                current[i],
                history[i],
                alpha[i],
                mode[i],
                parent[i],
                i,
                time_base,
                round_phase,
                gain,
                eps);
        }
        history.swap(current);
        current.swap(next);
    }
    return current;
}

VerifyResult verify_positions(
    VkDevice device,
    const Buffer& actual_buffer,
    const std::vector<Vec4f>& expected,
    float epsilon) {
    void* mapped_raw = nullptr;
    vk_check(vkMapMemory(device, actual_buffer.memory, 0, actual_buffer.size, 0, &mapped_raw), "vkMapMemory");
    const Vec4f* actual = static_cast<const Vec4f*>(mapped_raw);

    VerifyResult result{};
    double total_abs_error = 0.0;
    uint64_t compared_components = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        const std::array<float, 4> a = {actual[i].x, actual[i].y, actual[i].z, actual[i].w};
        const std::array<float, 4> e = {expected[i].x, expected[i].y, expected[i].z, expected[i].w};
        for (size_t lane = 0; lane < 4; ++lane) {
            result.checksum = fnv_mix(result.checksum, float_bits(a[lane]));
            const double abs_error = std::fabs(static_cast<double>(a[lane]) - static_cast<double>(e[lane]));
            result.max_abs_error = std::max(result.max_abs_error, abs_error);
            total_abs_error += abs_error;
            ++compared_components;
            if (abs_error > static_cast<double>(epsilon)) {
                ++result.mismatch_count;
                if (result.mismatch_count > 32) {
                    result.mean_abs_error = compared_components == 0 ? 0.0 : total_abs_error / static_cast<double>(compared_components);
                    vkUnmapMemory(device, actual_buffer.memory);
                    return result;
                }
            }
        }
    }

    result.mean_abs_error = compared_components == 0 ? 0.0 : total_abs_error / static_cast<double>(compared_components);
    vkUnmapMemory(device, actual_buffer.memory);
    return result;
}

std::string statistic_value_to_string(const VkPipelineExecutableStatisticKHR& stat) {
    std::ostringstream out;
    switch (stat.format) {
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR:
            out << (stat.value.b32 ? "true" : "false");
            break;
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR:
            out << stat.value.i64;
            break;
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR:
            out << stat.value.u64;
            break;
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR:
            out << stat.value.f64;
            break;
        default:
            out << "unknown";
            break;
    }
    return out.str();
}

std::string statistic_format_to_string(VkPipelineExecutableStatisticFormatKHR format) {
    switch (format) {
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR: return "bool32";
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR: return "int64";
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR: return "uint64";
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR: return "float64";
        default: return "unknown";
    }
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

uint64_t statistic_u64(const VkPipelineExecutableStatisticKHR& stat) {
    switch (stat.format) {
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR: return stat.value.b32 ? 1u : 0u;
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR: return stat.value.i64 < 0 ? 0u : static_cast<uint64_t>(stat.value.i64);
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR: return stat.value.u64;
        case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR: return static_cast<uint64_t>(stat.value.f64);
        default: return 0u;
    }
}

ExecutableTelemetry read_pipeline_executable_telemetry(
    VkDevice device,
    VkPipeline pipeline,
    bool extension_enabled) {
    ExecutableTelemetry telemetry{};
    telemetry.extension_enabled = extension_enabled;
    if (!extension_enabled) {
        return telemetry;
    }

    auto get_properties = reinterpret_cast<PFN_vkGetPipelineExecutablePropertiesKHR>(
        vkGetDeviceProcAddr(device, "vkGetPipelineExecutablePropertiesKHR"));
    auto get_statistics = reinterpret_cast<PFN_vkGetPipelineExecutableStatisticsKHR>(
        vkGetDeviceProcAddr(device, "vkGetPipelineExecutableStatisticsKHR"));
    if (!get_properties || !get_statistics) {
        return telemetry;
    }

    VkPipelineInfoKHR pipeline_info{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
    pipeline_info.pipeline = pipeline;
    uint32_t executable_count = 0;
    if (get_properties(device, &pipeline_info, &executable_count, nullptr) != VK_SUCCESS || executable_count == 0) {
        return telemetry;
    }
    std::vector<VkPipelineExecutablePropertiesKHR> executables(
        executable_count,
        VkPipelineExecutablePropertiesKHR{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR});
    if (get_properties(device, &pipeline_info, &executable_count, executables.data()) != VK_SUCCESS) {
        return telemetry;
    }

    for (uint32_t executable_index = 0; executable_index < executable_count; ++executable_index) {
        VkPipelineExecutableInfoKHR executable_info{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR};
        executable_info.pipeline = pipeline;
        executable_info.executableIndex = executable_index;
        uint32_t stat_count = 0;
        if (get_statistics(device, &executable_info, &stat_count, nullptr) != VK_SUCCESS || stat_count == 0) {
            continue;
        }
        std::vector<VkPipelineExecutableStatisticKHR> stats(
            stat_count,
            VkPipelineExecutableStatisticKHR{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR});
        if (get_statistics(device, &executable_info, &stat_count, stats.data()) != VK_SUCCESS) {
            continue;
        }
        for (const auto& stat : stats) {
            const std::string combined = lower_ascii(std::string(stat.name) + " " + stat.description);
            const uint64_t numeric = statistic_u64(stat);
            if (combined.find("vgpr") != std::string::npos && telemetry.vgpr_count == 0) {
                telemetry.vgpr_count = numeric;
            }
            if (combined.find("register count") != std::string::npos && telemetry.register_count == 0) {
                telemetry.register_count = numeric;
            }
            if (combined.find("binary size") != std::string::npos && telemetry.binary_size == 0) {
                telemetry.binary_size = numeric;
            }
            if (combined.find("sgpr") != std::string::npos && telemetry.sgpr_count == 0) {
                telemetry.sgpr_count = numeric;
            }
            if (combined.find("spill") != std::string::npos) {
                telemetry.spill_count += numeric;
            }
            telemetry.stats.push_back(TelemetryStat{
                stat.name,
                stat.description,
                statistic_format_to_string(stat.format),
                statistic_value_to_string(stat),
            });
        }
    }
    return telemetry;
}

void write_telemetry(
    const std::string& path,
    const std::string& case_id,
    const std::string& language,
    const std::string& shader_path,
    const std::string& entry_point,
    const std::string& device_name,
    uint32_t work_items,
    uint32_t width,
    uint32_t rounds,
    float epsilon,
    float gain,
    float time_step,
    const VerifyResult& verify,
    double duration_ns,
    double wall_ms,
    const ExecutableTelemetry& executable) {
    if (path.empty()) {
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return;
    }
    out << "{\n";
    out << "  \"ok\": " << (verify.mismatch_count == 0 ? "true" : "false") << ",\n";
    out << "  \"case_id\": \"" << json_escape(case_id) << "\",\n";
    out << "  \"language\": \"" << json_escape(language) << "\",\n";
    out << "  \"shader_path\": \"" << json_escape(shader_path) << "\",\n";
    out << "  \"entry_point\": \"" << json_escape(entry_point) << "\",\n";
    out << "  \"device_name\": \"" << json_escape(device_name) << "\",\n";
    out << "  \"work_items\": " << work_items << ",\n";
    out << "  \"width\": " << width << ",\n";
    out << "  \"rounds\": " << rounds << ",\n";
    out << "  \"verify_epsilon\": " << std::fixed << std::setprecision(6) << epsilon << ",\n";
    out << "  \"gain\": " << std::fixed << std::setprecision(6) << gain << ",\n";
    out << "  \"time_step\": " << std::fixed << std::setprecision(6) << time_step << ",\n";
    out << "  \"checksum\": " << verify.checksum << ",\n";
    out << "  \"mismatch_count\": " << verify.mismatch_count << ",\n";
    out << "  \"max_abs_error\": " << std::fixed << std::setprecision(9) << verify.max_abs_error << ",\n";
    out << "  \"mean_abs_error\": " << std::fixed << std::setprecision(9) << verify.mean_abs_error << ",\n";
    out << "  \"duration_ns\": " << std::fixed << std::setprecision(0) << duration_ns << ",\n";
    out << "  \"wall_ms\": " << std::fixed << std::setprecision(3) << wall_ms << ",\n";
    out << "  \"pipeline_executable_extension\": " << (executable.extension_enabled ? "true" : "false") << ",\n";
    out << "  \"register_count\": " << executable.register_count << ",\n";
    out << "  \"binary_size\": " << executable.binary_size << ",\n";
    out << "  \"vgpr_count\": " << executable.vgpr_count << ",\n";
    out << "  \"sgpr_count\": " << executable.sgpr_count << ",\n";
    out << "  \"spill_count\": " << executable.spill_count << ",\n";
    out << "  \"pipeline_executable_stat_count\": " << executable.stats.size() << ",\n";
    out << "  \"pipeline_executable_stats\": [\n";
    for (size_t i = 0; i < executable.stats.size(); ++i) {
        const auto& stat = executable.stats[i];
        out << "    {\"name\":\"" << json_escape(stat.name)
            << "\",\"description\":\"" << json_escape(stat.description)
            << "\",\"format\":\"" << json_escape(stat.format)
            << "\",\"value\":\"" << json_escape(stat.value) << "\"}";
        out << (i + 1 == executable.stats.size() ? "\n" : ",\n");
    }
    out << "  ]\n";
    out << "}\n";
}

void update_descriptor_set(
    VkDevice device,
    VkDescriptorSet descriptor_set,
    const Buffer& current_positions,
    const Buffer& history_positions,
    const Buffer& next_positions,
    const Buffer& alpha_per_joint,
    const Buffer& mode_per_joint,
    const Buffer& parent_index,
    const Buffer& count_buffer,
    const Buffer& time_base_buffer,
    const Buffer& round_phase_buffer,
    const Buffer& gain_buffer,
    const Buffer& eps_buffer,
    const Buffer& width_buffer) {
    VkDescriptorBufferInfo infos[12]{};
    infos[0] = VkDescriptorBufferInfo{current_positions.buffer, 0, current_positions.size};
    infos[1] = VkDescriptorBufferInfo{history_positions.buffer, 0, history_positions.size};
    infos[2] = VkDescriptorBufferInfo{next_positions.buffer, 0, next_positions.size};
    infos[3] = VkDescriptorBufferInfo{alpha_per_joint.buffer, 0, alpha_per_joint.size};
    infos[4] = VkDescriptorBufferInfo{mode_per_joint.buffer, 0, mode_per_joint.size};
    infos[5] = VkDescriptorBufferInfo{parent_index.buffer, 0, parent_index.size};
    infos[6] = VkDescriptorBufferInfo{count_buffer.buffer, 0, sizeof(uint32_t)};
    infos[7] = VkDescriptorBufferInfo{time_base_buffer.buffer, 0, sizeof(float)};
    infos[8] = VkDescriptorBufferInfo{round_phase_buffer.buffer, 0, sizeof(float)};
    infos[9] = VkDescriptorBufferInfo{gain_buffer.buffer, 0, sizeof(float)};
    infos[10] = VkDescriptorBufferInfo{eps_buffer.buffer, 0, sizeof(float)};
    infos[11] = VkDescriptorBufferInfo{width_buffer.buffer, 0, sizeof(uint32_t)};

    VkWriteDescriptorSet writes[12]{};
    for (uint32_t i = 0; i < 12; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptor_set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = i < 6 ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(device, 12, writes, 0, nullptr);
}

}  // namespace

int main() {
    const auto wall_start = std::chrono::steady_clock::now();
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkQueryPool query_pool = VK_NULL_HANDLE;
    Buffer position_a{};
    Buffer position_b{};
    Buffer position_c{};
    Buffer alpha_buffer{};
    Buffer mode_buffer{};
    Buffer parent_buffer{};
    Buffer count_buffer{};
    Buffer time_base_buffer{};
    Buffer round_phase_buffer{};
    Buffer gain_buffer{};
    Buffer eps_buffer{};
    Buffer width_buffer{};

    try {
        const std::string shader_path = env_string("KAIN_GPU_SHADER_SPV");
        if (shader_path.empty()) {
            die("KAIN_GPU_SHADER_SPV is required");
        }
        const std::string telemetry_path = env_string("KAIN_GPU_TELEMETRY_PATH");
        const std::string case_id = env_string("KAIN_GPU_CASE_ID", "semantic_ping_pong");
        const std::string language = env_string("KAIN_GPU_LANGUAGE", "unknown");
        const std::string entry_point = env_string("KAIN_GPU_ENTRY_POINT", "main");
        const uint32_t work_items = env_u32("KAIN_GPU_WORK_ITEMS", 131072u);
        const uint32_t width = env_u32("KAIN_GPU_WIDTH", 512u);
        const uint32_t rounds = env_u32("KAIN_GPU_ROUNDS", 12u);
        const float verify_epsilon = env_f32("KAIN_GPU_VERIFY_EPSILON", 0.0015f);
        const float gain = env_f32("KAIN_GPU_GAIN", 0.85f);
        const float time_step = env_f32("KAIN_GPU_TIME_STEP", 0.03125f);
        const float eps = 0.000001f;
        const uint32_t height = (work_items + width - 1u) / width;
        const uint32_t group_x = (width + 7u) / 8u;
        const uint32_t group_y = (height + 7u) / 8u;
        const std::vector<char> spirv = read_file(shader_path);

        const std::vector<Vec4f> current_seed = make_current_positions(work_items);
        const std::vector<Vec4f> history_seed = make_history_positions(work_items);
        const std::vector<float> alpha_values = make_alpha_values(work_items);
        const std::vector<int32_t> mode_values = make_mode_values(work_items);
        const std::vector<int32_t> parent_values = make_parent_values(work_items);

        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "kain-gpu-benchmark-semantic-ping-pong";
        app.apiVersion = VK_API_VERSION_1_1;
        VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance_info.pApplicationInfo = &app;
        vk_check(vkCreateInstance(&instance_info, nullptr, &instance), "vkCreateInstance");

        uint32_t physical_count = 0;
        vk_check(vkEnumeratePhysicalDevices(instance, &physical_count, nullptr), "vkEnumeratePhysicalDevices");
        if (physical_count == 0) {
            die("no Vulkan physical devices found");
        }
        std::vector<VkPhysicalDevice> physical_devices(physical_count);
        vk_check(vkEnumeratePhysicalDevices(instance, &physical_count, physical_devices.data()), "vkEnumeratePhysicalDevices");

        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        uint32_t queue_family = UINT32_MAX;
        VkPhysicalDeviceProperties physical_properties{};
        for (VkPhysicalDevice candidate : physical_devices) {
            uint32_t queue_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, nullptr);
            std::vector<VkQueueFamilyProperties> queues(queue_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_count, queues.data());
            for (uint32_t i = 0; i < queue_count; ++i) {
                if ((queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
                    physical_device = candidate;
                    queue_family = i;
                    vkGetPhysicalDeviceProperties(physical_device, &physical_properties);
                    break;
                }
            }
            if (physical_device != VK_NULL_HANDLE) {
                break;
            }
        }
        if (physical_device == VK_NULL_HANDLE) {
            die("no compute-capable Vulkan queue found");
        }

        uint32_t extension_count = 0;
        vk_check(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr), "vkEnumerateDeviceExtensionProperties");
        std::vector<VkExtensionProperties> extensions(extension_count);
        vk_check(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, extensions.data()), "vkEnumerateDeviceExtensionProperties");
        const bool executable_extension =
            has_extension(extensions, VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
        std::vector<const char*> enabled_extensions;
        if (executable_extension) {
            enabled_extensions.push_back(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);
        }

        const float queue_priority = 1.0f;
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.queueFamilyIndex = queue_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
        device_info.ppEnabledExtensionNames = enabled_extensions.data();
        vk_check(vkCreateDevice(physical_device, &device_info, nullptr, &device), "vkCreateDevice");

        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, queue_family, 0, &queue);

        const VkDeviceSize positions_size = static_cast<VkDeviceSize>(work_items) * sizeof(Vec4f);
        const VkDeviceSize scalar_float_size = static_cast<VkDeviceSize>(work_items) * sizeof(float);
        const VkDeviceSize scalar_int_size = static_cast<VkDeviceSize>(work_items) * sizeof(int32_t);
        position_a = create_buffer(device, physical_device, positions_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        position_b = create_buffer(device, physical_device, positions_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        position_c = create_buffer(device, physical_device, positions_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        alpha_buffer = create_buffer(device, physical_device, scalar_float_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        mode_buffer = create_buffer(device, physical_device, scalar_int_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        parent_buffer = create_buffer(device, physical_device, scalar_int_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        count_buffer = create_buffer(device, physical_device, sizeof(uint32_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        time_base_buffer = create_buffer(device, physical_device, sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        round_phase_buffer = create_buffer(device, physical_device, sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        gain_buffer = create_buffer(device, physical_device, sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        eps_buffer = create_buffer(device, physical_device, sizeof(float), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        width_buffer = create_buffer(device, physical_device, sizeof(uint32_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        write_vector_buffer(device, position_a, current_seed);
        write_vector_buffer(device, position_b, history_seed);
        clear_buffer(device, position_c);
        write_vector_buffer(device, alpha_buffer, alpha_values);
        write_vector_buffer(device, mode_buffer, mode_values);
        write_vector_buffer(device, parent_buffer, parent_values);
        write_scalar_buffer(device, count_buffer, work_items);
        write_scalar_buffer(device, gain_buffer, gain);
        write_scalar_buffer(device, eps_buffer, eps);
        write_scalar_buffer(device, width_buffer, width);

        VkShaderModuleCreateInfo shader_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        shader_info.codeSize = spirv.size();
        shader_info.pCode = reinterpret_cast<const uint32_t*>(spirv.data());
        vk_check(vkCreateShaderModule(device, &shader_info, nullptr, &shader_module), "vkCreateShaderModule");

        VkDescriptorSetLayoutBinding bindings[12]{};
        for (uint32_t i = 0; i < 12; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = i < 6 ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo descriptor_layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptor_layout_info.bindingCount = 12;
        descriptor_layout_info.pBindings = bindings;
        vk_check(vkCreateDescriptorSetLayout(device, &descriptor_layout_info, nullptr, &descriptor_set_layout), "vkCreateDescriptorSetLayout");

        VkPipelineLayoutCreateInfo pipeline_layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
        vk_check(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout), "vkCreatePipelineLayout");

        VkPipelineShaderStageCreateInfo stage_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage_info.module = shader_module;
        stage_info.pName = entry_point.c_str();

        VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipeline_info.flags = executable_extension ? VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR : 0;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = pipeline_layout;
        vk_check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline), "vkCreateComputePipelines");
        const ExecutableTelemetry executable_telemetry =
            read_pipeline_executable_telemetry(device, pipeline, executable_extension);

        VkDescriptorPoolSize pool_sizes[2]{};
        pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_sizes[0].descriptorCount = 6;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[1].descriptorCount = 6;
        VkDescriptorPoolCreateInfo descriptor_pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptor_pool_info.maxSets = 1;
        descriptor_pool_info.poolSizeCount = 2;
        descriptor_pool_info.pPoolSizes = pool_sizes;
        vk_check(vkCreateDescriptorPool(device, &descriptor_pool_info, nullptr, &descriptor_pool), "vkCreateDescriptorPool");

        VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo set_allocate_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        set_allocate_info.descriptorPool = descriptor_pool;
        set_allocate_info.descriptorSetCount = 1;
        set_allocate_info.pSetLayouts = &descriptor_set_layout;
        vk_check(vkAllocateDescriptorSets(device, &set_allocate_info, &descriptor_set), "vkAllocateDescriptorSets");

        VkCommandPoolCreateInfo command_pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        command_pool_info.queueFamilyIndex = queue_family;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vk_check(vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool), "vkCreateCommandPool");

        VkCommandBufferAllocateInfo command_allocate_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        command_allocate_info.commandPool = command_pool;
        command_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_allocate_info.commandBufferCount = 1;
        vk_check(vkAllocateCommandBuffers(device, &command_allocate_info, &command_buffer), "vkAllocateCommandBuffers");

        VkQueryPoolCreateInfo query_info{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        query_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        query_info.queryCount = 2;
        vk_check(vkCreateQueryPool(device, &query_info, nullptr, &query_pool), "vkCreateQueryPool");

        VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vk_check(vkCreateFence(device, &fence_info, nullptr, &fence), "vkCreateFence");

        Buffer* current_gpu = &position_a;
        Buffer* history_gpu = &position_b;
        Buffer* next_gpu = &position_c;
        double total_duration_ns = 0.0;
        for (uint32_t round = 0; round < rounds; ++round) {
            const float time_base = time_step * static_cast<float>(round + 1u);
            const float round_phase = static_cast<float>(round) * 0.25f;
            write_scalar_buffer(device, time_base_buffer, time_base);
            write_scalar_buffer(device, round_phase_buffer, round_phase);

            update_descriptor_set(
                device,
                descriptor_set,
                *current_gpu,
                *history_gpu,
                *next_gpu,
                alpha_buffer,
                mode_buffer,
                parent_buffer,
                count_buffer,
                time_base_buffer,
                round_phase_buffer,
                gain_buffer,
                eps_buffer,
                width_buffer);

            vk_check(vkResetFences(device, 1, &fence), "vkResetFences");
            vk_check(vkResetCommandBuffer(command_buffer, 0), "vkResetCommandBuffer");

            VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vk_check(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer");
            vkCmdResetQueryPool(command_buffer, query_pool, 0, 2);
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, 0);
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
            vkCmdDispatch(command_buffer, group_x, group_y, 1);
            VkMemoryBarrier memory_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            memory_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1,
                &memory_barrier,
                0,
                nullptr,
                0,
                nullptr);
            vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, 1);
            vk_check(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");

            VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;
            vk_check(vkQueueSubmit(queue, 1, &submit_info, fence), "vkQueueSubmit");
            vk_check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");

            uint64_t timestamps[2]{};
            if (vkGetQueryPoolResults(
                    device,
                    query_pool,
                    0,
                    2,
                    sizeof(timestamps),
                    timestamps,
                    sizeof(uint64_t),
                    VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS &&
                timestamps[1] >= timestamps[0]) {
                total_duration_ns += static_cast<double>(timestamps[1] - timestamps[0]) *
                    static_cast<double>(physical_properties.limits.timestampPeriod);
            }

            Buffer* reusable = history_gpu;
            history_gpu = current_gpu;
            current_gpu = next_gpu;
            next_gpu = reusable;
        }

        const std::vector<Vec4f> expected = simulate_rounds(
            current_seed,
            history_seed,
            alpha_values,
            mode_values,
            parent_values,
            rounds,
            time_step,
            gain,
            eps);
        const VerifyResult verify = verify_positions(device, *current_gpu, expected, verify_epsilon);
        const auto wall_end = std::chrono::steady_clock::now();
        const double wall_ms =
            std::chrono::duration<double, std::milli>(wall_end - wall_start).count();
        write_telemetry(
            telemetry_path,
            case_id,
            language,
            shader_path,
            entry_point,
            physical_properties.deviceName,
            work_items,
            width,
            rounds,
            verify_epsilon,
            gain,
            time_step,
            verify,
            total_duration_ns,
            wall_ms,
            executable_telemetry);
        if (verify.mismatch_count != 0) {
            std::cerr << "verification failed with " << verify.mismatch_count
                << " mismatches, max_abs_error=" << verify.max_abs_error << "\n";
            return 2;
        }

        if (fence) vkDestroyFence(device, fence, nullptr);
        if (query_pool) vkDestroyQueryPool(device, query_pool, nullptr);
        if (command_pool) vkDestroyCommandPool(device, command_pool, nullptr);
        if (descriptor_pool) vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        if (pipeline_layout) vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        if (descriptor_set_layout) vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
        if (shader_module) vkDestroyShaderModule(device, shader_module, nullptr);
        destroy_buffer(device, width_buffer);
        destroy_buffer(device, eps_buffer);
        destroy_buffer(device, gain_buffer);
        destroy_buffer(device, round_phase_buffer);
        destroy_buffer(device, time_base_buffer);
        destroy_buffer(device, count_buffer);
        destroy_buffer(device, parent_buffer);
        destroy_buffer(device, mode_buffer);
        destroy_buffer(device, alpha_buffer);
        destroy_buffer(device, position_c);
        destroy_buffer(device, position_b);
        destroy_buffer(device, position_a);
        if (device) vkDestroyDevice(device, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << "\n";
        if (fence) vkDestroyFence(device, fence, nullptr);
        if (query_pool) vkDestroyQueryPool(device, query_pool, nullptr);
        if (command_pool) vkDestroyCommandPool(device, command_pool, nullptr);
        if (descriptor_pool) vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        if (pipeline_layout) vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        if (descriptor_set_layout) vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
        if (shader_module) vkDestroyShaderModule(device, shader_module, nullptr);
        destroy_buffer(device, width_buffer);
        destroy_buffer(device, eps_buffer);
        destroy_buffer(device, gain_buffer);
        destroy_buffer(device, round_phase_buffer);
        destroy_buffer(device, time_base_buffer);
        destroy_buffer(device, count_buffer);
        destroy_buffer(device, parent_buffer);
        destroy_buffer(device, mode_buffer);
        destroy_buffer(device, alpha_buffer);
        destroy_buffer(device, position_c);
        destroy_buffer(device, position_b);
        destroy_buffer(device, position_a);
        if (device) vkDestroyDevice(device, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
        return 1;
    }
}
