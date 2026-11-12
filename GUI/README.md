# ODrive GUI

主 GUI 由 Vue 前端、Electron 桌面壳和 Python Flask-SocketIO 服务组成。前端默认连接 `http://127.0.0.1:5000`，Python 服务通过 ODrive 工具访问设备。

## 统一构建

在仓库根目录配置 CMake，GUI 默认关闭：

```sh
cmake -S . -B build -G Ninja -DODRIVE_BUILD_GUI=ON
cmake --build build --target gui_web
cmake --build build --target gui_package
```

`gui_web` 构建网页资源，`gui_package` 构建当前平台的 Electron 包。构建不运行 GUI、不连接设备。需要准备 Node.js 和 npm，并先执行根目标 `gui_dependencies` 安装依赖；也可在 `GUI/` 中执行 `npm ci`。

`gui_web` 使用独立的 Web 构建入口，不加载 Electron 打包插件，因此不需要 Electron 二进制。

本机 Node.js 24.18.0 下，旧 webpack 的哈希逻辑触发 `ERR_OSSL_EVP_UNSUPPORTED`；构建前需在 PowerShell 设置 `$env:NODE_OPTIONS = '--openssl-legacy-provider'`，该兼容设置由调用环境提供。

根 `interfaces` 目标从固件 YAML 生成 Python 枚举；GUI 通过 `ODRIVE_ENUMS_PATH` 读取构建目录中的枚举。直接从 `GUI/` 使用 npm 时，该变量可覆盖输入，默认仍读取 `tools/odrive/enums.py`。

## 开发

在 `GUI/` 中执行：

```sh
npm ci
npm run enumGenerate
npm run serve
```

`npm run serve` 只启动浏览器前端，Python 服务需要单独启动。使用 Electron 开发模式时执行 `npm run electron:serve -- ../tools/`，桌面壳会在界面请求后启动 Python 服务。`npm run electron:build` 可直接构建当前平台包。

## Python 服务部署

Electron 包中包含 `server/odrive_server.py`，使用系统 Python，不内置解释器或全部 Python 依赖。部署时需要安装本轮生成的 SDK 和 ODrive 工具包，并准备：

```sh
python -m pip install flask flask_socketio flask_cors
```

从源码使用时，在仓库 `tools/` 目录执行 `python -m pip install -r requirements.txt`。桌面程序可接收本地 Python 模块目录作为参数，Python 服务会把它们加入模块搜索路径。ODrive 的 USB 运行库和驱动由应用环境提供。
