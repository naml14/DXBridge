@echo off
setlocal
pushd "%~dp0"

call fetch_deps.bat
if errorlevel 1 goto :fail

set "OUT_DIR=%CD%\out"
set "CLASS_DIR=%OUT_DIR%\classes"
set "LIB_CP=%CD%\lib\jna-5.14.0.jar;%CD%\lib\jna-platform-5.14.0.jar"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if exist "%CLASS_DIR%" rmdir /s /q "%CLASS_DIR%"
mkdir "%CLASS_DIR%"

dir /s /b src\*.java > "%OUT_DIR%\sources.list"
javac -encoding UTF-8 -source 1.8 -target 1.8 -cp "%LIB_CP%" -d "%CLASS_DIR%" @"%OUT_DIR%\sources.list"
if errorlevel 1 goto :fail

echo Build completed: "%CLASS_DIR%"
popd
exit /b 0

:fail
echo Build failed.
popd
exit /b 1
