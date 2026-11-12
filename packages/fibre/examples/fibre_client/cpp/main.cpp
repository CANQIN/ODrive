#include "udp_transport.hpp"
#include "tcp_transport.hpp"
#include <fibre/legacy_protocol.hpp>
#include <fibre/protocol.hpp>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

template<typename Transport, typename Protocol>
int run_client(Transport& transport, Protocol& client, const char* transport_name) {
    std::shared_ptr<fibre::LegacyObject> remote;
    bool stopped = false;
    auto found = [&](fibre::LegacyObjectClient*, std::shared_ptr<fibre::LegacyObject> object) {
        remote = std::move(object);
    };
    auto on_stopped = [&](fibre::LegacyProtocolPacketBased*, fibre::StreamStatus) {
        stopped = true;
    };
    auto wait_until = [&](auto ready) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!ready() && !stopped && std::chrono::steady_clock::now() < deadline) {
            transport.poll(100);
        }
        if (ready() && !stopped) return true;
        // 先取消收发，完成仍引用调用缓冲区的回调，再离开当前作用域。
        transport.cancel_read(0);
        return false;
    };

    client.start(found, {}, on_stopped);
    if (!wait_until([&] { return static_cast<bool>(remote); })) {
        std::fputs("失败：5 秒内未发现设备，请先启动对应传输的 fibre_server。\n", stderr);
        return 1;
    }

    // 只知道业务名称和参数类型；端点编号由服务端 JSON 发现，不编译服务端 YAML。
    auto call = [&](fibre::LegacyObject* object, const char* name,
                    std::optional<float> input, auto& output) {
        using Output = std::decay_t<decltype(output)>;
        auto& function = object->intf->functions.at(name);
        std::vector<uint8_t> tx(sizeof(object) + (input ? sizeof(float) : 0));
        // 对象指针仅用于本地客户端 API，Fibre 会把它转换成线协议端点。
        std::memcpy(tx.data(), &object, sizeof(object));
        if (input) {
            auto buffer = fibre::bufptr_t{tx.data() + sizeof(object), sizeof(float)};
            fibre::Codec<float>::encode(*input, &buffer);
        }
        uint8_t rx[sizeof(Output)]{};
        bool completed = false;
        bool success = false;
        void* handle = nullptr;
        auto release = [&](fibre::CallBufferRelease result) -> std::optional<fibre::CallBuffers> {
            completed = true;
            success = result.status == fibre::kFibreClosed && result.rx_end == rx + sizeof(rx);
            return fibre::CallBuffers{fibre::kFibreClosed, {}, {}};
        };
        auto immediate = function.call(&handle,
            {fibre::kFibreClosed, {tx.data(), tx.size()}, rx}, release);
        if (immediate) release(*immediate);
        if (!wait_until([&] { return completed; }) || !success) return false;
        auto buffer = fibre::cbufptr_t{rx};
        auto decoded = fibre::Codec<Output>::decode(&buffer);
        if (!decoded) return false;
        output = *decoded;
        return true;
    };

    auto value = remote->intf->attributes.at("value").object;
    auto count = remote->intf->attributes.at("setter_count").object;
    float initial_value, old_value, new_value, result;
    uint32_t initial_count, final_count;
    bool success = call(value.get(), "read", std::nullopt, initial_value) &&
        call(count.get(), "read", std::nullopt, initial_count) &&
        call(value.get(), "exchange", 3.5f, old_value) &&
        call(value.get(), "read", std::nullopt, new_value) &&
        call(remote.get(), "add", 2.0f, result) &&
        call(count.get(), "read", std::nullopt, final_count);
    transport.cancel_read(0);
    if (!success || old_value != initial_value || new_value != 3.5f ||
        result != 5.5f || final_count != initial_count + 1) {
        std::fputs("失败：远程调用未完成或返回值不匹配。\n", stderr);
        return 1;
    }
    std::printf("已通过 %s 发现设备：127.0.0.1:39000\n", transport_name);
    std::printf("属性写入：旧值=%.2f，新值=%.2f，setter 次数=%u -> %u\n",
                old_value, new_value, static_cast<unsigned>(initial_count),
                static_cast<unsigned>(final_count));
    std::printf("函数调用：add(2.0)=%.2f\n", result);
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "udp") && std::strcmp(argv[1], "tcp"))) {
        std::fputs("用法：fibre_client udp|tcp\n", stderr);
        return 1;
    }
    try {
        if (std::strcmp(argv[1], "udp") == 0) {
            example::UdpTransport transport;
            transport.open(39000);
            fibre::LegacyProtocolPacketBased client(&transport, &transport, 127);
            return run_client(transport, client, "UDP");
        } else {
            example::TcpTransport transport;
            transport.connect(39000);
            fibre::LegacyProtocolStreamBased client(&transport, &transport);
            return run_client(transport, client, "TCP");
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "失败：%s\n", error.what());
        return 1;
    }
}
