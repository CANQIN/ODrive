"""上位机业务只依赖 Fibre 对象接口，不依赖具体传输。"""

import argparse

import fibre
from transport import SerialTransport


def main():
    parser = argparse.ArgumentParser(description="Fibre UART 客户端")
    parser.add_argument("--port", required=True, help="串口，例如 COM5 或 /dev/ttyUSB0")
    parser.add_argument("--baudrate", type=int, default=115200)
    args = parser.parse_args()
    if not args.port:
        parser.error("请指定 --port；CMake 运行目标通过 -DSERIAL_PORT=COM5 配置")
    transport = SerialTransport(args.port, args.baudrate)
    with fibre.Domain(transport=transport) as domain:
        device = domain.discover_one()
        old_value = device.value
        initial_count = device.setter_count
        device.value = 3.5
        new_value = device.value
        result = device.add(2.0)
        final_count = device.setter_count
        if new_value != 3.5 or result != 5.5 or final_count != initial_count + 1:
            raise RuntimeError("属性、函数结果或 setter 次数不匹配")
        print(f"已通过 UART 发现设备：{args.port}")
        print(f"属性写入：旧值={old_value:.2f}，新值={new_value:.2f}，"
              f"setter 次数={initial_count} -> {final_count}")
        print(f"函数调用：add(2.0)={result:.2f}")


if __name__ == "__main__":
    main()
