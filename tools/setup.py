"""
This script is used to deploy the ODrive python tools to PyPi
so that users can install them easily with
"pip install odrive"

To install the package and its dependencies locally, run:
    sudo pip install -r requirements.txt

To build and package the python tools into a tar archive:
    python setup.py sdist

Warning: Before you proceed, be aware that you can upload a
specific version only once ever. After that you need to increment
the hotfix number. Deleting the release manually on the PyPi
website does not help.

Use TestPyPi while developing.

To upload the python package to TestPyPi, run:
    twine upload dist/odrive-0.5.3.dev0.tar.gz -r pypitest
(Replace the file name with something appropriate)

To make a real release ensure the version number in the filename matches what
you intend to release and then run the above command without the "test" (so just
"pypi").

To install a prerelease version from test index:
(extra-index-url is there because some packages don't upload to test server)
    sudo pip install --pre --index-url https://test.pypi.org/simple/ --extra-index-url https://pypi.org/simple/ --no-cache-dir odrive


PyPi access requires that you have set up ~/.pypirc with your
PyPi credentials and that your account has the rights
to publish packages with the name odrive.
"""

# Set to true to make the current release
is_release = True

# Set to true to make an official post-release, rather than dev of new version
is_post_release = False
post_rel_num = 0

# To test higher numbered releases, bump to the next rev
devnum = 0
bump_rev = not is_post_release and not is_release

# TODO: add additional y/n prompt to prevent from erroneous upload

from setuptools import setup
import os
import sys
import runpy

if sys.version_info < (3, 3):
  import exceptions
  PermissionError = exceptions.OSError

creating_package = "sdist" in sys.argv

# Load version from Git tag
version_module = runpy.run_path(os.path.join(os.path.dirname(__file__), 'odrive', 'version.py'))
package_version = os.environ.get('ODRIVE_PACKAGE_VERSION')
if package_version:
  version = package_version
else:
  version = version_module['get_version_str'](
      git_only=creating_package,
      is_post_release=is_post_release,
      bump_rev=bump_rev,
      release_override=is_release)

# If we're currently creating the package we need to autogenerate
# a file that contains the version string
if creating_package:
  if not package_version:
    if is_post_release:
      version += str(post_rel_num)
    elif (devnum > 0):
      version += str(devnum)

  version_file_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'odrive', 'version.txt')
  with open(version_file_path, mode='w') as version_file:
    version_file.write(version)

# TODO: find a better place for this
if not creating_package:
  import platform
  if platform.system() == 'Linux':
    try:
      version_module['setup_udev_rules'](None)
    except Exception:
      print("Warning: could not set up udev rules. Run `sudo odrivetool udev-setup` to try again.")

try:
  setup(
    name = 'odrive',
    packages = ['odrive', 'odrive.dfuse', 'odrive.pyfibre'],
    scripts = ['odrivetool', 'odrivetool.bat', 'odrive_demo.py'],
    version = version,
    description = 'Control utilities for the ODrive high performance motor controller',
    author = 'Oskar Weigl',
    author_email = 'oskar.weigl@odriverobotics.com',
    license='MIT',
    url = 'https://github.com/odriverobotics/ODrive',
    keywords = ['odrive', 'motor', 'motor control'],
    install_requires = [
      'fibre==0.1.4',
      'ipython',  # Used to do the interactive parts of the odrivetool
      'PyUSB',    # USB communication and DFU are implemented by the ODrive application.
      'requests', # Used to by DFU to load firmware files
      'IntelHex', # Used to by DFU to download firmware from github
      'matplotlib', # Required to run the liveplotter
      'monotonic', # For compatibility with older python versions
      'setuptools',  # ubuntu-latest on GitHub Actions fails to install odrive without this dependency
      'pywin32 >= 222; platform_system == "Windows"' # Required for fancy terminal features on Windows
    ],
    package_data={
      'odrive': ['version.txt'],
      'odrive.pyfibre': ['runtime/*.so*', 'runtime/*.dll', 'runtime/*.dylib'],
    },
    classifiers = [],
  )

  # TODO: include README

finally:
  # clean up
  if creating_package:
    os.remove(version_file_path)
