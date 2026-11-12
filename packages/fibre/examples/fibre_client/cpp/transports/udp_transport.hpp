#pragma once

#include "socket.hpp"
#include <fibre/async_stream.hpp>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

namespace example {

// 电脑上位机：向指定设备发送请求，只接收该设备的响应。
class UdpTransport final : public fibre::AsyncStreamSource,
                           public fibre::AsyncStreamSink {
public:
    void open(uint16_t server_port) {
        socket_.open(SOCK_DGRAM);
        socket_.bind(0);
        peer_ = Socket::address(server_port);
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
        int sent = static_cast<int>(::sendto(socket_.handle,
            reinterpret_cast<const char*>(buffer.begin()), static_cast<int>(buffer.size()), 0,
            reinterpret_cast<const sockaddr*>(&peer_), sizeof(peer_)));
        bool success = sent == static_cast<int>(buffer.size());
        completer.invoke({success ? fibre::kStreamOk : fibre::kStreamError,
                          success ? buffer.end() : buffer.begin()});
    }

    // sendto 在 start_write 内完成，不保留待取消的发送请求。
    void cancel_write(fibre::TransferHandle) final {}

    // 主循环驱动接收；超时后交还控制权，便于处理退出信号和调用超时。
    void poll(int timeout_ms) {
        if (!read_completer_) return;
        int ready = socket_.wait_readable(timeout_ms);
        if (ready == 0) return;
        if (ready < 0) {
            read_completer_.invoke_and_clear({fibre::kStreamError, read_buffer_.begin()});
            return;
        }

        // 比协议接收缓冲区多一个字节，用于识别超长数据报，避免把截断包当成完整包。
        uint8_t packet[129];
        sockaddr_in sender{};
#ifdef _WIN32
        int sender_size = sizeof(sender);
#else
        socklen_t sender_size = sizeof(sender);
#endif
        int received = static_cast<int>(::recvfrom(socket_.handle, reinterpret_cast<char*>(packet),
            sizeof(packet), 0, reinterpret_cast<sockaddr*>(&sender), &sender_size));
        if (received < 0 || static_cast<size_t>(received) > read_buffer_.size()) {
            read_completer_.invoke_and_clear({fibre::kStreamError, read_buffer_.begin()});
            return;
        }
        if (sender.sin_addr.s_addr != peer_.sin_addr.s_addr ||
            sender.sin_port != peer_.sin_port) return;
        std::memcpy(read_buffer_.begin(), packet, static_cast<size_t>(received));
        read_completer_.invoke_and_clear({fibre::kStreamOk, read_buffer_.begin() + received});
    }

private:
    Socket socket_;
    sockaddr_in peer_{};
    fibre::bufptr_t read_buffer_;
    fibre::Callback<void, fibre::ReadResult> read_completer_;
};

} // namespace example
