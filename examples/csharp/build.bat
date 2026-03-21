@echo off
setlocal
dotnet build "%~dp0DXBridgeExamples.csproj" %*
