## Important Note

The firmware in this repository is compatible only with the ODrive v3.6-56V (NRND) and is no longer under active development.

Firmware for the new generation of ODrives ([ODrive Pro](https://odriverobotics.com/shop/odrive-pro), [S1](https://odriverobotics.com/shop/odrive-s1), [Micro](https://odriverobotics.com/shop/odrive-micro), etc.) is currently being actively maintained and developed, however its source code is currently not publicly available. Access may be available under NDA, please [reach out to us](mailto:info@odriverobotics.com) for inquiries.

## Overview

### 本地统一构建

工程入口为根目录 CMake，固件与上位机使用不同工具链和构建目录：

```sh
cmake -S . -B build -G Ninja -DARM_TOOLCHAIN_DIR=/path/to/arm-gcc -DPython3_EXECUTABLE=/path/to/python
cmake --build build --parallel
```

默认构建 `firmware` 和 `host`：固件输出在 `build/firmware/`，新 Fibre 客户端安装在 `build/host/install/`。`tools` 通过包外 PyUSB 适配使用该客户端，GUI 继续调用 tools 的应用接口。固件与 tools 共用 [独立 Fibre 包](packages/fibre/README.md)。

当前实现的板型为 `v3.6-56V`（STM32F405）。`ODRIVE_BOARD_VERSION` 从 `Firmware/Boards` 自动发现完整板型配置，未知板型会在配置阶段报错。按需启用 `ODRIVE_BUILD_TOOLS` 生成 Python 分发包，启用 `ODRIVE_BUILD_GUI` 提供 `gui_web`、`gui_package`；GUI 依赖可通过 `gui_dependencies` 安装。打包文件写入 `build/dist/`，原生库对应本次 PC 工具链。

具体配置见 [开发指南](docs/developer-guide.rst)。

CubeMX 基础已更新到 **6.18.1 / STM32CubeF4 1.28.3**。每个板型的配置、生成代码和硬件映射集中在 `Firmware/Boards/<板型>`，共享平台与 USB 协议位于 `Firmware/Platform`。修改 `.ioc` 后执行 `./scripts/regenerate-cubemx.ps1 -Board v3.6-56V`，再使用根目录 CMake 构建与测试；新增板型流程见 [板型接入说明](Firmware/Boards/README.md)。

![ODrive Logo](https://static1.squarespace.com/static/58aff26de4fcb53b5efd2f02/t/59bf2a7959cc6872bd68be7e/1505700483663/Odrive+logo+plus+text+black.png?format=1000w)

This project is all about accurately driving brushless motors, for cheap. The aim is to make it possible to use inexpensive brushless motors in high performance robotics projects, like [this](https://www.youtube.com/watch?v=WT4E5nb3KtY).

| Branch | Build Status |
|--------|--------------|
| master | [![Build Status](https://travis-ci.org/madcowswe/ODrive.png?branch=master)](https://travis-ci.org/madcowswe/ODrive) |
| devel  | [![Build Status](https://travis-ci.org/madcowswe/ODrive.png?branch=devel)](https://travis-ci.org/madcowswe/ODrive) |

[![pip install odrive (nightly)](https://github.com/madcowswe/ODrive/workflows/pip%20install%20odrive%20(nightly)/badge.svg)](https://github.com/madcowswe/ODrive/actions?query=workflow%3A%22pip+install+odrive+%28nightly%29%22)

Please refer to the [Developer Guide](https://docs.odriverobotics.com/v/latest/developer-guide.html#) to get started with ODrive firmware development.


### Repository Structure
 * **Firmware**: ODrive firmware
 * **tools**: Python library & tools
 * **docs**: Documentation

### Other Resources

 * [Main Website](https://www.odriverobotics.com/)
 * [User Guide](https://docs.odriverobotics.com/)
 * [Forum](https://discourse.odriverobotics.com/)
 * [Chat](https://discourse.odriverobotics.com/t/come-chat-with-us/281)
