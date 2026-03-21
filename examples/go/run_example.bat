@echo off
setlocal

if "%~1"=="" (
  echo Usage: run_example.bat example01_load_version_logs [args...]
  exit /b 1
)

set EXAMPLE=%~1
shift
go run .\cmd\%EXAMPLE% %*
