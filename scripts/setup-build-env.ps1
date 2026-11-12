[CmdletBinding()]
param(
    [string]$Python = 'py',
    [string]$IndexUrl = 'https://pypi.tuna.tsinghua.edu.cn/simple'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$tools = Join-Path $repo '.tools'
$downloads = Join-Path $tools 'downloads'
New-Item -ItemType Directory -Force $downloads | Out-Null

$cmake = (Get-Command cmake -ErrorAction Stop).Source
$cmakeVersion = & $cmake --version
if ($LASTEXITCODE -ne 0) { throw '无法读取 CMake 版本。' }
$versionMatch = [regex]::Match($cmakeVersion[0], 'version (\d+\.\d+(?:\.\d+)?)')
if (-not $versionMatch.Success) { throw '无法读取 CMake 版本。' }
if ([version]$versionMatch.Groups[1].Value -lt [version]'3.20') { throw '需要 CMake 3.20 或更新版本。' }
$ninja = (Get-Command ninja -ErrorAction Stop).Source
& $ninja --version
if ($LASTEXITCODE -ne 0) { throw 'Ninja 检查失败。' }

function Install-PortableTool {
    param($Url, $ArchiveName, $Sha256, $Directory, $Executable)
    $archive = Join-Path $downloads $ArchiveName
    if (-not (Test-Path $archive) -or (Get-FileHash $archive -Algorithm SHA256).Hash -ne $Sha256) {
        Write-Host "Downloading $ArchiveName ..."
        & curl.exe --ssl-revoke-best-effort --fail --location --retry 3 --connect-timeout 20 --max-time 900 --output $archive $Url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $Url" }
    }
    if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne $Sha256) {
        throw "SHA256 mismatch: $archive"
    }
    $destination = Join-Path $tools $Directory
    if (-not (Test-Path (Join-Path $destination $Executable))) {
        Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
    }
    if (-not (Test-Path (Join-Path $destination $Executable))) {
        throw "Tool executable missing after extraction: $Executable"
    }
}

Install-PortableTool `
    'https://developer.arm.com/-/media/Files/downloads/gnu-rm/9-2019q4/gcc-arm-none-eabi-9-2019-q4-major-win32.zip' `
    'gcc-arm-none-eabi-9-2019-q4-major-win32.zip' `
    'E4C964ADD8D0FDCC6B14F323E277A0946456082A84A1CC560DA265B357762B62' `
    'gcc-arm-none-eabi-9-2019-q4-major' 'bin/arm-none-eabi-gcc.exe'
$venvPython = Join-Path $repo '.venv/Scripts/python.exe'
if (-not (Test-Path $venvPython)) {
    if ($Python -eq 'py') {
        & $Python -3.10 -m venv (Join-Path $repo '.venv')
    } else {
        & $Python -m venv (Join-Path $repo '.venv')
    }
    if ($LASTEXITCODE -ne 0) { throw 'Creating Python environment failed. Install Python 3.10 or pass -Python with its executable path.' }
}
& $venvPython -m pip --disable-pip-version-check install --index-url $IndexUrl -r (Join-Path $PSScriptRoot 'requirements-build.txt')
if ($LASTEXITCODE -ne 0) { throw 'Installing Python build dependencies failed.' }
& (Join-Path $tools 'gcc-arm-none-eabi-9-2019-q4-major/bin/arm-none-eabi-gcc.exe') --version
if ($LASTEXITCODE -ne 0) { throw 'ARM GCC verification failed.' }
Write-Host '构建环境已准备，回到仓库根目录运行 .\build.ps1。'
