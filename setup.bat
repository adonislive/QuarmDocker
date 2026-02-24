@echo off
cd /d "%~dp0"
title Quarm Docker Setup
echo ================================================
echo  QUARM DOCKER SETUP
echo ================================================
echo.

REM ------------------------------------------------
REM STEP 1 - Check WSL2
REM ------------------------------------------------
echo Checking WSL2...
wsl --status >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo ERROR: WSL2 is not installed.
    echo.
    echo WSL2 is required to run Quarm Docker on Windows.
    echo Please install it by opening PowerShell as Administrator and running:
    echo.
    echo   wsl --install
    echo.
    echo Then restart your computer and run this setup again.
    echo.
    echo For full instructions visit:
    echo   https://learn.microsoft.com/en-us/windows/wsl/install
    echo.
    pause
    exit /b
)
echo WSL2 found.
echo.

REM ------------------------------------------------
REM STEP 2 - Check Docker is installed
REM ------------------------------------------------
echo Checking Docker...
docker --version >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Docker is not installed.
    echo.
    echo Please download and install Docker Desktop from:
    echo   https://docs.docker.com/desktop/install/windows-install/
    echo.
    echo Then run this setup again.
    echo.
    pause
    exit /b
)
echo Docker found.
echo.

REM ------------------------------------------------
REM STEP 3 - Check Docker is running
REM ------------------------------------------------
echo Checking Docker is running...
:dockercheck
docker info >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo Docker is not running. Please open Docker Desktop and wait for it to start.
    echo.
    pause
    goto dockercheck
)
echo Docker is running.
echo.

REM ------------------------------------------------
REM STEP 4 - Local or LAN play
REM ------------------------------------------------
echo ------------------------------------------------
echo  NETWORK SETUP
echo ------------------------------------------------
echo.
echo Quarm Docker defaults to local play only.
echo This means only this machine can connect to the server.
echo.
echo If you are not sure, choose local. You can change this later.
echo.
set /p lanChoice=Will only this machine connect to the server? (Y/N): 
if /i not "%lanChoice%"=="N" goto buildstep

echo.
echo Detecting LAN IP address...
set lanip=
for /f "tokens=2 delims=:" %%A in ('ipconfig ^| findstr /i "IPv4" ^| findstr "192.168.1. 192.168.0."') do (
    if not defined lanip set lanip=%%A
)
if defined lanip set lanip=%lanip: =%

if not defined lanip (
    echo.
    echo Could not automatically detect a LAN IP address.
    echo.
)
if not defined lanip set /p lanip=Please enter your LAN IP address manually e.g. 192.168.1.100: 

:ipconfirm
echo.
echo Detected LAN IP: %lanip%
echo.
set /p ipOk=Is this correct? (Y/N): 
if /i not "%ipOk%"=="Y" (
    echo.
    set /p lanip=Please enter your LAN IP address manually e.g. 192.168.1.100: 
)

echo.
echo Updating server address to %lanip%...
echo SERVER_ADDRESS=%lanip%> .env
echo Done.
echo.

REM ------------------------------------------------
REM STEP 5 - Build
REM ------------------------------------------------
:buildstep
echo ------------------------------------------------
echo  BUILD
echo ------------------------------------------------
echo.
echo The server needs to be built from source.
echo This will take 30-45 minutes. Do not close this window.
echo.
set /p buildChoice=Ready to build now? (Y/N): 
if /i not "%buildChoice%"=="Y" (
    echo.
    echo When you are ready, run this command from the QuarmDocker folder:
    echo   docker compose build
    echo.
    goto checklist
)

echo.
echo Starting build. Please wait...
echo.
docker compose build
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed. Check the output above for details.
    echo.
    pause
    exit /b
)
echo.
echo Build complete.
echo.

REM ------------------------------------------------
REM STEP 6 - Start server
REM ------------------------------------------------
echo ------------------------------------------------
echo  START SERVER
echo ------------------------------------------------
echo.
set /p startChoice=Start the server now? (Y/N): 
if /i not "%startChoice%"=="Y" (
    echo.
    echo When you are ready, double-click start.bat in the config\win\ folder.
    echo.
    goto checklist
)

echo.
docker compose up -d
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Failed to start the server. Check the output above for details.
    echo.
    pause
    exit /b
)
echo.
echo Server is running.
echo.

REM ------------------------------------------------
REM STEP 7 - Post install checklist
REM ------------------------------------------------
:checklist
if /i "%lanChoice%"=="N" (
    set eqip=%lanip%
) else (
    set eqip=127.0.0.1
)

echo ================================================
echo  SETUP COMPLETE — NEXT STEPS
echo ================================================
echo.
echo 1. Configure your EQ client
echo    Edit eqhost.txt in your TAKP client folder to contain:
echo.
echo    [Registration Servers]
echo    {
echo    "%eqip%:6000"
echo    }
echo    [Login Servers]
echo    {
echo    "%eqip%:6000"
echo    }
echo.
echo 2. Launch the client and log in
echo    Use any username and password.
echo    Your account is created automatically on first login.
echo.
echo 3. Make yourself a GM
echo    Run this command, replacing YOURNAME with your account name:
echo.
echo    docker exec quarm-server mariadb -e "UPDATE account SET status=255 WHERE name='YOURNAME';" quarm
echo.
echo 4. For future server management, use the scripts in the config\win\ folder:
echo      start.bat  : Start the server
echo      stop.bat   : Stop the server and back up the database
echo      backup.bat : Back up the database while the server is running
echo      restore.bat: Restore the database from a previous backup
echo.
echo ================================================
echo.
pause
