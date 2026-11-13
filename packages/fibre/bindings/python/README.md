# Fibre Python 客户端

发行包名和导入名均为 `fibre`。Python 包只包含绑定和通用对象访问，不包含原生库、USB/串口驱动或终端平台适配。

在本目录运行 `python -m pip install .`。导入前，通过环境变量 `FIBRE_LIBRARY_PATH` 明确指定应用构建的原生 Fibre 动态库完整文件名。

源码安装或打包生成的 `*.egg-info`、`build/` 和 `dist/` 属于本地生成物，由包根目录的 `.gitignore` 忽略，不纳入独立 SDK 源码归档。

## 注入应用传输

```python
import fibre

with fibre.Domain(transport=my_packets) as domain:
    device = domain.discover_one()
    device.value = 3.5
    print(device.value)
    print(device.add(2.0))
```

`discover_one()` 成功后保留发现登记，返回对象在所属 `Domain` 的有效期内可用；退出 `Domain` 时统一停止发现并释放对象。

`my_packets` 由应用实现：

| 成员 | 契约 |
| --- | --- |
| `mtu` | 单包最大长度，范围 12～128 字节；接口发现请求需要 8 字节协议字段及 4 字节偏移量 |
| `async read_packet(capacity)` | 返回一个完整的非空 `bytes` 包，不超过 `capacity`；断开时抛出异常 |
| `async write_packet(packet)` | 发送完整包后返回；失败时抛出异常 |
| `async close()` | 关闭应用资源，取消或完成外部待处理收发 |

这些协程在 Fibre 自己的 asyncio 线程中运行。应用可在协程中接入自己的异步驱动或线程安全队列，不应阻塞事件循环。字节流传输由应用适配成包通道。

`Domain` 退出时先停止所属发现并取消、等待后台发现回调，再取消并等待包收发任务，关闭原生通道、调用应用的 `close()`，最后关闭 Domain。退出后再次调用 `Discovery.stop()` 不会访问已释放的句柄。包收发异常会中止发现或远程调用。

[示例工程](../../examples/README.md) 按 UDP、TCP、USB、UART 分组，每组的 `client/python/` 提供对应传输及业务调用。USB 和串口示例分别通过 PyUSB、pyserial 连接 MCU。Python 绑定仅提供通用调度与定时器，不注册操作系统文件描述符。`Domain(path)` 仍可用于应用自行扩展了后端的原生库；默认原生核心没有内置后端。
