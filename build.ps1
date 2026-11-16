[CmdletBinding()]
param(
    [ValidateSet('v3.6-56V')]
    [string]$BoardVersion,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory = 'build',
    [string]$Python,
    [string]$ArmToolchainDir,
    [string]$Target = 'all'
)

$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$ninja = (Get-Command ninja -ErrorAction Stop).Source
if (-not $Python) {
    $localPython = Join-Path $repo '.venv/Scripts/python.exe'
    $Python = if (Test-Path -LiteralPath $localPython) { $localPython } else { (Get-Command python -ErrorAction Stop).Source }
}
if (-not $ArmToolchainDir) {
    $ArmToolchainDir = $env:ARM_TOOLCHAIN_DIR
}
if (-not $ArmToolchainDir) {
    $portableArm = Join-Path $repo '.tools/gcc-arm-none-eabi-9-2019-q4-major'
    if (Test-Path -LiteralPath (Join-Path $portableArm 'bin/arm-none-eabi-gcc.exe')) {
        $ArmToolchainDir = $portableArm
    }
}
if (-not [IO.Path]::IsPathRooted($BuildDirectory)) {
    $BuildDirectory = Join-Path $repo $BuildDirectory
}
$buildPath = [IO.Path]::GetFullPath($BuildDirectory)
$configure = @('-S', $repo, '-B', $buildPath, '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_MAKE_PROGRAM=$ninja", "-DPython3_EXECUTABLE=$Python")
if ($BoardVersion) { $configure += "-DODRIVE_BOARD_VERSION=$BoardVersion" }
if ($ArmToolchainDir) { $configure += "-DARM_TOOLCHAIN_DIR=$ArmToolchainDir" }
& $cmake @configure
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败。' }

$logPath = Join-Path $buildPath 'build.log'
& $cmake --build $buildPath --target $Target --parallel 2>&1 | Tee-Object -FilePath $logPath
if ($LASTEXITCODE -ne 0) { throw "编译失败，日志：$logPath" }
Write-Host "构建完成：$buildPath；固件位于 firmware/，主机 SDK 位于 host/install/。"
