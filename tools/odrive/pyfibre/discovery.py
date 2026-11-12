"""ODrive 应用维护 USB 热插拔；每个设备独立使用一个 SDK Domain。"""
import asyncio
import threading

import fibre

from .usb_transport import UsbPackets, enumerate_interfaces, parse_usb_path


class _DeviceSession:
    def __init__(self, interface, callback):
        self.transport = UsbPackets(interface)
        self.context = None
        try:
            self.context = fibre.Domain(transport=self.transport)
            self.domain = self.context.__enter__()
            self.discovery = self.domain.run_discovery(callback)
        except BaseException:
            try:
                if self.context is None:
                    asyncio.run(self.transport.close())
                else:
                    fibre.libfibre.run_coroutine_threadsafe(
                        fibre.libfibre.libfibre.loop, self.transport.close)
            finally:
                self.close()
            raise

    def close(self):
        context, self.context = self.context, None
        if context is not None:
            context.__exit__(None, None, None)


class Discovery:
    def __init__(self, domain, callback):
        self._domain = domain
        self._callback = callback
        self._stop_event = threading.Event()
        self._thread = threading.Thread(target=self._run, daemon=True, name='odrive-discovery')
        self._sessions = {}

    def start(self):
        self._thread.start()

    def stop(self):
        self._stop_event.set()
        if threading.current_thread() not in (self._thread, fibre.libfibre.libfibre_thread):
            self._thread.join()

    def _run(self):
        candidates = self._domain._initial_interfaces
        try:
            while not self._stop_event.is_set():
                for key, session in list(self._sessions.items()):
                    if key not in candidates or session.transport.closed.is_set():
                        self._sessions.pop(key)
                        try:
                            session.close()
                        except Exception as error:
                            self._domain._logger.warn('USB 关闭失败：' + str(error))
                for key, candidate in candidates.items():
                    if key not in self._sessions:
                        try:
                            self._sessions[key] = _DeviceSession(candidate, self._callback)
                        except Exception as error:
                            self._domain._logger.warn('USB 接入失败：' + str(error))
                if self._stop_event.wait(0.5):
                    break
                candidates = enumerate_interfaces(self._domain._filters)
        except Exception as error:
            self._domain._logger.error('USB 发现停止：' + str(error))
        finally:
            for session in self._sessions.values():
                try:
                    session.close()
                except Exception as error:
                    self._domain._logger.warn('USB 关闭失败：' + str(error))
            self._sessions.clear()


class Domain:
    def __init__(self, path):
        self._filters = parse_usb_path(path)
        self._initial_interfaces = enumerate_interfaces(self._filters)
        self._discovery = None
        self._logger = fibre.Logger(verbose=False)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if self._discovery is not None:
            self._discovery.stop()

    def run_discovery(self, callback):
        if self._discovery is not None:
            raise RuntimeError('Domain 已启动发现。')
        self._discovery = Discovery(self, callback)
        self._discovery.start()
        return self._discovery

    def discover_one(self):
        found = threading.Event()
        result = []

        async def callback(device):
            if not result:
                result.append(device)
                found.set()

        self.run_discovery(callback)
        found.wait()
        return result[0]
