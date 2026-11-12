"""ODrive 应用的 Fibre 加载和 USB 发现入口。"""
from .platform_loader import configure_fibre_library


def Domain(path):
    from .discovery import Domain as UsbDomain
    return UsbDomain(path)
