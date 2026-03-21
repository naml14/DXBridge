@echo off
setlocal
if "%~1"=="" (
  echo Usage: run_example.bat Example01LoadVersionLogs [args...]
  exit /b 1
)

dotnet run --project "%~dp0DXBridgeExamples.csproj" -- %*
