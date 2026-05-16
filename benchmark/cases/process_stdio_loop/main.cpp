#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

constexpr std::int64_t ROUNDS = 300;
constexpr std::int64_t EXPECTED = 5'988;

int main() {
    std::int64_t acc = 0;
    for (std::int64_t index = 0; index < ROUNDS; ++index) {
        FILE* handle = _popen("cmd.exe /d /c echo process-bench", "rb");
        if (handle == nullptr) {
            return 1;
        }
        std::array<char, 256> buffer{};
        std::string stdout_text;
        while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), handle) != nullptr) {
            stdout_text += buffer.data();
        }
        const int status = _pclose(handle);
        if (status != 0) {
            return 1;
        }
        if (stdout_text != "process-bench\r\n") {
            return 1;
        }
        acc += static_cast<std::int64_t>(stdout_text.size()) + (index % 11);
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
