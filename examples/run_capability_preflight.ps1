param(
    [ValidateSet('all', 'bun', 'nodejs', 'python', 'go', 'rust')]
    [string]$Runtime = 'all',
    [ValidateSet('all', 'dx11', 'dx12')]
    [string]$Backend = 'all',
    [string]$CompareActive = '',
    [string]$Dll = ''
)

$ErrorActionPreference = 'Stop'

if ($CompareActive -ne '' -and $CompareActive -notin @('dx11', 'dx12')) {
    throw "-CompareActive must be dx11 or dx12."
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

if ($Dll -ne '') {
    $Dll = (Resolve-Path -Path $Dll).Path
}

function Test-Tool {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Get-ExampleArgs {
    param([switch]$NeedsSeparator)

    $args = @()
    if ($NeedsSeparator -and ($Backend -ne 'all' -or $CompareActive -ne '' -or $Dll -ne '')) {
        $args += '--'
    }
    if ($Backend -ne 'all') {
        $args += @('--backend', $Backend)
    }
    if ($CompareActive -ne '') {
        $args += @('--compare-active', $CompareActive)
    }
    if ($Dll -ne '') {
        $args += @('--dll', $Dll)
    }
    return $args
}

function Invoke-Example {
    param(
        [string]$Title,
        [string]$Command,
        [string[]]$Arguments,
        [string]$WorkingDirectory = $repoRoot
    )

    Write-Host ''
    Write-Host "==> $Title"
    Push-Location $WorkingDirectory
    try {
        & $Command @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Title failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }
}

$targets = if ($Runtime -eq 'all') {
    @('bun', 'nodejs', 'python', 'go', 'rust')
}
else {
    @($Runtime)
}

foreach ($target in $targets) {
    switch ($target) {
        'bun' {
            if (-not (Test-Tool 'bun')) {
                Write-Warning 'Skipping Bun: bun is not on PATH.'
                continue
            }
            Invoke-Example 'Bun capability preflight' 'bun' (@('run', 'examples/bun/example06_capability_preflight.ts') + (Get-ExampleArgs))
        }
        'nodejs' {
            if (-not (Test-Tool 'node')) {
                Write-Warning 'Skipping Node.js: node is not on PATH.'
                continue
            }
            if (-not (Test-Path (Join-Path $repoRoot 'examples/nodejs/node_modules/koffi'))) {
                if (-not (Test-Tool 'npm')) {
                    Write-Warning 'Skipping Node.js: npm is required to install koffi.'
                    continue
                }
                Invoke-Example 'Node.js dependency install' 'npm' @('--prefix', 'examples/nodejs', 'ci')
            }
            Invoke-Example 'Node.js capability preflight' 'node' (@('examples/nodejs/example06_capability_preflight.js') + (Get-ExampleArgs))
        }
        'python' {
            if (-not (Test-Tool 'python')) {
                Write-Warning 'Skipping Python: python is not on PATH.'
                continue
            }
            Invoke-Example 'Python capability preflight' 'python' (@('examples/python/example_06_capability_preflight.py') + (Get-ExampleArgs))
        }
        'go' {
            if (-not (Test-Tool 'go')) {
                Write-Warning 'Skipping Go: go is not on PATH.'
                continue
            }
            Invoke-Example 'Go capability preflight' 'go' (@('run', './cmd/example06_capability_preflight') + (Get-ExampleArgs)) (Join-Path $repoRoot 'examples/go')
        }
        'rust' {
            if (-not (Test-Tool 'cargo')) {
                Write-Warning 'Skipping Rust: cargo is not on PATH.'
                continue
            }
            Invoke-Example 'Rust capability preflight' 'cargo' (@('run', '--manifest-path', 'examples/rust/Cargo.toml', '--bin', 'example06_capability_preflight') + (Get-ExampleArgs -NeedsSeparator))
        }
    }
}
