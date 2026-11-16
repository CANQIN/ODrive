.. _developer-guide-doc:

================================================================================
ODrive Firmware Developer Guide
================================================================================

.. contents::
   :depth: 1
   :local:


This guide is intended for developers who wish to modify the firmware of the ODrive.
As such it assumes that you know things like how to use Git, what a compiler is, etc. If that sounds scary, turn around now.

The official releases are maintained on the `master` branch. However since you are a developer, you are encouraged to use the `devel` branch, as it contains the latest features.

The project is under active development, so make sure to check the `Changelog <https://github.com/odriverobotics/ODrive/tree/master/CHANGELOG.md>`_ to keep track of updates.

.. _dev-prereq:

构建依赖
-------------------------------------------------------------------------------

统一构建入口位于仓库根目录，使用 CMake 3.20 或更新版本和 Ninja。
固件交叉编译需要 ARM GNU 工具链；默认主机 SDK 构建还需要本机 C/C++ 编译器。
Python 3.10 及 ``scripts/requirements-build.txt`` 中的依赖用于接口和版本生成。

.. code:: bash

  cmake --version
  ninja --version
  arm-none-eabi-gcc --version
  python -m pip install -r scripts/requirements-build.txt

工具链可放在 ``PATH`` 中，也可通过 ``ARM_TOOLCHAIN_DIR`` 指定工具链根目录或 ``bin`` 目录。
OpenOCD、ARM GDB 和 ST-Link 驱动用于用户主动发起的烧录或调试，不是默认构建依赖。

Windows PowerShell 构建
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

先安装 CMake、Ninja、本机 C/C++ 编译器及带 Python launcher 的 Python 3.10。
在仓库根目录运行：

.. code:: powershell

  .\scripts\setup-build-env.ps1
  .\build.ps1 -BoardVersion v3.6-56V

环境脚本检查 CMake 和 Ninja，下载并校验 ARM GCC 9-2019-q4-major，
在 ``.venv`` 中安装接口生成依赖。脚本不再安装旧构建系统。
指定 Python 或包索引时可使用：

.. code:: powershell

  .\scripts\setup-build-env.ps1 -Python D:\Python\Python310\python.exe -IndexUrl https://pypi.org/simple

``build.ps1`` 调用根 CMake，不删除已有生成文件。默认输出目录为 ``build/``，
可通过 ``-BuildDirectory`` 更换，通过 ``-Configuration Debug`` 选择调试配置。
``-Target firmware`` 只构建固件，默认 ``all`` 构建启用的所有目标。
VSCode 根目录或 Firmware 目录的默认构建任务都使用此入口；构建日志位于 ``build/build.log``。

配置与产物
--------------------------------------------------------------------------------

在仓库根目录执行：

.. code:: bash

  cmake -S . -B build -G Ninja -DODRIVE_BOARD_VERSION=v3.6-56V -DCMAKE_BUILD_TYPE=Release
  cmake --build build --parallel

默认同时启用固件和主机 SDK。两个子工程使用不同编译器与构建目录：

* 固件：``build/firmware/ODriveFirmware.{elf,hex,bin,map}``。
* 主机 SDK：``build/host/install/``。
* Python 枚举：``build/generated/python/``。

常用选项：

* ``ODRIVE_BUILD_FIRMWARE``、``ODRIVE_BUILD_HOST``：默认 ``ON``，按需关闭某一侧。
* ``ODRIVE_BOARD_VERSION``：自动发现 ``Firmware/Boards`` 中的完整板型配置；当前实现 ``v3.6-56V``（STM32F405）。
* ``CMAKE_BUILD_TYPE``：``Release`` 或 ``Debug``。
* ``ODRIVE_USE_LTO``、``ODRIVE_STRICT``：默认 ``OFF``，分别控制链接优化与警告视为错误。
* ``ARM_TOOLCHAIN_DIR``、``Python3_EXECUTABLE``：显式选择 ARM 工具链和 Python。
* ``ODRIVE_BUILD_TOOLS``、``ODRIVE_BUILD_GUI``：默认 ``OFF``；启用后提供 ``tools``、
  ``gui_web``、``gui_package`` 可选目标。

``interfaces`` 目标生成供主机工具和 GUI 使用的枚举，写入构建目录。
``tools`` 打包本轮 SDK 和 ODrive Python 工具；GUI 构建参见 ``GUI/README.md``。
这些目标不运行服务、不连接或烧录设备。

容器构建使用根 Dockerfile，需要把完整仓库挂载到 ``/ODrive``。
固件版本由 Git 标签生成，缺少本分支所需固件标签时应先取得对应标签。

