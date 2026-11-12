"""示例应用提供 UDP 包通道和 TCP 流到包适配，核心绑定不操作 socket。"""

import asyncio
from collections import deque
import socket

from fibre.protocol import (
    CRC8_INIT, CRC16_INIT, SYNC_BYTE, StreamToPacketSegmenter,
    calc_crc8, calc_crc16,
)


class UdpTransport:
    mtu = 127

    def __init__(self):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._socket.connect(("127.0.0.1", 39000))
        self._socket.setblocking(False)

    async def read_packet(self, capacity):
        # 多收一个字节，超长包会被绑定边界校验拒绝。
        return await asyncio.wait_for(
            asyncio.get_running_loop().sock_recv(self._socket, capacity + 1), 5)

    async def write_packet(self, packet):
        await asyncio.wait_for(
            asyncio.get_running_loop().sock_sendall(self._socket, packet), 5)

    async def close(self):
        self._socket.close()


class TcpTransport:
    mtu = 127

    def __init__(self):
        # 连接建立在进入 Domain 前完成；后续收发在 Fibre 的 asyncio 线程执行。
        self._socket = socket.create_connection(("127.0.0.1", 39000), timeout=5)
        self._socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self._socket.setblocking(False)
        self._packets = deque()
        self._segmenter = StreamToPacketSegmenter(self)

    def process_packet(self, packet):
        # 复用包内解析器，处理 TCP 拆包、粘包、CRC 和帧同步。
        self._packets.append(bytes(packet))

    async def read_packet(self, capacity):
        while not self._packets:
            data = await asyncio.wait_for(
                asyncio.get_running_loop().sock_recv(self._socket, 512), 5)
            if not data:
                raise ConnectionError("TCP 连接已关闭")
            self._segmenter.process_bytes(data)
        packet = self._packets.popleft()
        if len(packet) > capacity:
            raise ValueError("TCP 解帧后的包超过接收容量")
        return packet

    async def write_packet(self, packet):
        header = bytes((SYNC_BYTE, len(packet)))
        frame = (header + bytes((calc_crc8(CRC8_INIT, header),)) + packet
                 + calc_crc16(CRC16_INIT, packet).to_bytes(2, "big"))
        await asyncio.wait_for(
            asyncio.get_running_loop().sock_sendall(self._socket, frame), 5)

    async def close(self):
        self._socket.close()
