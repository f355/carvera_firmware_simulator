# This file is part of the Carvera Firmware Simulator.
#
# Copyright (c) 2026 Konstantin Tcepliaev <f355@f355.org>.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

[CmdletBinding()]
param(
    [string]$RootDir = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$CommandArguments
    )

    & $Executable @CommandArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Executable $($CommandArguments -join ' ')"
    }
}

function Get-UcrtRuntimeDlls {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$UcrtBin,
        [Parameter(Mandatory = $true)][string]$Objdump
    )

    $pending = [System.Collections.Generic.Queue[string]]::new()
    $pending.Enqueue($Executable)
    $seen = @{}
    $runtimeDlls = [System.Collections.Generic.List[string]]::new()

    while ($pending.Count -gt 0) {
        $current = $pending.Dequeue()
        $output = & $Objdump -p $current
        if ($LASTEXITCODE -ne 0) {
            throw "objdump failed while inspecting $current"
        }

        foreach ($line in $output) {
            if ($line -notmatch "DLL Name:\s*(\S+)\s*$") {
                continue
            }
            $name = $Matches[1]
            if ($seen.ContainsKey($name)) {
                continue
            }
            $candidate = Join-Path $UcrtBin $name
            if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
                continue
            }
            $seen[$name] = $true
            $runtimeDlls.Add($candidate)
            $pending.Enqueue($candidate)
        }
    }

    return $runtimeDlls.ToArray()
}

if (-not $RootDir) {
    $RootDir = Split-Path -Parent $PSScriptRoot
}
$RootDir = (Resolve-Path -LiteralPath $RootDir).Path
if (-not [Environment]::Is64BitOperatingSystem) {
    throw "The Windows desktop application must be built on x64 Windows"
}

$AppName = "Carvera Simulator"
$AppConfig = Join-Path $RootDir "packaging\windows\$AppName.exe.config"
$WebView2Version = "150.0.4078.48"
$WebView2ArchiveName = "Microsoft.WebView2.FixedVersionRuntime.150.0.4078.48.x64.cab"
$WebView2Url = "https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/files/60926d99-f201-46bb-91a0-d868dc06b275/$WebView2ArchiveName"
$WebView2Sha256 = "9e347ba96d031e381d1041d1c20fd434d457875c422eeac3f40eee4a5e0ab5c0"
$BuildDir = if ($env:CARVERA_SIM_PACKAGE_BUILD_DIR) {
    $env:CARVERA_SIM_PACKAGE_BUILD_DIR
} else {
    Join-Path $RootDir "build"
}
$OutputDir = if ($env:CARVERA_SIM_PACKAGE_OUTPUT_DIR) {
    $env:CARVERA_SIM_PACKAGE_OUTPUT_DIR
} else {
    Join-Path $RootDir "dist\windows"
}
$PackageWorkDir = if ($env:CARVERA_SIM_PACKAGE_WORK_DIR) {
    $env:CARVERA_SIM_PACKAGE_WORK_DIR
} else {
    Join-Path $BuildDir "package"
}
$MsysRoot = if ($env:CARVERA_SIM_MSYS2_ROOT) { $env:CARVERA_SIM_MSYS2_ROOT } else { "C:\msys64" }
$UcrtBin = Join-Path $MsysRoot "ucrt64\bin"
$Cmake = Join-Path $UcrtBin "cmake.exe"
$Ninja = Join-Path $UcrtBin "ninja.exe"
$Objdump = Join-Path $UcrtBin "objdump.exe"
$Bash = Join-Path $MsysRoot "usr\bin\bash.exe"
$Cygpath = Join-Path $MsysRoot "usr\bin\cygpath.exe"

foreach ($tool in @($Cmake, $Ninja, $Objdump, $Bash, $Cygpath)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required MSYS2 UCRT64 tool is missing: $tool"
    }
}
$Uv = (Get-Command uv.exe -ErrorAction Stop).Source
$env:Path = "$UcrtBin;$env:Path"