.. _build-and-flash:

构建与手动烧录
--------------------------------------------------------------------------------

#. 在仓库根目录执行 :code:`cmake --build build --target firmware`。
#. Connect the ODrive via USB and power it up.
#. Flash the firmware using :ref:`odrivetool dfu <firmware-update>`.

.. _flashing-with-an-stlink: 

Flashing using an STLink/v2 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Connect `GND`, `SWD`, and `SWC` on connector J2 to the programmer. 

  .. note:: Always plug in `GND` first!

* You need to power the board by only **ONE** of the following: VCC(3.3v), 5V, or the main power connection (the DC bus). The USB port (J1) does not power the board.
* 构建不会自动烧录。使用 OpenOCD 手动指定 ``build/firmware/ODriveFirmware.elf``，或使用下述 DFU 工具。

在仓库根目录手动烧录 v3 板型：

.. code:: bash

  openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program {build/firmware/ODriveFirmware.elf} verify reset exit"

需要选择特定 ST-Link 时，在目标配置前增加 ``-c "hla_serial <序列号>"``。
v4 板型使用 ``target/stm32f7x.cfg``，并在 ``program`` 命令前增加 ``-c "reset_config none separate"``。

.. note:: If you receive the error `can't find target interface/stlink-v2.cfg` or similar, create and set an environment variable named :code:`OPENOCD_SCRIPTS` to the location of the openocd scripts directory.

If the flashing worked, you can connect to the board using the :ref:`odrivetool <odrivetool-startup>`.

Testing
--------------------------------------------------------------------------------

.. include:: testing.rst

Debugging
--------------------------------------------------------------------------------

If you're using VSCode, make sure you have the Cortex Debug extension, OpenOCD, and the STLink.  
You can verify that OpenOCD and STLink are working by ensuring you can flash code.  
Open the ODrive_Workspace.code-workspace file, and start a debugging session (F5).  
VSCode will pick up the correct settings from the workspace and automatically connect.  
Breakpoints can be added graphically in VSCode.

* 手动使用 :code:`arm-none-eabi-gdb build/firmware/ODriveFirmware.elf`，或选择 VSCode 中相应的 Cortex-Debug 配置。

Setting up an IDE
--------------------------------------------------------------------------------

For working with the ODrive code you don't need an IDE, but the open-source IDE VSCode is recommended.  
It is also possible to use Eclipse. If you'd like to go that route, please see the respective configuration document:

* :ref:`Configuring VSCode <configuring-vscode>`
* :ref:`Configuring Eclipse <configuring-eclipse>`


STM32CubeMX
--------------------------------------------------------------------------------

The v3.6-56V project is generated with **STM32CubeMX 6.18.1** and
**STM32CubeF4 1.28.3**, targeting STM32F405RGT6. The firmware package includes
HAL, CMSIS device/core headers, FreeRTOS 10.3.1 (CMSIS-RTOS v1), and USB Device Core.
Generated files are checked in, so normal CMake builds do not require CubeMX.

Ownership and layout
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ``Firmware/Boards/<board>/board.json`` records the MCU, pinned tool versions,
  IOC configuration. CMake discovers these
  profiles automatically; select one with ``ODRIVE_BOARD_VERSION``.
* ``Firmware/Boards/v3.6-56V/CubeMX/CubeMX.ioc`` owns clocks, pins, peripheral
  initialization, DMA/NVIC priorities and RTOS configuration. Its ``Core``,
  ``Drivers``, ``Middlewares`` and ``USB_DEVICE`` contain actual generated code.
  Edits to generated files belong only in ``USER CODE`` sections.
* ``board.cpp`` maps application objects to generated handles, applies runtime
  baud/GPIO settings and implements motor sampling/control interrupts.
  ``board.cmake`` selects generated sources and the MCU platform. The board also
  owns its linker script, small hardware hooks and ``verify.py`` invariants.
* ``Firmware/Platform/STM32`` contains RTOS support and ODrive's CDC + native USB
  class (VID/PID 1209:0D32, WinUSB). USB device startup, PCD/MSP and HAL callbacks
  use generated code; a small LL-init wrapper adds the board's native EP3 FIFO
  allocation, for which the F4 CubeMX template has no USER CODE hook.
* ``Firmware/Platform/STM32F4`` selects the F4 platform and fault handling.
  ``Firmware/ThirdParty/CMSIS_DSP`` retains the DSP 1.5.3 headers and M4 hard-float
  math archive to avoid changing motor math during this migration.

