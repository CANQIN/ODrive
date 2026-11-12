#pragma once

#include "socket.hpp"
#include <fibre/async_stream.hpp>

namespace example {

// 下位机桌面模拟：处理 TcpListener 接收的连接，Fibre 负责流分帧。
class TcpTransport final : public fibre::AsyncStreamSource,
                           public fibre::AsyncStreamSink {
public:
    void start_read(fibre::bufptr_t buffer, fibre::TransferHandle* handle,
                    fibre::Callback<void, fibre::ReadResult> completer) final {
        if (handle) *handle = reinterpret_cast<fibre::TransferHandle>(this);
        read_buffer_ = buffer;
        read_completer_ = completer;
    }
    void cancel_read(fibre::TransferHandle) final {
        read_completer_.invoke_and_clear({fibre::kStreamCancelled, read_buffer_.begin()});
    }
    void start_write(fibre::cbufptr_t buffer, fibre::TransferHandle* handle,
                     fibre::Callback<void, fibre::WriteResult> completer) final {
        if (handle) *handle = reinterpret_cast<fibre::TransferHandle>(this);
        write_buffer_ = buffer;
        write_completer_ = completer;
    }
    void cancel_write(fibre::TransferHandle) final {
        write_completer_.invoke_and_clear({fibre::kStreamCancelled, write_buffer_.begin()});
    }

    void poll(int timeout_ms) {
        fd_set readers, writers;
        FD_ZERO(&readers);
        FD_ZERO(&writers);
        if (read_completer_) FD_SET(socket_.handle, &readers);
        if (write_completer_) FD_SET(socket_.handle, &writers);
        if (!read_completer_ && !write_completer_) return;
        timeval timeout{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        int ready = ::select(static_cast<int>(socket_.handle + 1), &readers, &writers, nullptr, &timeout);
        if (ready == 0 || (ready < 0 && Socket::interrupted())) return;
        if (ready < 0) {
            read_completer_.invoke_and_clear({fibre::kStreamError, read_buffer_.begin()});
            write_completer_.invoke_and_clear({fibre::kStreamError, write_buffer_.begin()});
            return;
        }
        if (read_completer_ && FD_ISSET(socket_.handle, &readers)) {
            int received = static_cast<int>(::recv(socket_.handle,
                reinterpret_cast<char*>(read_buffer_.begin()), static_cast<int>(read_buffer_.size()), 0));
            if (received > 0) {
                read_completer_.invoke_and_clear({fibre::kStreamOk, read_buffer_.begin() + received});
            } else if (received == 0 || (!Socket::would_block() && !Socket::interrupted())) {
                read_completer_.invoke_and_clear(
                    {received == 0 ? fibre::kStreamClosed : fibre::kStreamError, read_buffer_.begin()});
            }
        }
        if (write_completer_ && FD_ISSET(socket_.handle, &writers)) {
            int flags = 0;
#ifdef MSG_NOSIGNAL
            flags = MSG_NOSIGNAL;
#endif
            int sent = static_cast<int>(::send(socket_.handle,
                reinterpret_cast<const char*>(write_buffer_.begin()),
                static_cast<int>(write_buffer_.size()), flags));
            if (sent >= 0) {
                // 允许短写；上层 PacketWrapper 会继续发送剩余字节。
                write_completer_.invoke_and_clear({fibre::kStreamOk, write_buffer_.begin() + sent});
            } else if (!Socket::would_block() && !Socket::interrupted()) {
                write_completer_.invoke_and_clear({fibre::kStreamError, write_buffer_.begin()});
            }
        }
    }

private:
    friend class TcpListener;
    Socket socket_;
    fibre::bufptr_t read_buffer_;
    fibre::cbufptr_t write_buffer_;
    fibre::Callback<void, fibre::ReadResult> read_completer_;
    fibre::Callback<void, fibre::WriteResult> write_completer_;
};

// 示例一次服务一个连接；连接断开后，主循环重新创建协议实例。
class TcpListener {
public:
    explicit TcpListener(uint16_t port) {
        socket_.open(SOCK_STREAM);
#ifndef _WIN32
        int enabled = 1;
        if (::setsockopt(socket_.handle, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) != 0) {
            throw std::runtime_error("SO_REUSEADDR: " + std::to_string(Socket::error()));
        }
#endif
        socket_.bind(port);
        if (::listen(socket_.handle, 1) != 0) {
            throw std::runtime_error("listen: " + std::to_string(Socket::error()));
        }
    }
    bool accept(TcpTransport& transport, int timeout_ms) {
        int ready = socket_.wait_readable(timeout_ms);
        if (ready == 0) return false;
        if (ready < 0) throw std::runtime_error("select: " + std::to_string(Socket::error()));
        transport.socket_.handle = ::accept(socket_.handle, nullptr, nullptr);
        if (transport.socket_.handle == Socket::invalid) {
            throw std::runtime_error("accept: " + std::to_string(Socket::error()));
        }
        transport.socket_.configure_tcp();
        return true;
    }
private:
    Socket socket_;
};

} // namespace example
