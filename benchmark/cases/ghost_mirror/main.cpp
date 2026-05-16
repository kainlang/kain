#include <array>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
using SocketLength = int;
constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
using SocketLength = socklen_t;
constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif

constexpr std::size_t UPDATES = 64;
constexpr std::size_t BYTES_PER_PAYLOAD = 1'048'576;
constexpr std::uint64_t EXPECTED_CHECKSUM = 2'080;
constexpr std::uint64_t MODULUS = 1'000'000'007ULL;

void close_socket(SocketHandle socket_handle) {
#ifdef _WIN32
    if (socket_handle != INVALID_SOCKET_HANDLE) {
        closesocket(socket_handle);
    }
#else
    if (socket_handle != INVALID_SOCKET_HANDLE) {
        close(socket_handle);
    }
#endif
}

bool send_all(SocketHandle socket_handle, const std::uint8_t* data, std::size_t length) {
    std::size_t sent_total = 0;
    while (sent_total < length) {
#ifdef _WIN32
        const int sent = send(
            socket_handle,
            reinterpret_cast<const char*>(data + sent_total),
            static_cast<int>(length - sent_total),
            0
        );
#else
        const auto sent = send(socket_handle, data + sent_total, length - sent_total, 0);
#endif
        if (sent <= 0) {
            return false;
        }
        sent_total += static_cast<std::size_t>(sent);
    }
    return true;
}

bool recv_all(SocketHandle socket_handle, std::uint8_t* data, std::size_t length) {
    std::size_t received_total = 0;
    while (received_total < length) {
#ifdef _WIN32
        const int received = recv(
            socket_handle,
            reinterpret_cast<char*>(data + received_total),
            static_cast<int>(length - received_total),
            0
        );
#else
        const auto received = recv(socket_handle, data + received_total, length - received_total, 0);
#endif
        if (received <= 0) {
            return false;
        }
        received_total += static_cast<std::size_t>(received);
    }
    return true;
}

std::uint64_t load_le_u64(const std::uint8_t* bytes) {
    std::uint64_t value = 0;
    for (int offset = 0; offset < 8; ++offset) {
        value |= static_cast<std::uint64_t>(bytes[offset]) << (offset * 8);
    }
    return value;
}

void store_le_u64(std::uint8_t* bytes, std::uint64_t value) {
    for (int offset = 0; offset < 8; ++offset) {
        bytes[offset] = static_cast<std::uint8_t>((value >> (offset * 8)) & 0xffU);
    }
}

std::uint64_t read_exact_payload(SocketHandle socket_handle) {
    std::vector<std::uint8_t> payload(BYTES_PER_PAYLOAD);
    std::uint64_t checksum = 0;
    for (std::size_t update = 0; update < UPDATES; ++update) {
        std::array<std::uint8_t, 8> header{};
        if (!recv_all(socket_handle, header.data(), header.size())) {
            return 0;
        }
        const std::uint64_t revision = load_le_u64(header.data());
        if (!recv_all(socket_handle, payload.data(), payload.size())) {
            return 0;
        }
        checksum = (checksum + revision) % MODULUS;
    }
    return checksum;
}

struct WinsockScope {
    bool ok = true;

    WinsockScope() {
#ifdef _WIN32
        WSADATA data{};
        ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#endif
    }

    ~WinsockScope() {
#ifdef _WIN32
        if (ok) {
            WSACleanup();
        }
#endif
    }
};

int main() {
    WinsockScope winsock;
    if (!winsock.ok) {
        return 1;
    }

    SocketHandle listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET_HANDLE) {
        return 1;
    }

    sockaddr_in bind_address{};
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons(0);
    bind_address.sin_addr.s_addr = htonl(0x7f000001U);

    if (bind(listener, reinterpret_cast<sockaddr*>(&bind_address), sizeof(bind_address)) != 0) {
        close_socket(listener);
        return 1;
    }

    if (listen(listener, 1) != 0) {
        close_socket(listener);
        return 1;
    }

    sockaddr_in local_address{};
    SocketLength local_length = static_cast<SocketLength>(sizeof(local_address));
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&local_address), &local_length) != 0) {
        close_socket(listener);
        return 1;
    }

    const std::uint16_t port = ntohs(local_address.sin_port);
    SocketHandle stream = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (stream == INVALID_SOCKET_HANDLE) {
        close_socket(listener);
        return 1;
    }

    sockaddr_in connect_address{};
    connect_address.sin_family = AF_INET;
    connect_address.sin_port = htons(port);
    connect_address.sin_addr.s_addr = htonl(0x7f000001U);
    if (connect(stream, reinterpret_cast<sockaddr*>(&connect_address), sizeof(connect_address)) != 0) {
        close_socket(stream);
        close_socket(listener);
        return 1;
    }

    std::uint64_t checksum = 0;
    bool receiver_ok = false;
    std::thread receiver([listener, &checksum, &receiver_ok]() mutable {
        SocketHandle accepted = accept(listener, nullptr, nullptr);
        close_socket(listener);
        if (accepted == INVALID_SOCKET_HANDLE) {
            return;
        }
        checksum = read_exact_payload(accepted);
        close_socket(accepted);
        receiver_ok = true;
    });

    std::vector<std::uint8_t> payload(BYTES_PER_PAYLOAD);
    for (std::size_t revision = 1; revision <= UPDATES; ++revision) {
        const std::uint8_t seed = static_cast<std::uint8_t>(revision & 0xffU);
        std::size_t index = 0;
        while (index < payload.size()) {
            payload[index] = static_cast<std::uint8_t>(seed + static_cast<std::uint8_t>(index & 0xffU));
            index += 4096;
        }
        std::array<std::uint8_t, 8> header{};
        store_le_u64(header.data(), static_cast<std::uint64_t>(revision));
        if (!send_all(stream, header.data(), header.size()) || !send_all(stream, payload.data(), payload.size())) {
            close_socket(stream);
            receiver.join();
            return 1;
        }
    }

    close_socket(stream);
    receiver.join();
    return receiver_ok && checksum == EXPECTED_CHECKSUM ? 0 : 1;
}
