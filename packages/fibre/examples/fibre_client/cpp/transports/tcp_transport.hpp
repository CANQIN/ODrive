#pragma once

#include "socket.hpp"
#include <fibre/async_stream.hpp>

namespace example {

// 电脑上位机：主动连接设备。TCP 交付字节流，Fibre 负责流分帧。
class TcpTransport final : public fibre::AsyncStreamSource,
                           public fibre::AsyncStreamSink {
public:
    void connect(uint16_t port) {
        socket_.open(SOCK_STREAM);
        auto peer = Socket::address(port);
        if (::connect(socket_.handle, reinterpret_cast<const sockaddr*>(&peer), sizeof(peer)) != 0) {
            throw std::runtime_error("connect: " + std::to_string(Socket::error()));
        }
        socket_.configure_tcp();
    }

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
    Socket socket_;
    fibre::bufptr_t read_buffer_;
    fibre::cbufptr_t write_buffer_;
    fibre::Callback<void, fibre::ReadResult> read_completer_;
    fibre::Callback<void, fibre::WriteResult> write_completer_;
};

} // namespace example
