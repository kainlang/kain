#include <vulkan/vulkan.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdint>
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

struct Vec4f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct TelemetryStat {
    std::string name;
    std::string description;
    std::string format;
    std::string value;
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

void write_u32_buffer(VkDevice device, const Buffer& buffer, uint32_t value) {
    void* mapped = nullptr;
    vk_check(vkMapMemory(device, buffer.memory, 0, sizeof(uint32_t), 0, &mapped), "vkMapMemory");
    std::memcpy(mapped, &value, sizeof(uint32_t));
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

void fill_src_buffer(VkDevice device, const Buffer& buffer, uint32_t count) {
    float* mapped = nullptr;
    vk_check(vkMapMemory(device, buffer.memory, 0, buffer.size, 0, reinterpret_cast<void**>(&mapped)), "vkMapMemory");
    for (uint32_t i = 0; i < count; ++i) {
        mapped[i * 4 + 0] = static_cast<float>(((i * 3u) % 4096u) + 1u);
        mapped[i * 4 + 1] = static_cast<float>(((i * 5u) % 4096u) + 7u);
        mapped[i * 4 + 2] = static_cast<float>(((i * 7u) % 4096u) + 13u);
        mapped[i * 4 + 3] = static_cast<float>(((i * 11u) % 4096u) + 17u);
    }
    vkUnmapMemory(device, buffer.memory);
}

void clear_dst_buffer(VkDevice device, const Buffer& buffer) {
    void* mapped = nullptr;
    vk_check(vkMapMemory(device, buffer.memory, 0, buffer.size, 0, &mapped), "vkMapMemory");
    std::memset(mapped, 0, static_cast<size_t>(buffer.size));
    vkUnmapMemory(device, buffer.memory);
}

struct VerifyResult {
    uint64_t checksum = 1469598103934665603ull;
    uint32_t mismatch_count = 0;
};

Vec4f load_packet(const float* values, uint32_t index) {
    return Vec4f{
        values[index * 4 + 0],
        values[index * 4 + 1],
        values[index * 4 + 2],
        values[index * 4 + 3],
    };
}

Vec4f transform_packet(const Vec4f& packet, uint32_t idx, uint32_t bias) {
    const uint32_t lane = (idx + bias) % 6u;
    const float bias_value = static_cast<float>(bias);
    const float step = static_cast<float>((idx % 97u) + 1u);
    const float a = packet.x + step + bias_value;
    const float b = packet.y + step + 17.0f;
    const float c = packet.z + packet.x + 23.0f;
    const float d = packet.w + packet.y + 31.0f;
    if (lane == 0u) {
        return Vec4f{a + b, b + d, c + a, d + step};
    }
    if (lane == 1u) {
        return Vec4f{a + c, b + packet.x + 29.0f, c + d, d + packet.z};
    }
    if (lane == 2u) {
        return Vec4f{a + d + 7.0f, b + a + 31.0f, c + packet.y + 3.0f, d + c + 5.0f};
    }
    if (lane == 3u) {
        return Vec4f{a + packet.w + step, b + c + 43.0f, c + packet.x + 11.0f, d + a + 13.0f};
    }
    if (lane == 4u) {
        return Vec4f{a + packet.y + 59.0f, b + d + 61.0f, c + packet.w + 15.0f, d + packet.x + 19.0f};
    }
    return Vec4f{a + b + 71.0f, b + packet.z + 73.0f, c + d + 79.0f, d + packet.y + 83.0f};
}

VerifyResult verify_packets(VkDevice device, const Buffer& src, const Buffer& dst, uint32_t count, uint32_t bias) {
    void* src_raw = nullptr;
    void* dst_raw = nullptr;
    vk_check(vkMapMemory(device, src.memory, 0, src.size, 0, &src_raw), "vkMapMemory");
    vk_check(vkMapMemory(device, dst.memory, 0, dst.size, 0, &dst_raw), "vkMapMemory");
    const float* src_map = static_cast<const float*>(src_raw);
    const float* dst_map = static_cast<const float*>(dst_raw);

    VerifyResult result{};
    for (uint32_t i = 0; i < count; ++i) {
        const Vec4f packet = load_packet(src_map, i);
        const Vec4f expected = transform_packet(packet, i, bias);
        const Vec4f actual = load_packet(dst_map, i);
        const uint32_t expected_bits[4] = {
            float_bits(expected.x),
            float_bits(expected.y),
            float_bits(expected.z),
            float_bits(expected.w),
        };
        const uint32_t actual_bits[4] = {
            float_bits(actual.x),
            float_bits(actual.y),
            float_bits(actual.z),
            float_bits(actual.w),
        };
        for (uint32_t lane = 0; lane < 4; ++lane) {
            result.checksum = fnv_mix(result.checksum, actual_bits[lane]);
            if (expected_bits[lane] != actual_bits[lane]) {
                ++result.mismatch_count;
                if (result.mismatch_count > 16) {
                    vkUnmapMemory(device, dst.memory);
                    vkUnmapMemory(device, src.memory);
                    return result;
                }
            }
        }
    }

    vkUnmapMemory(device, dst.memory);
    vkUnmapMemory(device, src.memory);
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

struct ExecutableTelemetry {
    bool extension_enabled = false;
    uint64_t register_count = 0;
    uint64_t binary_size = 0;
    uint64_t vgpr_count = 0;
    uint64_t sgpr_count = 0;
    uint64_t spill_count = 0;
    std::vector<TelemetryStat> stats;
};

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
    out << "  \"checksum\": " << verify.checksum << ",\n";
    out << "  \"mismatch_count\": " << verify.mismatch_count << ",\n";
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
    Buffer src_buffer{};
    Buffer dst_buffer{};
    Buffer count_buffer{};
    Buffer width_buffer{};
    Buffer bias_buffer{};

    try {
        const std::string shader_path = env_string("KAIN_GPU_SHADER_SPV");
        if (shader_path.empty()) {
            die("KAIN_GPU_SHADER_SPV is required");
        }
        const std::string telemetry_path = env_string("KAIN_GPU_TELEMETRY_PATH");
        const std::string case_id = env_string("KAIN_GPU_CASE_ID", "packed_route_scatter");
        const std::string language = env_string("KAIN_GPU_LANGUAGE", "unknown");
        const std::string entry_point = env_string("KAIN_GPU_ENTRY_POINT", "main");
        const uint32_t work_items = env_u32("KAIN_GPU_WORK_ITEMS", 1024u * 1024u);
        const uint32_t width = env_u32("KAIN_GPU_WIDTH", 1024u);
        const uint32_t bias = env_u32("KAIN_GPU_BIAS", 19u);
        const uint32_t height = (work_items + width - 1u) / width;
        const uint32_t group_x = (width + 7u) / 8u;
        const uint32_t group_y = (height + 7u) / 8u;
        const std::vector<char> spirv = read_file(shader_path);

        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "kain-gpu-benchmark-packed-route-scatter";
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

        const VkDeviceSize packet_stride_bytes = 16;
        const VkDeviceSize storage_size = static_cast<VkDeviceSize>(work_items) * packet_stride_bytes;
        src_buffer = create_buffer(device, physical_device, storage_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        dst_buffer = create_buffer(device, physical_device, storage_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        count_buffer = create_buffer(device, physical_device, sizeof(uint32_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        width_buffer = create_buffer(device, physical_device, sizeof(uint32_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        bias_buffer = create_buffer(device, physical_device, sizeof(uint32_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        fill_src_buffer(device, src_buffer, work_items);
        clear_dst_buffer(device, dst_buffer);
        write_u32_buffer(device, count_buffer, work_items);
        write_u32_buffer(device, width_buffer, width);
        write_u32_buffer(device, bias_buffer, bias);

        VkShaderModuleCreateInfo shader_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        shader_info.codeSize = spirv.size();
        shader_info.pCode = reinterpret_cast<const uint32_t*>(spirv.data());
        vk_check(vkCreateShaderModule(device, &shader_info, nullptr, &shader_module), "vkCreateShaderModule");

        VkDescriptorSetLayoutBinding bindings[5]{};
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[1] = bindings[0];
        bindings[1].binding = 1;
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[3] = bindings[2];
        bindings[3].binding = 3;
        bindings[4] = bindings[2];
        bindings[4].binding = 4;

        VkDescriptorSetLayoutCreateInfo descriptor_layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptor_layout_info.bindingCount = 5;
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
        pool_sizes[0].descriptorCount = 2;
        pool_sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pool_sizes[1].descriptorCount = 3;
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

        VkDescriptorBufferInfo src_info{src_buffer.buffer, 0, src_buffer.size};
        VkDescriptorBufferInfo dst_info{dst_buffer.buffer, 0, dst_buffer.size};
        VkDescriptorBufferInfo count_info{count_buffer.buffer, 0, sizeof(uint32_t)};
        VkDescriptorBufferInfo width_info{width_buffer.buffer, 0, sizeof(uint32_t)};
        VkDescriptorBufferInfo bias_info{bias_buffer.buffer, 0, sizeof(uint32_t)};
        VkWriteDescriptorSet writes[5]{};
        for (uint32_t i = 0; i < 5; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = descriptor_set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = i < 2 ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        writes[0].pBufferInfo = &src_info;
        writes[1].pBufferInfo = &dst_info;
        writes[2].pBufferInfo = &count_info;
        writes[3].pBufferInfo = &width_info;
        writes[4].pBufferInfo = &bias_info;
        vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);

        VkCommandPoolCreateInfo command_pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        command_pool_info.queueFamilyIndex = queue_family;
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

        VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vk_check(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer");
        vkCmdResetQueryPool(command_buffer, query_pool, 0, 2);
        vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, 0);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
        vkCmdDispatch(command_buffer, group_x, group_y, 1);
        vkCmdWriteTimestamp(command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, 1);
        vk_check(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer");

        VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vk_check(vkCreateFence(device, &fence_info, nullptr, &fence), "vkCreateFence");
        VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;
        vk_check(vkQueueSubmit(queue, 1, &submit_info, fence), "vkQueueSubmit");
        vk_check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");

        uint64_t timestamps[2]{};
        double duration_ns = 0.0;
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
            duration_ns = static_cast<double>(timestamps[1] - timestamps[0]) *
                static_cast<double>(physical_properties.limits.timestampPeriod);
        }

        const VerifyResult verify = verify_packets(device, src_buffer, dst_buffer, work_items, bias);
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
            verify,
            duration_ns,
            wall_ms,
            executable_telemetry);
        if (verify.mismatch_count != 0) {
            std::cerr << "verification failed with " << verify.mismatch_count << " mismatches\n";
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
        destroy_buffer(device, bias_buffer);
        destroy_buffer(device, width_buffer);
        destroy_buffer(device, count_buffer);
        destroy_buffer(device, dst_buffer);
        destroy_buffer(device, src_buffer);
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
        destroy_buffer(device, bias_buffer);
        destroy_buffer(device, width_buffer);
        destroy_buffer(device, count_buffer);
        destroy_buffer(device, dst_buffer);
        destroy_buffer(device, src_buffer);
        if (device) vkDestroyDevice(device, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
        return 1;
    }
}
