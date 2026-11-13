"""上位机业务只依赖 Fibre 对象接口，不依赖具体传输。"""

import fibre
from transport import UdpTransport


def main():
    transport = UdpTransport()
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
        print(f"已通过 UDP 发现设备：127.0.0.1:39000")
        print(f"属性写入：旧值={old_value:.2f}，新值={new_value:.2f}，"
              f"setter 次数={initial_count} -> {final_count}")
        print(f"函数调用：add(2.0)={result:.2f}")


if __name__ == "__main__":
    main()
