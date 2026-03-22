@echo off
setlocal

if "%~1"=="" (
  echo Usage: run_example.bat example01_load_version_logs [args...]
  echo Aliases: example05, example06, preflight
  exit /b 1
)

set EXAMPLE=%~1
shift
if /I "%EXAMPLE%"=="example05" set EXAMPLE=example05_dx11_moving_triangle
if /I "%EXAMPLE%"=="example06" set EXAMPLE=example06_capability_preflight
if /I "%EXAMPLE%"=="preflight" set EXAMPLE=example06_capability_preflight
go run .\cmd\%EXAMPLE% %*
