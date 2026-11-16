param(
    [string]$CubeMXPath = $env:STM32CUBEMX_PATH,
    [string]$Board = "v3.6-56V"
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ($Board -notmatch '^[a-zA-Z0-9_.-]+$') { throw "Invalid board ID: $Board" }
$boardRoot = Join-Path $repoRoot "Firmware/Boards/$Board"
$manifestPath = Join-Path $boardRoot 'board.json'
if (-not (Test-Path -LiteralPath $manifestPath)) { throw "Unknown board: $Board" }
$boardInfo = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$ioc = (Resolve-Path -LiteralPath (Join-Path $boardRoot $boardInfo.cube_project)).Path
$cubeRoot = Split-Path $ioc
$boardRoot = (Resolve-Path -LiteralPath $boardRoot).Path
if (-not $cubeRoot.StartsWith($boardRoot + [IO.Path]::DirectorySeparatorChar,
    [StringComparison]::OrdinalIgnoreCase)) { throw 'CubeMX project must be inside the selected board profile.' }
$logDir = Join-Path $repoRoot "build/cubemx/$Board"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

if (-not $CubeMXPath) {
    $CubeMXPath = @(
        'D:/STM32CubeMX/STM32CubeMX.exe',
        'C:/Program Files/STMicroelectronics/STM32Cube/STM32CubeMX/STM32CubeMX.exe'
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $CubeMXPath) {
    throw "Specify -CubeMXPath or STM32CUBEMX_PATH (CubeMX $($boardInfo.cube_version))."
}
$CubeMXPath = (Resolve-Path -LiteralPath $CubeMXPath).Path
if (Test-Path -LiteralPath $CubeMXPath -PathType Container) {
    $CubeMXPath = Join-Path $CubeMXPath 'STM32CubeMX.exe'
}
$java = Join-Path (Split-Path $CubeMXPath) 'jre/bin/java.exe'
if (-not (Test-Path -LiteralPath $java)) { throw "Bundled Java not found: $java" }
$python = Join-Path $repoRoot '.venv/Scripts/python.exe'
if (-not (Test-Path -LiteralPath $python)) { $python = (Get-Command python -ErrorAction Stop).Source }

# Loading the IOC establishes its own output directory; do not redirect the
# project to a staging directory, as that loses existing USER CODE sections.
function Invoke-CubeProject([string]$project) {
$project = (Resolve-Path -LiteralPath $project).Path
if (-not $project.StartsWith($boardRoot + [IO.Path]::DirectorySeparatorChar,
    [StringComparison]::OrdinalIgnoreCase)) { throw 'Project is outside the board profile.' }
$tag = [IO.Path]::GetFileNameWithoutExtension($project)
$commandFile = Join-Path $logDir "$tag.mx"
$stdout = Join-Path $logDir "$tag.log"
$stderr = Join-Path $logDir "$tag.err.log"
[IO.File]::WriteAllLines($commandFile, @("config load $project", 'project generate', 'exit'),
    [Text.UTF8Encoding]::new($false))
Write-Host "Generating $Board with CubeMX $($boardInfo.cube_version) (log: $stdout)..."
$process = Start-Process -FilePath $java -ArgumentList @('-jar', "`"$CubeMXPath`"", '-q', "`"$commandFile`"") `
    -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$null = $process.Handle # Retain the handle so Windows PowerShell can read ExitCode after exit.
$deadline = [DateTime]::UtcNow.AddMinutes(3)
while (-not $process.WaitForExit(1000)) {
    if ((Get-Content -LiteralPath $stdout -Tail 12 -ErrorAction SilentlyContinue) -match 'IP not ready for code generation|^KO\s*$') {
        $process.Kill()
        throw "CubeMX rejected the configuration; inspect $stdout."
    }
    if ([DateTime]::UtcNow -gt $deadline) {
        $process.Kill()
        throw "CubeMX timed out; inspect $stdout and $stderr for invalid configuration or missing packages."
    }
}
$process.WaitForExit()
$log = [IO.File]::ReadAllText($stdout) + [IO.File]::ReadAllText($stderr)
if ($process.ExitCode -ne 0 -or $log -notmatch 'Bye bye' -or
    $log -match '(?m)^KO\s*$|Exception|IP not ready for code generation') {
    throw "CubeMX generation failed; inspect $stdout and $stderr."
}
}

Invoke-CubeProject $ioc

# Retain only the selected MCU/core subset of the copied vendor package.
function Remove-CubeOutput([string]$RelativePath) {
    $target = [IO.Path]::GetFullPath((Join-Path $cubeRoot $RelativePath))
    if (-not $target.StartsWith($cubeRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) { throw "Path outside CubeMX output: $target" }
    for ($attempt = 0; $attempt -lt 5; $attempt++) {
        try {
            if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
            return
        } catch {
            if ($attempt -eq 4) { throw }
            Start-Sleep -Milliseconds 300
        }
    }
}
foreach ($directory in @('Core', 'Core_A', 'DAP', 'Documentation', 'DSP', 'NN', 'RTOS', 'RTOS2')) {
    Remove-CubeOutput "Drivers/CMSIS/$directory"
}
$devicePath = "Drivers/CMSIS/Device/ST/$($boardInfo.cmsis_device)"
Remove-CubeOutput "$devicePath/Source"
$deviceHeaders = @("$($boardInfo.cmsis_device.ToLower()).h", "$($boardInfo.mcu.ToLower()).h", "system_$($boardInfo.cmsis_device.ToLower()).h")
Get-ChildItem (Join-Path $cubeRoot "$devicePath/Include") -File |
    Where-Object { $_.Name -notin $deviceHeaders } |
    ForEach-Object { Remove-CubeOutput "$devicePath/Include/$($_.Name)" }
Get-ChildItem -LiteralPath $cubeRoot -File -Filter '*.ld' |
    ForEach-Object { Remove-CubeOutput $_.Name }
foreach ($file in @('MXTmpFiles', 'Makefile', 'Core/Src/syscalls.c', 'Core/Src/sysmem.c',
    'Core/Src/freertos.c', 'USB_DEVICE/App/usbd_desc.c', 'USB_DEVICE/App/usbd_desc.h',
    'USB_DEVICE/App/usbd_cdc_if.c', 'USB_DEVICE/App/usbd_cdc_if.h',
    'Middlewares/ST/STM32_USB_Device_Library/Class')) {
    Remove-CubeOutput $file
}

# CubeMX's full-library copy also includes unused bus drivers. Keep disabled
# two-wire drivers out of the board output so regeneration cannot restore them.
$halPrefix = $boardInfo.cmsis_device.ToLower()
$halConfig = [IO.File]::ReadAllText((Join-Path $cubeRoot "Core/Inc/${halPrefix}_hal_conf.h"))
$halDirectory = "Drivers/$($boardInfo.cmsis_device)_HAL_Driver"
foreach ($module in @('i2c', 'fmpi2c')) {
    if ($halConfig -notmatch "(?m)^\s*#define\s+HAL_$($module.ToUpper())_MODULE_ENABLED\b") {
        Get-ChildItem -LiteralPath (Join-Path $cubeRoot $halDirectory) -Recurse -File |
            Where-Object { $_.Name -match "^${halPrefix}_(hal|ll)_${module}(_ex)?\.(c|h)$" } |
            ForEach-Object { Remove-CubeOutput $_.FullName.Substring($cubeRoot.Length + 1) }
    }
}

# Remove only empty directories, including directories left by older generations.
Get-ChildItem -LiteralPath $boardRoot -Recurse -Directory -Force |
    Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
        if (-not (Get-ChildItem -LiteralPath $_.FullName -Force | Select-Object -First 1)) {
            Remove-Item -LiteralPath $_.FullName -Force
        }
    }

& $python -X utf8 (Join-Path $PSScriptRoot 'check-cubemx.py') --board $Board
if ($LASTEXITCODE -ne 0) { throw 'Generated board configuration failed validation.' }
Write-Host 'CubeMX generation complete. Build with: cmake --build build --target firmware tests'
