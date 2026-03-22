param(
    [string]$VersionFile = "version.txt",
    [string]$CMakeFile = "CMakeLists.txt",
    [string]$HeaderFile = "include/dxbridge/dxbridge_version.h",
    [string]$Preset,
    [string]$Configuration,
    [string]$BinaryPath,
    [switch]$SkipBinaryCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-TrimmedFile {
    param([string]$Path)
    return (Get-Content -Path $Path -Raw).Trim()
}

function Get-VersionFromCMake {
    param([string]$Path)

    $content = Get-Content -Path $Path -Raw
    $match = [regex]::Match($content, 'project\(dxbridge VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)')
    if (-not $match.Success) {
        throw "Could not parse project version from $Path"
    }

    return $match.Groups[1].Value
}

function Get-VersionFromHeader {
    param([string]$Path)

    $content = Get-Content -Path $Path -Raw
    $major = [regex]::Match($content, '#define DXBRIDGE_VERSION_MAJOR\s+([0-9]+)')
    $minor = [regex]::Match($content, '#define DXBRIDGE_VERSION_MINOR\s+([0-9]+)')
    $patch = [regex]::Match($content, '#define DXBRIDGE_VERSION_PATCH\s+([0-9]+)')

    if (-not ($major.Success -and $minor.Success -and $patch.Success)) {
        throw "Could not parse version macros from $Path"
    }

    return "$($major.Groups[1].Value).$($minor.Groups[1].Value).$($patch.Groups[1].Value)"
}

function Resolve-BinaryPath {
    param(
        [string]$PresetName,
        [string]$ConfigName,
        [string]$ExplicitPath
    )

    if ($ExplicitPath) {
        return $ExplicitPath
    }

    if (-not $PresetName) {
        return $null
    }

    $resolvedConfig = $ConfigName
    if (-not $resolvedConfig) {
        switch ($PresetName) {
            "debug" { $resolvedConfig = "Debug" }
            default { $resolvedConfig = "Release" }
        }
    }

    return Join-Path -Path (Join-Path -Path "out/build" -ChildPath $PresetName) -ChildPath "$resolvedConfig/dxbridge.dll"
}

function Get-DllVersion {
    param([string]$Path)

    $typeDefinition = @"
using System;
using System.Runtime.InteropServices;

public static class DXBridgeVersionProbe
{
    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr LoadLibrary(string lpFileName);

    [DllImport("kernel32", SetLastError = true)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string procName);

    [DllImport("kernel32", SetLastError = true)]
    public static extern bool FreeLibrary(IntPtr hModule);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate UInt32 GetVersionDelegate();
}
"@

    if (-not ([System.Management.Automation.PSTypeName]'DXBridgeVersionProbe').Type) {
        Add-Type -TypeDefinition $typeDefinition | Out-Null
    }

    $module = [DXBridgeVersionProbe]::LoadLibrary($Path)
    if ($module -eq [IntPtr]::Zero) {
        $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "LoadLibrary failed for $Path (Win32 error $code)"
    }

    try {
        $proc = [DXBridgeVersionProbe]::GetProcAddress($module, "DXBridge_GetVersion")
        if ($proc -eq [IntPtr]::Zero) {
            throw "DXBridge_GetVersion was not exported by $Path"
        }

        $delegate = [Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer(
            $proc,
            [type][DXBridgeVersionProbe+GetVersionDelegate]
        )
        $packed = $delegate.Invoke()
        $major = ($packed -shr 16) -band 0xFFFF
        $minor = ($packed -shr 8) -band 0xFF
        $patch = $packed -band 0xFF
        return "$major.$minor.$patch"
    }
    finally {
        [void][DXBridgeVersionProbe]::FreeLibrary($module)
    }
}

$versionFromFile = Read-TrimmedFile -Path $VersionFile
$versionFromCMake = Get-VersionFromCMake -Path $CMakeFile
$versionFromHeader = Get-VersionFromHeader -Path $HeaderFile

Write-Host "version.txt: $versionFromFile"
Write-Host "CMakeLists.txt: $versionFromCMake"
Write-Host "dxbridge_version.h: $versionFromHeader"

if ($versionFromFile -ne $versionFromCMake -or $versionFromFile -ne $versionFromHeader) {
    throw "Version metadata is out of sync."
}

if ($SkipBinaryCheck) {
    Write-Host "Binary check skipped."
    exit 0
}

$resolvedBinaryPath = Resolve-BinaryPath -PresetName $Preset -ConfigName $Configuration -ExplicitPath $BinaryPath
if (-not $resolvedBinaryPath) {
    Write-Host "No binary path or preset provided; metadata check only."
    exit 0
}

$fullBinaryPath = (Resolve-Path -Path $resolvedBinaryPath).Path
$binaryVersion = Get-DllVersion -Path $fullBinaryPath
Write-Host "dxbridge.dll: $binaryVersion ($fullBinaryPath)"

if ($binaryVersion -ne $versionFromFile) {
    throw "Binary version $binaryVersion does not match expected version $versionFromFile"
}
