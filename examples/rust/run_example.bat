@echo off
setlocal
if "%~1"=="" (
  echo Usage: run_example.bat example01_load_version_logs [args...]
  echo Aliases: example05, example06, preflight
  exit /b 1
)

set BIN=%~1
shift
if /I "%BIN%"=="example05" set BIN=example05_dx11_moving_triangle
if /I "%BIN%"=="example06" set BIN=example06_capability_preflight
if /I "%BIN%"=="preflight" set BIN=example06_capability_preflight
cargo run --manifest-path "%~dp0Cargo.toml" --bin %BIN% -- %*
