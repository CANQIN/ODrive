FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends gcc-arm-none-eabi libnewlib-arm-none-eabi \
      libstdc++-arm-none-eabi-newlib cmake ninja-build build-essential git \
      python3 python3-yaml python3-jinja2 python3-jsonschema && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /ODrive

# 将完整仓库挂载到 /ODrive；这里只构建，不连接或烧录设备。
CMD ["sh", "-c", "cmake -S . -B build -G Ninja -DODRIVE_BUILD_HOST=OFF -DPython3_EXECUTABLE=/usr/bin/python3 && cmake --build build --target firmware --parallel"]
