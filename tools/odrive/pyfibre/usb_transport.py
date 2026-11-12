"""ODrive 应用的 PyUSB 包通道；不属于平台无关 SDK。"""
import asyncio
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
import os
import threading

import usb.backend.libusb1
import usb.core
import usb.util


@dataclass
class UsbInterface:
    device: object
    configuration: int
    interface: int
    rx_endpoint: int
    tx_endpoint: int
    mtu: int

    @property
    def key(self):
        return self.device.bus, self.device.address, self.interface


def parse_usb_path(path):
    """保留原 Fibre USB 过滤字段；一个路径可用分号列出多个 USB 过滤器。"""
    filters = []
    allowed = {'idVendor', 'idProduct', 'bInterfaceClass', 'bInterfaceSubClass',
               'bInterfaceProtocol', 'bus', 'address'}
    for part in path.split(';'):
        provider, _, specification = part.partition(':')
        if provider != 'usb':
            raise ValueError('ODrive 应用当前只提供 USB 传输：' + provider)
        values = {}
        if specification:
            for item in specification.split(','):
                name, separator, value = item.partition('=')
                if not separator or name not in allowed:
                    raise ValueError('无效 USB 过滤条件：' + item)
                values[name] = int(value, 0)
        filters.append(values)
    return filters


def enumerate_interfaces(filters):
    interfaces = {}
    backend = None
    library_path = os.environ.get('ODRIVE_USB_LIBRARY_PATH')
    if library_path:
        library_path = os.path.abspath(os.path.expanduser(library_path))
        backend = usb.backend.libusb1.get_backend(find_library=lambda _: library_path)
        if backend is None:
            raise usb.core.NoBackendError('无法加载 ODRIVE_USB_LIBRARY_PATH 指定的 USB 运行库。')
    for specification in filters:
        device_filter = {name: value for name, value in specification.items()
                         if name in ('idVendor', 'idProduct')}
        for device in usb.core.find(find_all=True, backend=backend, **device_filter):
            if any(getattr(device, name) != specification[name]
                   for name in ('bus', 'address') if name in specification):
                continue
            try:
                for configuration in device:
                    for interface in configuration:
                        if interface.bAlternateSetting != 0:
                            continue
                        if any(getattr(interface, name) != specification[name]
                               for name in ('bInterfaceClass', 'bInterfaceSubClass', 'bInterfaceProtocol')
                               if name in specification):
                            continue
                        endpoints = [endpoint for endpoint in interface
                                     if usb.util.endpoint_type(endpoint.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK]
                        incoming = next((endpoint for endpoint in endpoints
                                         if usb.util.endpoint_direction(endpoint.bEndpointAddress) == usb.util.ENDPOINT_IN), None)
                        outgoing = next((endpoint for endpoint in endpoints
                                         if usb.util.endpoint_direction(endpoint.bEndpointAddress) == usb.util.ENDPOINT_OUT), None)
                        if incoming is None or outgoing is None:
                            continue
                        candidate = UsbInterface(device, configuration.bConfigurationValue,
                            interface.bInterfaceNumber, incoming.bEndpointAddress,
                            outgoing.bEndpointAddress,
                            min(incoming.wMaxPacketSize, outgoing.wMaxPacketSize, 128) - 1)
                        interfaces[candidate.key] = candidate
            except usb.core.USBError:
                # 枚举期间拔出的设备由下一轮热插拔扫描处理。
                continue
    return interfaces


class UsbPackets:
    def __init__(self, interface):
        self._interface = interface
        self.mtu = interface.mtu
        self.closed = threading.Event()
        self._closing = threading.Event()
        self._close_task = None
        self._pending = set()
        self._pending_lock = threading.Lock()
        try:
            configuration = interface.device.get_active_configuration()
            if configuration.bConfigurationValue != interface.configuration:
                raise ValueError('USB 接口不属于设备当前配置。')
            usb.util.claim_interface(interface.device, interface.interface)
        except Exception:
            usb.util.dispose_resources(interface.device)
            raise
        self._executor = ThreadPoolExecutor(max_workers=2, thread_name_prefix='odrive-usb')

    def _finished(self, future):
        with self._pending_lock:
            self._pending.discard(future)

    async def _io(self, function, *args):
        if self._closing.is_set():
            raise EOFError('USB 通道已关闭。')
        future = self._executor.submit(function, *args)
        with self._pending_lock:
            self._pending.add(future)
        future.add_done_callback(self._finished)
        return await asyncio.wrap_future(future)

    async def read_packet(self, capacity):
        while not self._closing.is_set():
            try:
                packet = await self._io(self._interface.device.read,
                    self._interface.rx_endpoint, capacity, 100)
                if packet:
                    return bytes(packet)
            except usb.core.USBTimeoutError:
                continue
        raise EOFError('USB 通道已关闭。')

    async def write_packet(self, packet):
        written = await self._io(self._interface.device.write,
            self._interface.tx_endpoint, packet, 1000)
        if written != len(packet):
            raise IOError('USB 包未完整发送。')

    def _release(self):
        try:
            usb.util.release_interface(self._interface.device, self._interface.interface)
        except usb.core.USBError:
            # 设备拔出后释放接口仍可报 USBError，资源句柄由 dispose_resources 关闭。
            pass
        finally:
            usb.util.dispose_resources(self._interface.device)

    async def close(self):
        if self._close_task is None:
            self._closing.set()
            async def cleanup():
                try:
                    with self._pending_lock:
                        pending = tuple(self._pending)
                    await asyncio.gather(*(asyncio.wrap_future(future) for future in pending),
                                         return_exceptions=True)
                    await asyncio.wrap_future(self._executor.submit(self._release))
                finally:
                    self._executor.shutdown(wait=False)
                    self.closed.set()
            self._close_task = asyncio.create_task(cleanup())
        try:
            await asyncio.shield(self._close_task)
        except asyncio.CancelledError:
            await asyncio.shield(self._close_task)
            raise
