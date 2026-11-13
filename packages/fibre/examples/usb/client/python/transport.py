"""PyUSB 提供整包通道；USB 资源归上位机应用管理。"""
import asyncio
from concurrent.futures import ThreadPoolExecutor
import os
import time
import usb.backend.libusb1
import usb.core
import usb.util


class UsbTransport:
    mtu = 63

    def __init__(self, vid=0xCAFE, pid=0xF405):
        library = os.environ.get("FIBRE_USB_LIBRARY_PATH")
        backend = usb.backend.libusb1.get_backend(find_library=lambda _: library) if library else None
        if library and backend is None:
            raise RuntimeError("无法加载 FIBRE_USB_LIBRARY_PATH 指定的 libusb")
        self._device = usb.core.find(idVendor=vid, idProduct=pid, backend=backend)
        if self._device is None:
            raise RuntimeError(f"未找到 USB 示例设备 {vid:04x}:{pid:04x}")
        self._detached = False
        try:
            try:
                if self._device.is_kernel_driver_active(0):
                    self._device.detach_kernel_driver(0)
                    self._detached = True
            except NotImplementedError:
                pass  # Windows 的 libusb 后端不提供内核驱动分离接口。
            self._device.set_configuration(1)
            interface = self._device.get_active_configuration()[(0, 0)]
            endpoints = {endpoint.bEndpointAddress: endpoint for endpoint in interface}
            if (interface.bInterfaceClass != 0xFF or
                    any(address not in endpoints or
                        usb.util.endpoint_type(endpoints[address].bmAttributes) != usb.util.ENDPOINT_TYPE_BULK
                        for address in (0x01, 0x81))):
                raise RuntimeError("设备不包含示例约定的 Bulk 接口")
            usb.util.claim_interface(self._device, 0)
        except Exception:
            if self._detached:
                try:
                    self._device.attach_kernel_driver(0)
                except usb.core.USBError:
                    pass
            usb.util.dispose_resources(self._device)
            raise
        self._executor = ThreadPoolExecutor(max_workers=2, thread_name_prefix="fibre-usb-example")

    async def read_packet(self, capacity):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            try:
                packet = bytes(await asyncio.get_running_loop().run_in_executor(
                    self._executor, self._device.read, 0x81, 64, 100))
            except usb.core.USBTimeoutError:
                continue
            if not 0 < len(packet) <= min(capacity, self.mtu):
                raise ValueError("USB 响应超过示例包容量或为空")
            return packet
        raise TimeoutError("等待 USB 响应超过 5 秒")

    async def write_packet(self, packet):
        written = await asyncio.get_running_loop().run_in_executor(
            self._executor, self._device.write, 0x01, packet, 1000)
        if written != len(packet):
            raise IOError("USB 包未完整发送")

    async def close(self):
        # 绑定已取消收发协程；先等待仍在执行的有限超时 USB 操作，再释放句柄。
        await asyncio.get_running_loop().run_in_executor(None, self._executor.shutdown, True)
        try:
            usb.util.release_interface(self._device, 0)
            if self._detached:
                self._device.attach_kernel_driver(0)
        except usb.core.USBError:
            pass  # 拔出设备时仍需在 finally 中释放本地资源。
        finally:
            usb.util.dispose_resources(self._device)
