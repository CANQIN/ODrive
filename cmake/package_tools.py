"""将 Python 代码、新客户端和本轮枚举暂存后打包，不写入源码目录。"""
import argparse
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--native-prefix', type=Path, required=True)
parser.add_argument('--enums', type=Path, required=True)
args = parser.parse_args()

libraries = [path for directory in ('bin', 'lib')
             for path in (args.native_prefix / directory).glob('*')
             if path.name in ('fibre.dll', 'libfibre.dll', 'libfibre.so', 'libfibre.dylib')]
if len(libraries) != 1:
    raise RuntimeError('新客户端安装目录中应有一个 C ABI 动态库：' + str(args.native_prefix))

staging = args.build.resolve() / 'staging'
staging.mkdir(parents=True, exist_ok=True)
output = (args.build / 'dist').resolve()
output.mkdir(parents=True, exist_ok=True)
version = runpy.run_path(str(args.repo / 'tools/odrive/version.py'))['get_version_str'](
    git_only=True, release_override=True)
environment = dict(os.environ, ODRIVE_PACKAGE_VERSION=version)
ignore = shutil.ignore_patterns('__pycache__', '*.egg-info', 'build', 'dist', '.gitignore')

with tempfile.TemporaryDirectory(prefix='tools-', dir=staging) as temporary:
    temporary = Path(temporary)
    temporary.resolve().relative_to(staging)
    sdk = temporary / 'sdk'
    tools = temporary / 'tools'
    shutil.copytree(args.repo / 'packages/fibre/bindings/python', sdk, ignore=ignore)
    shutil.copytree(args.repo / 'tools/odrive', tools / 'odrive', ignore=ignore)
    for filename in ('setup.py', 'odrivetool', 'odrivetool.bat', 'odrive_demo.py'):
        shutil.copy2(args.repo / 'tools' / filename, tools / filename)
    shutil.copy2(args.enums, tools / 'odrive/enums.py')
    runtime = tools / 'odrive/pyfibre/runtime'
    runtime.mkdir(parents=True, exist_ok=True)
    shutil.copy2(libraries[0], runtime / libraries[0].name)
    for source in (sdk, tools):
        subprocess.run([sys.executable, 'setup.py', 'sdist', '--dist-dir', str(output)],
                       cwd=source, env=environment, check=True)
print('Python 分发包已生成：' + str(output))
