@echo off
setlocal
if "%~1"=="" (
  echo Usage: run_example.bat example01_load_version_logs [args...]
  exit /b 1
)

set BIN=%~1
shift
cargo run --manifest-path "%~dp0Cargo.toml" --bin %BIN% -- %*
