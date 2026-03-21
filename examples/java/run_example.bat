@echo off
setlocal
pushd "%~dp0"

if "%~1"=="" (
    echo Usage: run_example.bat Example01LoadVersionLogs [args...]
    echo.
    echo Available examples:
    echo   Example01LoadVersionLogs
    echo   Example02EnumerateAdapters
    echo   Example03CreateDeviceErrors
    echo   Example04Dx11ClearWindow
    echo   Example05Dx11MovingTriangle
    popd
    exit /b 1
)

set "MAIN_CLASS=examples.%~1"

call build.bat
if errorlevel 1 goto :fail

set "CP=%CD%\out\classes;%CD%\lib\jna-5.14.0.jar;%CD%\lib\jna-platform-5.14.0.jar"
java -cp "%CP%" "%MAIN_CLASS%" %2 %3 %4 %5 %6 %7 %8 %9
set "EXIT_CODE=%ERRORLEVEL%"

popd
exit /b %EXIT_CODE%

:fail
echo Could not run example.
popd
exit /b 1
