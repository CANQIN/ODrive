#include "tcp_transport.hpp"
#include "device.hpp"
#include <fibre/legacy_protocol.hpp>
#include <csignal>
#include <cstdio>

// 无 MCU 时的桌面模拟入口；移植时由固件主循环或通信任务替换。
volatile std::sig_atomic_t interrupted = 0;

template<typename Transport, typename Protocol>
void serve(Transport& transport, Protocol& protocol) {
    bool stopped = false;
    auto on_stopped = [&](fibre::LegacyProtocolPacketBased*, fibre::StreamStatus) {
        stopped = true;
    };
    protocol.start(on_stopped);
    while (!interrupted && !stopped) transport.poll(100);
    transport.cancel_read(0);
}

int main() {
    std::signal(SIGINT, [](int) { interrupted = 1; });
    std::signal(SIGTERM, [](int) { interrupted = 1; });
    try {
        example::TcpListener listener(39000);
        std::puts("Fibre TCP 服务端：127.0.0.1:39000，按 Ctrl+C 退出。");
        std::fflush(stdout);
        while (!interrupted) {
            example::TcpTransport transport;
            if (!listener.accept(transport, 100)) continue;
            fibre::LegacyProtocolStreamBased server(&transport, &transport,
                                                    &example::device_endpoints());
            serve(transport, server);
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "失败：%s\n", error.what());
        return 1;
    }
}
