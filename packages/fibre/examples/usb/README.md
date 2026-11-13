# MCU USB Bulk 示例

`server` 是 STM32F405RG 固件，`client/python` 是电脑 PyUSB 客户端。接口是自定义 Bulk，接口类 `0xFF`，不是 CDC 虚拟串口或 HID。

## 硬件与 CubeMX

- 芯片：STM32F405RGT6，8 MHz HSE，主频 168 MHz，USB 时钟 48 MHz。
- USB FS：PA11 = D−，PA12 = D+；PA13/PA14 保留 SWD。
- MCU 由开发板供电，本示例关闭 VBUS sensing，USB 描述符声明自供电。
- `.ioc`：`server/fibre_usb.ioc`，USB_OTG_FS 配置为 Device Only，开启 OTG_FS 中断。

CubeMX 生成 `Core/`、`Drivers/`、启动文件、链接脚本和 CMake。`ThirdParty/STM32_USB_Device_Library` 是随示例携带的 ST USB Device Core；自定义 Bulk 类及描述符在 `App/usb_bulk.c`，HAL PCD 与 USB Core 的桥接在 `App/usbd_port.c`。

默认 VID/PID 为示例值 `0xCAFE:0xF405`，OUT 端点 `0x01`、IN 端点 `0x81`、接口 0。端点最大包长 64 字节，Fibre MTU 为 63 字节，使每次传输都是一个短包，无需把多个 USB 包重新拼成 Fibre 包，也无需尾部 ZLP。USB 的 MTU 是这个示例的适配约定，不改变 Fibre 核心协议。

## 构建 MCU

以下命令从 Fibre 包根目录执行，使用 CMake 3.22+、Ninja、Arm GNU Toolchain 和 Python。先将 `arm-none-eabi-gcc` 等工具加入 `PATH`：

```sh
python -m pip install -r codegen/requirements.txt
cmake -S examples/usb/server -B build/examples/usb-server -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake"
cmake --build build/examples/usb-server
```

输出 `build/examples/usb-server/fibre_usb.elf`，可通过 SWD 烧录。打开 `.ioc` 后可在 CubeMX 重新生成；应用代码位于 `App/`，ST USB Device Core 位于应用管理的 `ThirdParty/`，主函数只在 `USER CODE` 区域调用 `fibre_app_init()` 和 `fibre_app_poll()`。

USB 配置变化、复位或重新连接时，适配层在主循环重建 Fibre 协议实例，保留业务对象。USB 中断只更新驱动的自有缓冲区和状态，不直接执行 Fibre 业务。

## 构建并运行电脑客户端

```sh
python -m pip install -r examples/usb/client/python/requirements.txt
cmake -S examples/usb/client/python -B build/examples/usb-python -G Ninja
cmake --build build/examples/usb-python
cmake --build build/examples/usb-python --target run
```

PyUSB 需要 libusb 后端。Windows 需要为此自定义接口配置 WinUSB 驱动；系统无法找到 libusb 时，可将 `FIBRE_USB_LIBRARY_PATH` 设置为 `libusb-1.0.dll` 的完整路径。Linux 需要 libusb 和设备访问权限。

客户端根据 VID/PID 查找设备，检查接口及 Bulk 端点，然后读取接口 JSON、操作属性并调用函数。应用退出时先等待后台 USB 操作结束，再释放接口与设备句柄。

验证范围：已完成固件交叉编译和电脑端构建；USB 枚举、实际收发及拔插恢复需要连接实物验证。
