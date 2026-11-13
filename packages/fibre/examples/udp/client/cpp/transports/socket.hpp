#pragma once

// 电脑上位机的 Windows / POSIX socket 适配，由客户端工程独立维护。
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace example {

class Socket {
public:
#ifdef _WIN32
    using Handle = SOCKET;
    static constexpr Handle invalid = INVALID_SOCKET;
#else
    using Handle = int;
    static constexpr Handle invalid = -1;
#endif
    Handle handle = invalid;

    Socket() {
#ifdef _WIN32
        WSADATA data;
        int status = WSAStartup(MAKEWORD(2, 2), &data);
        if (status != 0) throw std::runtime_error("WSAStartup: " + std::to_string(status));
#endif
    }
    ~Socket() {
#ifdef _WIN32
        if (handle != invalid) closesocket(handle);
        WSACleanup();
#else
        if (handle != invalid) ::close(handle);
#endif
    }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    static int error() {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }
    static bool interrupted() {
#ifdef _WIN32
        return error() == WSAEINTR;
#else
        return error() == EINTR;
#endif
    }
    static bool would_block() {
#ifdef _WIN32
        return error() == WSAEWOULDBLOCK;
#else
        return error() == EAGAIN || error() == EWOULDBLOCK;
#endif
    }
    static sockaddr_in address(uint16_t port) {
        sockaddr_in result{};
        result.sin_family = AF_INET;
        result.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        result.sin_port = htons(port);
        return result;
    }
    void open(int type) {
        handle = ::socket(AF_INET, type, 0);
        if (handle == invalid) throw std::runtime_error("socket: " + std::to_string(error()));
    }
    void bind(uint16_t port) {
        auto local = address(port);
        if (::bind(handle, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0) {
            throw std::runtime_error("bind: " + std::to_string(error()));
        }
    }
    int wait_readable(int timeout_ms) {
        fd_set readers;
        FD_ZERO(&readers);
        FD_SET(handle, &readers);
        timeval timeout{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        int result = ::select(static_cast<int>(handle + 1), &readers, nullptr, nullptr, &timeout);
        return result < 0 && interrupted() ? 0 : result;
    }
    void configure_tcp() {
        int enabled = 1;
        if (::setsockopt(handle, IPPROTO_TCP, TCP_NODELAY,
                         reinterpret_cast<const char*>(&enabled), sizeof(enabled)) != 0) {
            throw std::runtime_error("TCP_NODELAY: " + std::to_string(error()));
        }
#ifdef SO_NOSIGPIPE
        if (::setsockopt(handle, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0) {
            throw std::runtime_error("SO_NOSIGPIPE: " + std::to_string(error()));
        }
#endif
#ifdef _WIN32
        u_long nonblocking = 1;
        int status = ioctlsocket(handle, FIONBIO, &nonblocking);
#else
        int flags = fcntl(handle, F_GETFL, 0);
        int status = flags < 0 ? -1 : fcntl(handle, F_SETFL, flags | O_NONBLOCK);
#endif
        if (status != 0) throw std::runtime_error("nonblocking: " + std::to_string(error()));
    }
};

} // namespace example
