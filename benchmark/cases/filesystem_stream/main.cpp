#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

constexpr std::int64_t ROUNDS = 80;
constexpr std::int64_t EXPECTED = 6'846'690;

std::string build_payload(std::size_t line_count) {
    std::string text;
    for (std::size_t index = 0; index < line_count; ++index) {
        text += "line-";
        text += std::to_string(index % 97);
        text += "-orbital-flux\n";
    }
    return text;
}

std::int64_t copy_streaming(const std::string& source, const std::string& dest) {
    std::ifstream reader(source, std::ios::binary);
    std::ofstream writer(dest, std::ios::binary | std::ios::trunc);
    std::array<char, 256> buffer{};
    std::int64_t total = 0;
    while (reader) {
        reader.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize read = reader.gcount();
        if (read <= 0) {
            break;
        }
        writer.write(buffer.data(), read);
        total += static_cast<std::int64_t>(read);
    }
    writer.flush();
    return total;
}

int main() {
    const std::string payload = build_payload(2'048);
    const char* temp_root = std::getenv("TEMP");
    const std::string prefix = temp_root && temp_root[0] != '\0' ? std::string(temp_root) + "\\" : std::string();
    const std::string source_path = prefix + "kain-benchmark-fs-source.txt";
    const std::string dest_path = prefix + "kain-benchmark-fs-copy.txt";

    std::int64_t acc = 0;
    for (std::int64_t index = 0; index < ROUNDS; ++index) {
        {
            std::ofstream source(source_path, std::ios::binary | std::ios::trunc);
            source.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }
        const std::int64_t copied = copy_streaming(source_path, dest_path);
        std::ifstream dest(dest_path, std::ios::binary);
        std::string readback((std::istreambuf_iterator<char>(dest)), std::istreambuf_iterator<char>());
        if (readback != payload) {
            return 1;
        }
        acc += copied + static_cast<std::int64_t>(readback.size()) + (index % 17);
    }

    std::remove(source_path.c_str());
    std::remove(dest_path.c_str());

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
