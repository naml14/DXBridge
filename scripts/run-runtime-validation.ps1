param(
    [string[]]$Runtime = @('nodejs', 'python', 'go', 'rust'),
    [ValidateSet('all', 'dx11', 'dx12')]
    [string]$Backend = 'all',
    [ValidateSet('', 'dx11', 'dx12')]
    [string]$CompareActive = 'dx11',
    [string]$Dll = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$launcher = Join-Path $repoRoot 'examples/run_capability_preflight.ps1'

foreach ($runtime in $Runtime) {
    Write-Host ''
    Write-Host "==> Runtime capability validation: $runtime"

    $arguments = @(
        '-File', $launcher,
        '-Runtime', $runtime,
        '-Backend', $Backend
    )

    if ($CompareActive -ne '') {
        $arguments += @('-CompareActive', $CompareActive)
    }

    if ($Dll -ne '') {
        $arguments += @('-Dll', $Dll)
    }

    & pwsh -NoProfile @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Runtime capability validation failed for $runtime"
    }
}
