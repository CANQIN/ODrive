#include "fibre_app.h"
#include "device.hpp"
#include "usart.h"
#include <fibre/legacy_protocol.hpp>
#include <algorithm>

// 中断只收集字节和完成事件，所有 Fibre 回调在主循环中执行。
class UartTransport final : public fibre::AsyncStreamSource, public fibre::AsyncStreamSink {
public:
    void init() {
        if (HAL_UART_Receive_IT(&huart4, &rx_byte_, 1) != HAL_OK) Error_Handler();
    }

    void start_read(fibre::bufptr_t buffer, fibre::TransferHandle* handle,
                    fibre::Callback<void, fibre::ReadResult> done) final {
        if (handle) *handle = reinterpret_cast<fibre::TransferHandle>(this);
        read_buffer_ = buffer;
        read_done_ = done;
    }
    void cancel_read(fibre::TransferHandle) final {
        // HAL 接收的是适配器自有字节，不再持有被取消的 Fibre 缓冲区。
        read_done_.invoke_and_clear({fibre::kStreamCancelled, read_buffer_.begin()});
    }
    void start_write(fibre::cbufptr_t buffer, fibre::TransferHandle* handle,
                     fibre::Callback<void, fibre::WriteResult> done) final {
        if (handle) *handle = reinterpret_cast<fibre::TransferHandle>(this);
        write_buffer_ = buffer;
        write_done_ = done;
        tx_finished_ = false;
        if (HAL_UART_Transmit_IT(&huart4, const_cast<uint8_t*>(buffer.begin()), buffer.size()) != HAL_OK) {
            write_done_.invoke_and_clear({fibre::kStreamError, buffer.begin()});
        }
    }
    void cancel_write(fibre::TransferHandle) final {
        HAL_UART_AbortTransmit(&huart4);
        tx_finished_ = false;
        write_done_.invoke_and_clear({fibre::kStreamCancelled, write_buffer_.begin()});
    }

    void received() {
        uint16_t next = (head_ + 1) % sizeof(rx_ring_);
        if (next == tail_) {
            failed_ = true;
        } else {
            rx_ring_[head_] = rx_byte_;
            __DMB();
            head_ = next;
        }
        if (HAL_UART_Receive_IT(&huart4, &rx_byte_, 1) != HAL_OK) failed_ = true;
    }
    void transmitted() { tx_finished_ = true; }
    void error() { failed_ = true; }

    void poll() {
        if (failed_) Error_Handler();
        if (read_done_) {
            size_t count = 0;
            uint32_t mask = __get_PRIMASK();
            __disable_irq();
            while (tail_ != head_ && count < read_buffer_.size()) {
                read_buffer_.begin()[count++] = rx_ring_[tail_];
                tail_ = (tail_ + 1) % sizeof(rx_ring_);
            }
            __set_PRIMASK(mask);
            if (count) read_done_.invoke_and_clear({fibre::kStreamOk, read_buffer_.begin() + count});
        }
        if (tx_finished_ && write_done_) {
            tx_finished_ = false;
            write_done_.invoke_and_clear({fibre::kStreamOk, write_buffer_.end()});
        }
    }

private:
    uint8_t rx_ring_[512]{};
    uint8_t rx_byte_ = 0;
    volatile uint16_t head_ = 0, tail_ = 0;
    volatile bool tx_finished_ = false, failed_ = false;
    fibre::bufptr_t read_buffer_;
    fibre::cbufptr_t write_buffer_;
    fibre::Callback<void, fibre::ReadResult> read_done_;
    fibre::Callback<void, fibre::WriteResult> write_done_;
};

static UartTransport transport;
static fibre::LegacyProtocolStreamBased protocol(&transport, &transport, &example::device_endpoints());
static auto on_stopped = [](fibre::LegacyProtocolPacketBased*, fibre::StreamStatus) { Error_Handler(); };

extern "C" void fibre_app_init(void) {
    transport.init();
    protocol.start(on_stopped);
}
extern "C" void fibre_app_poll(void) { transport.poll(); }
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef* uart) {
    if (uart == &huart4) transport.received();
}
extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef* uart) {
    if (uart == &huart4) transport.transmitted();
}
extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* uart) {
    if (uart == &huart4) transport.error();
}
