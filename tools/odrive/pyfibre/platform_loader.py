"""ODrive 应用选择新 SDK 的原生 C ABI 库及运行时搜索目录。"""
import os
from pathlib import Path


_native_directory_handle = None


def configure_fibre_library():
    global _native_directory_handle

    if not os.environ.get('FIBRE_LIBRARY_PATH'):
        application_dir = Path(__file__).resolve().parent
        repository_dir = application_dir.parents[2]
        build_dir = Path(os.environ.get('ODRIVE_BUILD_DIR', repository_dir / 'build'))
        directories = [build_dir / 'host' / 'install' / 'bin',
                       build_dir / 'host' / 'install' / 'lib',
                       application_dir / 'runtime']
        candidates = (directory / name for directory in directories
                      for name in ('libfibre.dll', 'fibre.dll', 'libfibre.so', 'libfibre.dylib'))
        library = next((path for path in candidates if path.is_file()), None)
        if library is None:
            raise ModuleNotFoundError(
                '找不到新 Fibre C ABI 库。请构建 host 安装目标，'
                '或通过 FIBRE_LIBRARY_PATH 指定构建产物。')
        os.environ['FIBRE_LIBRARY_PATH'] = str(library)

    if os.name == 'nt':
        directory = str(Path(os.environ['FIBRE_LIBRARY_PATH']).expanduser().resolve().parent)
        if hasattr(os, 'add_dll_directory'):
            _native_directory_handle = os.add_dll_directory(directory)
        else:
            os.environ['PATH'] = directory + os.pathsep + os.environ['PATH']
