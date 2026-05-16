#include <cstdint>
#include <string>

constexpr std::int64_t ROUNDS = 250'000;
constexpr std::int64_t MODULUS = 1'000'000'007;
constexpr std::int64_t EXPECTED = 35'749'995;
constexpr const char* PAYLOAD_A = "{\"id\":17,\"name\":\"orbital\",\"enabled\":true,\"count\":42}";
constexpr const char* PAYLOAD_B = "{\"id\":23,\"name\":\"lattice\",\"enabled\":false,\"count\":57}";

std::int64_t parse_positive_int(const std::string& text, std::size_t start) {
    std::size_t index = start;
    std::int64_t value = 0;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
        value = (value * 10) + static_cast<std::int64_t>(text[index] - '0');
        ++index;
    }
    return value;
}

std::int64_t parse_int_field(const std::string& text, const std::string& key) {
    const std::size_t start = text.find(key) + key.size();
    return parse_positive_int(text, start);
}

std::string parse_name_field(const std::string& text) {
    const std::string key = "\"name\":\"";
    const std::size_t start = text.find(key) + key.size();
    const std::size_t finish = text.find('"', start);
    return text.substr(start, finish - start);
}

bool parse_enabled_field(const std::string& text) {
    const std::string key = "\"enabled\":";
    const std::size_t start = text.find(key) + key.size();
    return text.compare(start, 4, "true") == 0;
}

std::string render_payload(std::int64_t id, const std::string& name, bool enabled, std::int64_t count) {
    return "{\"id\":" + std::to_string(id) + ",\"name\":\"" + name + "\",\"enabled\":" + (enabled ? "true" : "false") + ",\"count\":" + std::to_string(count) + "}";
}

int main() {
    std::int64_t acc = 0;
    for (std::int64_t index = 0; index < ROUNDS; ++index) {
        const std::string payload = (index & 1) == 0 ? PAYLOAD_A : PAYLOAD_B;
        const std::int64_t id = parse_int_field(payload, "\"id\":");
        const std::string name = parse_name_field(payload);
        const bool enabled = parse_enabled_field(payload);
        const std::int64_t count = parse_int_field(payload, "\"count\":");
        const std::string rendered = render_payload(id, name, enabled, count);
        if (rendered != payload) {
            return 1;
        }
        const std::int64_t enabled_score = enabled ? 17 : 5;
        acc = (acc + id + count + static_cast<std::int64_t>(name.size()) + enabled_score +
               static_cast<std::int64_t>(rendered.size()) + (index % 7)) %
              MODULUS;
    }

    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
