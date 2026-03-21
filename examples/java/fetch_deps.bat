@echo off
setlocal
pushd "%~dp0"

set "LIB_DIR=%CD%\lib"
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"

call :download "https://repo1.maven.org/maven2/net/java/dev/jna/jna/5.14.0/jna-5.14.0.jar" "%LIB_DIR%\jna-5.14.0.jar"
if errorlevel 1 goto :fail
call :download "https://repo1.maven.org/maven2/net/java/dev/jna/jna-platform/5.14.0/jna-platform-5.14.0.jar" "%LIB_DIR%\jna-platform-5.14.0.jar"
if errorlevel 1 goto :fail

echo Dependencies ready in "%LIB_DIR%".
popd
exit /b 0

:download
set "URL=%~1"
set "TARGET=%~2"
if exist "%TARGET%" (
    echo Already present: "%TARGET%"
    exit /b 0
)

echo Downloading "%TARGET%"...
powershell -NoProfile -ExecutionPolicy Bypass -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri '%URL%' -OutFile '%TARGET%'"
if errorlevel 1 exit /b 1
exit /b 0

:fail
echo Failed to download dependencies.
popd
exit /b 1
