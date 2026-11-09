[CmdletBinding()]
param(
    [ValidateSet('v3.1', 'v3.2', 'v3.3', 'v3.4-24V', 'v3.4-48V', 'v3.5-24V', 'v3.5-48V', 'v3.6-24V', 'v3.6-56V')]
    [string]$BoardVersion
)

$ErrorActionPreference = 'Stop'
$firmware = Join-Path $PSScriptRoot 'Firmware'
$pythonBin = Join-Path $PSScriptRoot '.venv/Scripts'
$tupBin = Join-Path $PSScriptRoot '.tools/tup-0.8'
$gccRoot = Join-Path $PSScriptRoot '.tools/gcc-arm-none-eabi-9-2019-q4-major'
foreach ($executable in @("$pythonBin/python.exe", "$tupBin/tup.exe", "$gccRoot/bin/arm-none-eabi-gcc.exe")) {
    if (-not (Test-Path $executable)) {
        throw "Missing $executable. Run .\scripts\setup-build-env.ps1 first."
    }
}

$configPath = Join-Path $firmware 'tup.config'
if (-not (Test-Path $configPath)) {
    $config = (Get-Content (Join-Path $firmware 'tup.config.default') -Raw).Replace('#CONFIG_BOARD_VERSION=v3.6-56V', 'CONFIG_BOARD_VERSION=v3.6-56V')
} else {
    $config = Get-Content $configPath -Raw
}
if ($BoardVersion) {
    if ($config -match '(?m)^CONFIG_BOARD_VERSION=') {
        $config = [regex]::Replace($config, '(?m)^CONFIG_BOARD_VERSION=[^\r\n]*', "CONFIG_BOARD_VERSION=$BoardVersion")
    } else {
        $config += "`nCONFIG_BOARD_VERSION=$BoardVersion`n"
    }
}
[IO.File]::WriteAllText($configPath, $config, [Text.UTF8Encoding]::new($false))

$previousPath = $env:PATH
$previousArmRoot = $env:ARM_GCC_ROOT
$env:PATH = "$pythonBin;$tupBin;$gccRoot/bin;$env:PATH"
$env:ARM_GCC_ROOT = $gccRoot
Push-Location $firmware
try {
    New-Item -ItemType Directory -Force autogen, build | Out-Null
    # Standalone tup generate requires outputs from the previous run to be absent.
    # Keep version.c, which is generated separately and is an input to Tup.
    foreach ($output in @(
        'autogen/interfaces.hpp', 'autogen/function_stubs.hpp',
        'autogen/endpoints.hpp', 'autogen/type_info.hpp',
        'build/ODriveFirmware.elf', 'build/ODriveFirmware.hex',
        'build/ODriveFirmware.bin', 'build/ODriveFirmware.map',
        'build/ODriveFirmware.asm'
    )) {
        if (Test-Path -LiteralPath $output) {
            Remove-Item -LiteralPath $output -Force
        }
    }
    if (Test-Path -LiteralPath 'build/obj') {
        Get-ChildItem -LiteralPath 'build/obj' -Filter '*.o' -File | ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Force
        }
    }
    Write-Host (($config -split '\r?\n' | Where-Object { $_ -match '^CONFIG_(BOARD_VERSION|DEBUG|USE_LTO)=' }) -join "`n")
    & python -B ../tools/odrive/version.py --output autogen/version.c
    if ($LASTEXITCODE -ne 0) { throw 'Version generation failed.' }
    if (-not (Test-Path '.tup')) {
        & tup init
        if ($LASTEXITCODE -ne 0) { throw 'Tup initialization failed.' }
    }
    # Same standalone build path as .github/workflows/firmware.yaml on Windows.
    & tup generate build/tup-build.bat
    if ($LASTEXITCODE -ne 0) { throw 'Tup build generation failed.' }
    # Tup emits a batch file without error checks. Stop at the first failed rule.
    $batch = foreach ($line in Get-Content build/tup-build.bat) {
        if ($line -match '^mkdir (.+)$') {
            "if not exist $($Matches[1]) $line"
        } elseif ($line.StartsWith('(')) {
            "$line || exit /b 1"
        } else {
            $line
        }
    }
    [IO.File]::WriteAllLines((Join-Path $firmware 'build/tup-build.bat'), [string[]]$batch, [Text.UTF8Encoding]::new($false))
    Write-Host 'Compiling firmware; log: Firmware/build/build.log'
    & cmd.exe /d /c 'call build\tup-build.bat 2>&1' | Tee-Object -FilePath build/build.log
    if ($LASTEXITCODE -ne 0) {
        throw 'Firmware compilation failed. See Firmware/build/build.log.'
    }
    foreach ($extension in @('elf', 'hex', 'bin', 'map')) {
        $artifact = Get-Item "build/ODriveFirmware.$extension"
        if ($artifact.Length -eq 0) { throw "Empty build output: $artifact" }
    }
    & arm-none-eabi-size build/ODriveFirmware.elf
    if ($LASTEXITCODE -ne 0) { throw 'Firmware ELF validation failed.' }
    Write-Host 'Build succeeded: Firmware/build/ODriveFirmware.{elf,hex,bin,map}'
} finally {
    Pop-Location
    $env:PATH = $previousPath
    $env:ARM_GCC_ROOT = $previousArmRoot
}
