param(
    [string]$Preset = "debug",
    [string]$Label,
    [switch]$NoOutputOnFailure,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs
)

function Resolve-CTestCommand {
    $ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
    if ($ctestCommand) {
        return $ctestCommand.Source
    }

    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCommand) {
        $candidate = Join-Path -Path (Split-Path -Path $cmakeCommand.Source -Parent) -ChildPath "ctest.exe"
        if (Test-Path -Path $candidate) {
            return $candidate
        }
    }

    $visualStudioCandidates = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
    )

    foreach ($candidate in $visualStudioCandidates) {
        if (Test-Path -Path $candidate) {
            return $candidate
        }
    }

    throw "Could not locate ctest. Install CMake or add ctest.exe to PATH."
}

$arguments = @("--preset", $Preset)

if ($Label) {
    $arguments += @("-L", $Label)
}

if (-not $NoOutputOnFailure -and -not ($ExtraArgs -contains "--output-on-failure")) {
    $arguments += "--output-on-failure"
}

if ($ExtraArgs) {
    $arguments += $ExtraArgs
}

$ctest = Resolve-CTestCommand
& $ctest @arguments
exit $LASTEXITCODE
