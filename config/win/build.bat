@echo off
REM ============================================================
REM  Build QuarmDockerServer.exe
REM  Requires: Visual Studio with C++ workload
REM  Run from: x64 Native Tools Command Prompt for VS
REM ============================================================

echo Building QuarmDockerServer.exe (x64)...
echo.

cl /O2 /W3 /EHsc /std:c++17 ^
   /DUNICODE /D_UNICODE ^
   /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 ^
   QuarmDockerServer.cpp ^
   /Fe:QuarmDockerServer.exe ^
   /link ^
   /SUBSYSTEM:WINDOWS ^
   /MACHINE:X64 ^
   user32.lib ^
   gdi32.lib ^
   comctl32.lib ^
   shell32.lib ^
   shlwapi.lib ^
   comdlg32.lib ^
   advapi32.lib

if %errorlevel% neq 0 (
    echo.
    echo BUILD FAILED.
    echo.
    echo Make sure you are running from the
    echo "x64 Native Tools Command Prompt for VS"
    echo and that the Visual Studio C++ workload is installed.
    pause
    exit /b 1
)

echo.
echo BUILD SUCCEEDED: QuarmDockerServer.exe
echo.
echo Copy QuarmDockerServer.exe to your install directory
echo (e.g. C:\QuarmDocker\) alongside docker-compose.yml.
echo.
pause
