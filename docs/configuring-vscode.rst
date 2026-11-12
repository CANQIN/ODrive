.. _configuring-vscode:

================================================================================
Configuring Visual Studio Code
================================================================================

.. contents::
   :depth: 1
   :local:
   
VSCode is the recommended IDE for working with the ODrive codebase.  
It is a light-weight text editor with Git integration and GDB debugging functionality.

Before doing the VSCode setup, make sure you've installed all of your :ref:`prerequisites. <dev-prereq>`

Setup Procedure
--------------------------------------------------------------------------------

#. Clone the ODrive repository
#. `Download VSCode <https://code.visualstudio.com/download>`_
#. Open VSCode
#. Install extensions.  This can be done directly from VSCode (:kbd:`Ctrl` **+** :kbd:`Shift` **+** :kbd:`X`)

    * Required extensions:
        * C/C++ :code:`ext install ms-vscode.cpptools`
        * Cortex-Debug :code:`ext install marus25.cortex-debug`
        * Cortex-Debug: Device Support Pack - STM32F4 :code:`ext install marus25.cortex-debug-dp-stm32f4`

    * Recommended Extensions:
        * Include Autocomplete
        * Path Autocomplete
        * Auto Comment Blocks

#. Create an environment variable named :code:`ARM_GCC_ROOT` whose value is the location of the :code:`GNU Arm Embedded Toolchain` (.e.g :code:`C:\Program Files (x86)\GNU Tools Arm Embedded\7 2018-q2-update`) that you installed in the prerequisites section of the developer's guide. 
   This is not strictly needed for Linux or Mac, and you can alternatively use the :code:`Cortex-debug: Arm Toolchain Path` setting in VSCode extension settings.
#. Relaunch VSCode 
#. Open the VSCode Workspace file, which is located in the root of the ODrive repository.  It is called :code:`ODrive_Workspace.code-workspace`.  
   The first time you open it, VSCode will install some dependencies.  If it fails, you may need to `change your proxy settings <https://code.visualstudio.com/docs/getstarted/settings>`_.

You should now be ready to compile and test the ODrive project.

Building the Firmware
--------------------------------------------------------------------------------

* **Terminal** -> **Run Build Task** (:kbd:`Ctrl1` **+** :kbd:`Shift` **+** :kbd:`B`)

当前 ``build`` 任务从仓库根目录配置 CMake，并执行 ``firmware`` 目标；Windows 使用根目录的 ``build.ps1``。
命令行也可以在仓库根目录执行：

.. code:: bash

  cmake -S . -B build -G Ninja -DODRIVE_BOARD_VERSION=v3.6-56V -DCMAKE_BUILD_TYPE=Release
  cmake --build build --target firmware --parallel

固件输出为 ``build/firmware/ODriveFirmware.elf``、``.hex`` 和 ``.bin``。

Flashing the Firmware
--------------------------------------------------------------------------------

当前 VSCode 配置只提供构建任务。按 :ref:`开发指南的 STLink 烧录流程 <flashing-with-an-stlink>`，
使用 OpenOCD 手动烧录 ``build/firmware/ODriveFirmware.elf``。

If the flashing worked, you can connect to the board using the :ref:`odrivetool <odrivetool-startup>`.

Debugging
--------------------------------------------------------------------------------

An extension called Cortex-Debug has recently been released which is designed specifically for debugging ARM Cortex projects.  
You can read more on Cortex-Debug `here. <https://github.com/Marus/cortex-debug>`_

Note: If developing on Windows, you should have :code:`arm-none-eabi-gdb` and :code:`openOCD` on your PATH.

  * Make sure you have the Firmware folder as your active folder
  * 在仓库根目录用 :code:`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` 配置，
    再执行 :code:`cmake --build build --target firmware --parallel`。
    Windows 也可使用 :code:`./build.ps1 -Configuration Debug -Target firmware`。
  * Flash the board with the newest code (starting debug session doesn't do this)
  * 在 **Run** 页签 (:kbd:`Ctrl` **+** :kbd:`Shift` **+** :kbd:`D`) 选择对应板型配置，例如 "Debug ODrive v3.x - ST-Link"。
  * Press **Start Debugging** (or press :kbd:`F5`)
  * The processor will reset and halt.
  * Set your breakpoints. Note: you can only set breakpoints when the processor is halted, if you set them during run mode, they won't get applied.
  * **Continue** (press :kbd:`F5`)
  * Stepping over/in/out, restarting, and changing breakpoints can be done by first pressing the "pause" (F6) button at the top the screen.
  * When done debugging, simply stop (:kbd:`Shift` **+** :kbd:`F5`) the debugger.  It will kill your openOCD process too.

Cleaning the Build
--------------------------------------------------------------------------------

This sometimes needs to be done if you change branches.

* 在仓库根目录执行 :code:`cmake --build build/firmware --target clean` 清理 MCU 构建产物，
  再通过 ``build`` 任务或 :code:`cmake --build build --target firmware --parallel` 重建。