$FirmwareRoot = $env:CARVERA_FIRMWARE_ROOT
if (-not $FirmwareRoot) {
    $FirmwareRoot = Join-Path $RootDir "firmware\Carvera_Community_Firmware"
}
if (-not (Test-Path -LiteralPath (Join-Path $FirmwareRoot "src\main.cpp") -PathType Leaf)) {
    $PosixRoot = (& $Cygpath -u $RootDir).Trim()
    Invoke-NativeCommand $Bash @("-lc", "cd '$PosixRoot' && scripts/ensure_firmware_checkout.sh")
}
if (-not (Test-Path -LiteralPath (Join-Path $FirmwareRoot "src\main.cpp") -PathType Leaf)) {
    throw "Firmware checkout is missing: $FirmwareRoot"
}

$env:UV_PROJECT_ENVIRONMENT = Join-Path $BuildDir "python-environment"
$env:UV_LINK_MODE = "copy"
Invoke-NativeCommand $Uv @(
    "sync", "--project", $RootDir, "--locked", "--no-dev", "--group", "package"
)

Invoke-NativeCommand $Cmake @(
    "-S", $RootDir,
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCARVERA_SIM_ENABLE_COVERAGE=OFF",
    "-DCMAKE_MAKE_PROGRAM=$Ninja",
    "-DCARVERA_FIRMWARE_ROOT=$FirmwareRoot"
)
Invoke-NativeCommand $Cmake @(
    "--build", $BuildDir, "--target", "carvera_sim_stream_stdio", "--parallel", $env:NUMBER_OF_PROCESSORS
)

$SimulatorBinary = Join-Path $BuildDir "carvera_sim_stream_stdio.exe"
if (-not (Test-Path -LiteralPath $SimulatorBinary -PathType Leaf)) {
    throw "Native simulator binary is missing: $SimulatorBinary"
}

