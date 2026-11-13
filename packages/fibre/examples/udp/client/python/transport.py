"""UDP 数据报直接作为 Fibre 逻辑包。"""
import asyncio
import socket


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
