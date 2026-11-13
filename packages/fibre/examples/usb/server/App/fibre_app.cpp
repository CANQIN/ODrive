#include "fibre_app.h"
#include "device.hpp"
#include "usb_bulk.h"
#include <fibre/legacy_protocol.hpp>
#include <optional>

// USB 中断只更新 usb_bulk 的自有缓冲区和状态，协议回调全部在 poll 中执行。
class UsbTransport final : public fibre::AsyncStreamSource, public fibre::AsyncStreamSink {
public:
    void start_read(fibre::bufptr_t buffer, fibre::TransferHandle* handle,
                    fibre::Callback<void, fibre::ReadResult> done) final {
        if (handle) *handle = reinterpret_cast<fibre::TransferHandle>(this);
        read_buffer_ = buffer;
        read_done_ = done;
    }
    void cancel_read(fibre::TransferHandle) final {
        read_done_.invoke_and_clear({fibre::kStreamCancelled, read_buffer_.begin()});
    }
    void start_write(fibre::cbufptr_t buffer, fibre::TransferHandle* handle,
                     fibre::Callback<void, fibre::WriteResult> done) final {
        if (handle) *handle = reinterpret_cast<fibre::TransferHandle>(this);
        write_buffer_ = buffer;
        write_done_ = done;
        if (usb_bulk_write(buffer.begin(), buffer.size()) != 0) {
            write_done_.invoke_and_clear({fibre::kStreamError, buffer.begin()});
        }
    }
    void cancel_write(fibre::TransferHandle) final {
        // USB 使用驱动自有 TX 副本，被取消的 Fibre 缓冲区可立即释放。
        write_done_.invoke_and_clear({fibre::kStreamCancelled, write_buffer_.begin()});
    }
    void poll() {
        if (read_done_) {
            int count = usb_bulk_read(read_buffer_.begin(), read_buffer_.size());
            if (count != 0) {
                read_done_.invoke_and_clear({count > 0 ? fibre::kStreamOk : fibre::kStreamError,
                                            read_buffer_.begin() + (count > 0 ? count : 0)});
            }
        }
        if (write_done_ && usb_bulk_tx_completed()) {
            write_done_.invoke_and_clear({fibre::kStreamOk, write_buffer_.end()});
        }
    }
private:
    fibre::bufptr_t read_buffer_;
    fibre::cbufptr_t write_buffer_;
    fibre::Callback<void, fibre::ReadResult> read_done_;
    fibre::Callback<void, fibre::WriteResult> write_done_;
};

static UsbTransport transport;
static std::optional<fibre::LegacyProtocolPacketBased> protocol;
static uint32_t previous_generation = 0;
static bool stopped = false;
static auto on_stopped = [](fibre::LegacyProtocolPacketBased*, fibre::StreamStatus) { stopped = true; };

extern "C" void fibre_app_init(void) { usb_bulk_init(); }
extern "C" void fibre_app_poll(void) {
    uint32_t generation = usb_bulk_generation();
    if (protocol && (!usb_bulk_ready() || generation != previous_generation || stopped)) {
        transport.cancel_read(0);
        transport.cancel_write(0);
        protocol.reset();
    }
    if (!protocol && usb_bulk_ready()) {
        previous_generation = generation;
        stopped = false;
        protocol.emplace(&transport, &transport, FIBRE_USB_MTU, &example::device_endpoints());
        protocol->start(on_stopped);
    }
    if (protocol) transport.poll();
}