$DownloadDir = Join-Path $BuildDir "downloads"
$WebView2Archive = Join-Path $DownloadDir $WebView2ArchiveName
New-Item -ItemType Directory -Force -Path $DownloadDir | Out-Null
if (Test-Path -LiteralPath $WebView2Archive -PathType Leaf) {
    $ArchiveHash = (Get-FileHash -LiteralPath $WebView2Archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($ArchiveHash -ne $WebView2Sha256) {
        Remove-Item -LiteralPath $WebView2Archive -Force
    }
}
if (-not (Test-Path -LiteralPath $WebView2Archive -PathType Leaf)) {
    $ProgressPreference = "SilentlyContinue"
    Invoke-WebRequest -Uri $WebView2Url -OutFile $WebView2Archive -UseBasicParsing
}
$ArchiveHash = (Get-FileHash -LiteralPath $WebView2Archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($ArchiveHash -ne $WebView2Sha256) {
    throw "WebView2 archive checksum mismatch: expected $WebView2Sha256, got $ArchiveHash"
}

$PyInstallerDist = Join-Path $PackageWorkDir "pyinstaller-dist"
$PyInstallerWork = Join-Path $PackageWorkDir "pyinstaller-work"
$SpecDir = Join-Path $PackageWorkDir "spec"
$RuntimeBin = Join-Path $PackageWorkDir "runtime\bin"
$WebView2ExtractRoot = Join-Path $PackageWorkDir "webview2-extract"
if (Test-Path -LiteralPath $PackageWorkDir) {
    Remove-Item -LiteralPath $PackageWorkDir -Recurse -Force
}
if (Test-Path -LiteralPath $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Force `
    -Path $PyInstallerDist, $PyInstallerWork, $SpecDir, $RuntimeBin, $WebView2ExtractRoot, $OutputDir | Out-Null

$Expand = Join-Path $env:SystemRoot "System32\expand.exe"
& $Expand $WebView2Archive "-F:*" $WebView2ExtractRoot | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "expand.exe failed with exit code $LASTEXITCODE"
}
$WebView2Runtime = Join-Path $WebView2ExtractRoot "Microsoft.WebView2.FixedVersionRuntime.$WebView2Version.x64"
if (-not (Test-Path -LiteralPath (Join-Path $WebView2Runtime "msedgewebview2.exe") -PathType Leaf)) {
    throw "Expanded WebView2 runtime is missing msedgewebview2.exe: $WebView2Runtime"
}

Copy-Item -LiteralPath $SimulatorBinary -Destination $RuntimeBin
foreach ($dll in Get-UcrtRuntimeDlls -Executable $SimulatorBinary -UcrtBin $UcrtBin -Objdump $Objdump) {
    Copy-Item -LiteralPath $dll -Destination $RuntimeBin
}

$PyInstallerArguments = [System.Collections.Generic.List[string]]::new()
foreach ($argument in @(
    "--name", $AppName,
    "--windowed",
    "--onedir",
    "--noconfirm",
    "--clean",
    "--hidden-import", "webview.platforms.edgechromium",
    "--runtime-hook", (Join-Path $RootDir "scripts\pyinstaller_windows_runtime.py"),
    "--paths", $RootDir,
    "--add-data", "$(Join-Path $RootDir 'machine_models');machine_models",
    "--add-data", "$(Join-Path $RootDir 'default_sdcard');default_sdcard",
    "--add-data", "$WebView2Runtime;webview2",
    "--distpath", $PyInstallerDist,
    "--workpath", $PyInstallerWork,
    "--specpath", $SpecDir
)) {
    $PyInstallerArguments.Add($argument)
}
foreach ($runtimeFile in Get-ChildItem -LiteralPath $RuntimeBin -File | Sort-Object Name) {
    $PyInstallerArguments.Add("--add-binary")
    $PyInstallerArguments.Add("$($runtimeFile.FullName);bin")
}
$PyInstallerArguments.Add((Join-Path $RootDir "gui\desktop.py"))

$UvArguments = @("run", "--project", $RootDir, "--no-sync", "python", "-m", "PyInstaller") + $PyInstallerArguments
Invoke-NativeCommand $Uv $UvArguments

$PyInstallerApp = Join-Path $PyInstallerDist $AppName
$DesktopExecutable = Join-Path $PyInstallerApp "$AppName.exe"
if (-not (Test-Path -LiteralPath $DesktopExecutable -PathType Leaf)) {
    throw "PyInstaller did not create $DesktopExecutable"
}
if (-not (Test-Path -LiteralPath $AppConfig -PathType Leaf)) {
    throw "Windows application configuration is missing: $AppConfig"
}
Copy-Item -LiteralPath $AppConfig -Destination "$DesktopExecutable.config"
$PackagedBackend = @(Get-ChildItem -LiteralPath $PyInstallerApp -Recurse -File -Filter "carvera_sim_stream_stdio.exe")
if ($PackagedBackend.Count -ne 1) {
    throw "Expected exactly one packaged simulator backend, found $($PackagedBackend.Count)"
}
$PackagedBin = $PackagedBackend[0].Directory.FullName
foreach ($runtimeFile in Get-ChildItem -LiteralPath $RuntimeBin -File) {
    $packagedFile = Join-Path $PackagedBin $runtimeFile.Name
    if (-not (Test-Path -LiteralPath $packagedFile -PathType Leaf)) {
        throw "Packaged runtime dependency is missing: $packagedFile"
    }
}
foreach ($resourceDirectory in @("machine_models", "default_sdcard", "webview2")) {
    $packagedResource = @(
        Get-ChildItem -LiteralPath $PyInstallerApp -Recurse -Directory -Filter $resourceDirectory
    )
    if ($packagedResource.Count -eq 0) {
        throw "Packaged resource directory is missing: $resourceDirectory"
    }
}
$PackagedWebView2 = @(Get-ChildItem -LiteralPath $PyInstallerApp -Recurse -File -Filter "msedgewebview2.exe")
if ($PackagedWebView2.Count -ne 1) {
    throw "Expected exactly one packaged WebView2 runtime, found $($PackagedWebView2.Count)"
}

$FinalApp = Join-Path $OutputDir $AppName
New-Item -ItemType Directory -Path $FinalApp | Out-Null
Copy-Item -Path (Join-Path $PyInstallerApp "*") -Destination $FinalApp -Recurse -Force
$FinalWebView2 = @(Get-ChildItem -LiteralPath $FinalApp -Recurse -File -Filter "msedgewebview2.exe")
if ($FinalWebView2.Count -ne 1) {
    throw "Expected exactly one final WebView2 runtime, found $($FinalWebView2.Count)"
}
$FinalWebView2Runtime = $FinalWebView2[0].Directory.FullName
$Icacls = Join-Path $env:SystemRoot "System32\icacls.exe"
foreach ($sid in @("*S-1-15-2-2:(OI)(CI)(RX)", "*S-1-15-2-1:(OI)(CI)(RX)")) {
    Invoke-NativeCommand $Icacls @($FinalWebView2Runtime, "/grant", $sid)
}
Write-Output "Created $FinalApp"
