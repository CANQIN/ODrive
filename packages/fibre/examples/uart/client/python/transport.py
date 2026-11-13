"""电脑串口收发，使用与 MCU 相同的 Fibre 字节流帧格式。"""
import asyncio
from collections import deque
from concurrent.futures import ThreadPoolExecutor
import time
import serial
from fibre.protocol import (
    CRC8_INIT, CRC16_INIT, SYNC_BYTE, StreamToPacketSegmenter,
    calc_crc8, calc_crc16,
)


class SerialTransport:
    mtu = 127

    def __init__(self, port, baudrate=115200):
        self._serial = serial.Serial(port, baudrate, timeout=0.05, write_timeout=5)
        self._serial.reset_input_buffer()
        self._executor = ThreadPoolExecutor(max_workers=2, thread_name_prefix="fibre-uart-example")
        self._packets = deque()
        self._segmenter = StreamToPacketSegmenter(self)

    def process_packet(self, packet):
        self._packets.append(bytes(packet))

    async def read_packet(self, capacity):
        deadline = time.monotonic() + 5
        while not self._packets:
            data = await asyncio.get_running_loop().run_in_executor(self._executor, self._serial.read, 256)
            self._segmenter.process_bytes(data)
            if time.monotonic() >= deadline:
                raise TimeoutError("等待串口响应超过 5 秒")
        packet = self._packets.popleft()
        if len(packet) > capacity:
            raise ValueError("串口解帧后的包超过接收容量")
        return packet

    async def write_packet(self, packet):
        header = bytes((SYNC_BYTE, len(packet)))
        frame = (header + bytes((calc_crc8(CRC8_INIT, header),)) + packet
                 + calc_crc16(CRC16_INIT, packet).to_bytes(2, "big"))
        written = await asyncio.get_running_loop().run_in_executor(self._executor, self._serial.write, frame)
        if written != len(frame):
            raise IOError("串口帧未完整发送")

    async def close(self):
        await asyncio.get_running_loop().run_in_executor(None, self._executor.shutdown, True)
        self._serial.close()
