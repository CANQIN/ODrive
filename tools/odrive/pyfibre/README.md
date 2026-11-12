# ODrive 应用平台适配

平台无关的 Fibre Python 客户端位于 [`packages/fibre/bindings/python`](../../../packages/fibre/bindings/python/README.md)。这里属于 ODrive 应用，负责原生库加载、PyUSB 收发、设备热插拔和交互终端。

## 新原生库加载

`FIBRE_LIBRARY_PATH` 显式指定的库优先。未设置时，加载器依次查找项目 `build/host/install/bin`、`build/host/install/lib` 和本包 `runtime/` 中的 `libfibre.dll`、`fibre.dll`、`libfibre.so`、`libfibre.dylib`。`ODRIVE_BUILD_DIR` 可指定替代的项目构建根目录。

这里不选择或回退到旧的 `libfibre-<平台名>` 库。Windows 的运行时 DLL 搜索目录在应用加载器中设置。`runtime/` 可随发行包携带新 C ABI 库及其工具链运行时依赖。

## USB 发现与收发

```python
import odrive

with odrive.pyfibre.Domain(odrive.default_search_path) as domain:
    discovery = domain.run_discovery(on_device)
```

`on_device` 是异步回调，收到与原先相同的 SDK 远程对象。应用每 0.5 秒枚举 USB 热插拔，每个设备独立创建一个 SDK Domain。`find_any(path=..., serial_number=...)` 与 odrivetool 仍使用原有入口。

USB 过滤支持 `idVendor`、`idProduct`、`bInterfaceClass`、`bInterfaceSubClass`、`bInterfaceProtocol`、`bus`、`address`，多个 USB 过滤器用分号分隔。例：`usb:idVendor=0x1209,idProduct=0x0D32,bInterfaceClass=0,bInterfaceSubClass=1,bInterfaceProtocol=0`。

PyUSB 通过设备当前配置查找成对 Bulk 端点，认领匹配接口。系统未提供可发现的 USB 运行库时，可用 `ODRIVE_USB_LIBRARY_PATH` 指定 libusb 动态库文件；该选择属于应用适配。适配不会自动切换配置、解绑内核驱动或重启设备。阻塞 USB 读写在线程池执行；关闭时等待在途收发结束，再释放接口和句柄。

`shell.py` 提供 odrivetool 的交互终端。SDK 本身不包含这些平台实现。