The generated main function is renamed; application startup and task creation
remain in ``MotorControl/main.cpp``. Default RTOS tasks and USB class/descriptor
skeletons are removed by the regeneration script, since the application supplies
those pieces. There are no parallel handwritten CAN/UART initialization files.
TIM5 supplies a generated base timer, with PWM capture channels selected at runtime.

Configuration format changes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Removal of the obsolete bus interface changes the byte offsets in ``BoardConfig_t``.
NVM configuration version is now 2; version 1 data is rejected and startup uses the
default configuration. Reconfigure and save settings after installing this firmware.
GPIO enum value 8 remains unused; the remaining mode values keep their wire IDs.

Adding a board
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See ``Firmware/Boards/README.md`` for the profile contract and onboarding steps.
Add a profile with its own IOC, board mapping, CMake source list, linker layout and
validation. Use a separate build directory and ``-DODRIVE_BOARD_VERSION=<board>``.
The current platform supports STM32F4/Cortex-M4F; a different MCU family or motor
sampling architecture requires a platform/control port in addition to CubeMX.
Only v3.6-56V is currently implemented.

Regeneration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Install CubeMX 6.18.1 and STM32CubeF4 1.28.3 using the CubeMX package manager.
Open ``Firmware/Boards/v3.6-56V/CubeMX/CubeMX.ioc`` to edit the configuration, save it,
then run from the repository root:

.. code:: powershell

   ./scripts/regenerate-cubemx.ps1 -Board v3.6-56V -CubeMXPath D:/STM32CubeMX/STM32CubeMX.exe
   cmake --build build --target firmware tests --parallel 8

``STM32CUBEMX_PATH`` can also identify the executable or its installation directory.
Enable ``ODRIVE_BUILD_TESTS=ON`` and ``ODRIVE_STRICT=ON`` when configuring CMake for
validation. Generation logs are written to ``build/cubemx/<board>``. The script uses the
bundled Java CLI, preserves USER CODE, removes unused vendor documentation/device
examples and generic build files, then runs ``scripts/check-cubemx.py``. Build and
flash only the CMake firmware, which uses the ODrive linker/memory layout.

Keep the firmware package pinned (``LastFirmware=false``), library copy mode at
"copy all", and "Keep User Code" enabled.
The former STM32CubeMX-start/end branch-rebase workflow is no longer needed.

Configuration invariants
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The checked configuration keeps 168 MHz system/PWM clocks, 84 MHz APB1 timer
clocks, TIM1/8 period 3500, repetition counter 2 and dead time 20. TIM13 has ARR
10499; its integer expression divides after multiplication to avoid truncating
the APB1/PWM clock ratio to zero. Timing constants live in IOC ``Mcu.UserConstants``
and are emitted into ``Core/Inc/main.h``.

SPI TX/RX DMA IRQ priorities remain 3/4, with high/medium DMA arbitration.
TIM5 has priority 1, TIM8/TIM13 priority 0, control-loop IRQ priority 5, USB and
HAL tick priority 6, CAN priority 9, and UART priority 10. High-priority ISRs do
not use FreeRTOS APIs: leave "Uses FreeRTOS functions" unchecked for them in CubeMX.
ADC DMA is polled and its interrupt must remain disabled.

USER CODE hooks retain SPI DMA alignment when switching between 8-bit and 16-bit
transactions, PWM shutdown on fatal initialization errors, the 64 KB application
heap in CCM, and the FreeRTOS tick mapping. The shared platform supplies the SysTick wrapper. GPIO runtime configuration is applied
after the generated peripheral setup. FLASH remains 768 KB plus 256 KB reserved
NVM, SRAM 128 KB, and CCM 64 KB. The custom linker and ``USER_VECT_TAB_ADDRESS``
compile definition preserve the application vector table at FLASH base.

Host tests cover the generated configuration and the actual custom USB class
against the new ST core (enumeration, interface 2, WinUSB, two bulk endpoint pairs,
and zero-length packet completion). They do not replace hardware validation:
check USB/UART/CAN communication, encoder SPI, PWM/ADC synchronization, configuration
save/reboot, and motor operation on a v3.6-56V board before deployment.

Troubleshooting
--------------------------------------------------------------------------------

:code:`LIBUSB_ERROR_IO` when flashing with the STLink/v2
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Problem:** when I try to flash the ODrive with the STLink using OpenOCD 手动烧录命令 I get this error:

.. code:: Bash

  Open On-Chip Debugger 0.10.0
  Licensed under GNU GPL v2
  For bug reports, read
    http://openocd.org/doc/doxygen/bugs.html
  Info : auto-selecting first available session transport "hla_swd". To override use 'transport select <transport>'.
  Info : The selected transport took over low-level target control. The results might differ compared to plain JTAG/SWD
  adapter speed: 2000 kHz
  adapter_nsrst_delay: 100
  none separate
  Info : Unable to match requested speed 2000 kHz, using 1800 kHz
  Info : Unable to match requested speed 2000 kHz, using 1800 kHz
  Info : clock speed 1800 kHz
  Error: libusb_open() failed with LIBUSB_ERROR_IO
  Error: open failed
  in procedure 'init' 
  in procedure 'ocd_bouncer'

**Solution:**
This happens from time to time.

#. Unplug the STLink and all ODrives from your computer
#. Power off the ODrive you're trying to flash
#. Plug in the STLink into your computer
#. Power on the ODrive
#. Run OpenOCD 手动烧录命令 again

Documentation
--------------------------------------------------------------------------------
.. admonition:: In Progress 

  ODrive documentation is now built using `Sphinx <https://www.sphinx-doc.org/en/master/>`_.

  * Prerequisites:

    * Install Sphinx: :code:`pip install -U sphinx`
    * Install packages: :code:`pip install sphinx-copybutton sphinx-panels sphinx-rtd-theme`


  * Run :code:`make html` within the :kbd:`./docs/reStructuredText` folder 
  * Open :kbd:`./docs/reStructuredText/_build/index.html` to view

.. _modifying-libfibre:

修改 Fibre SDK
--------------------------------------------------------------------------------

平台无关 SDK 位于 ``packages/fibre/``。修改后通过统一入口重建：

.. code:: bash

  cmake -S . -B build -G Ninja
  cmake --build build --parallel

主机库安装到 ``build/host/install/``。Python 客户端使用应用提供的传输实现，
原生库路径通过 ``FIBRE_LIBRARY_PATH`` 指定；MCU 通过收发接口和端点实现接入。
独立 SDK 的接入细节参见 ``packages/fibre/README.md``。
跨平台库需使用目标平台的工具链单独构建，不使用固件的 ARM 构建目录。

Releases
--------------------------------------------------------------------------------

We use GitHub Releases to provide firmware releases.

#. Cut off the changelog to reflect the new release
#. Merge the release candidate into master.
#. Push a (lightweight) tag to the master branch. Follow the existing naming convention.
#. 修改 Fibre 后，使用各目标平台工具链构建 SDK 及应用适配，见 :ref:`修改 Fibre SDK <modifying-libfibre>`。
#. Push the python tools to PyPI (see setup.py for details).
#. Edit the release on GitHub to add a title and description (copy&paste from changelog).

Code Maintenance Notes
--------------------------------------------------------------------------------

The cortex M4F processor has hardware single precision float unit. However double precision operations are not accelerated, and hence should be avoided. 
The following regex is helpful for cleaning out double constants:

find: 

.. code::

  ([-+]?[0-9]+\.[0-9]+(?:[eE][-+]?[0-9]+)?)([^f0-9e])

replace: 

  .. code::

    \1f\2

Notes for Contributors
--------------------------------------------------------------------------------

In general the project uses the `Google C++ Style Guide <https://google.github.io/styleguide/cppguide.html>`__ with a few exceptions:

 * The default indentation is 4 spaces.
 * The 80 character limit is not very strictly enforced, merely encouraged.
 * The file extensions `*.cpp` and `*.hpp` are used instead of `*.cc` and `*.h`.

Your help is welcome! However before you start working on a feature/change that will take you a non-negligible amount of time and that you plan to upstream please discuss your plans with us on GitHub or Discord. 
This will ensure that your implementation is in line with the direction that ODrive is going.

When filing a PR please go through this checklist:

 * Make sure you adhere to the same coding style that we use (see note above).
 * Update CHANGELOG.md.
 * If you removed/moved/renamed things in :code:`odrive-interface.yaml` make sure to add corresponding bullet points tp the "API migration notes" section in the changelog. Use git to compare against the :code:`devel` branch.
 * Also, for each removed/moved/renamed API item use your IDE's search feature to search for occurrences of this name. Update the places you found (this will usually be documentation and test scripts).
 * If you added things to :code:`odrive-interface.yaml` make sure the new things have decent documentation in the YAML file. We don't expect 100% coverage but use good sense of what to document.
 * Make sure your PR doesn't contain spurious changes that unnecessarily add or remove whitespace. These add noise and make the reviewer's lifes harder.
 * If you changed any enums in :code:`odrive-interface.yaml`, regenerate ``tools/odrive/enums.py`` and the GUI enums from that definition.
   The file includes instructions on how to do this. Check the diff to verify that none of the existing enumerators changed their value.
