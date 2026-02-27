@echo off
cd /d "%~dp0\..\..\"

echo ============================================================
echo  Quarm Server - Rebuild
echo ============================================================
echo.
echo  This will rebuild the server image from scratch.
echo.
echo  YOUR CHARACTER DATA WILL BE PRESERVED.
echo  The Docker volume quarm-data is not affected by a rebuild.
echo.
echo  The server image will be deleted and recompiled.
echo  This takes 30-45 minutes.
echo.
echo  The server will be stopped and a backup taken first.
echo ============================================================
echo.
set confirm=
set /p confirm=Type Y and press Enter to continue, or just Enter to cancel: 
if /i not "%confirm%"=="Y" (
    echo Rebuild cancelled.
    pause
    exit /b
)

echo.
echo Checking server state...
docker inspect quarm-server >nul 2>&1
if %errorlevel% equ 0 (
    echo Server is running. Taking backup before rebuild...
    if not exist config\backups mkdir config\backups
    for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HHmm"') do set datestamp=%%I
    docker exec quarm-server mariadb-dump quarm > config\backups\backup_%datestamp%.sql
    for %%A in (config\backups\backup_%datestamp%.sql) do set filesize=%%~zA
    if %filesize% LSS 100 (
        echo.
        echo ERROR: Backup failed. Aborting rebuild to protect your data.
        echo Check that the server is running correctly and try again.
        pause
        exit /b
    )
    echo Backup saved: config\backups\backup_%datestamp%.sql (%filesize% bytes)
    echo.
    echo Stopping server...
    docker compose down
    if %errorlevel% neq 0 (
        echo ERROR: Failed to stop server. Aborting.
        pause
        exit /b
    )
) else (
    echo Server is not running. No backup needed.
    echo.
    echo Cleaning up any stopped containers...
    docker compose down >nul 2>&1
)

echo.
echo ============================================================
echo  Building server image. This will take 30-45 minutes.
echo  Do not close this window.
echo ============================================================
echo.
docker compose build
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed. Check the output above for details.
    echo Your character data is safe in the quarm-data volume.
    echo.
    echo You can try running rebuild.bat again. Docker will resume
    echo from where it stopped thanks to its build cache.
    pause
    exit /b
)

echo.
echo Build complete. Starting server...
docker compose up -d
if %errorlevel% neq 0 (
    echo ERROR: Failed to start server after rebuild.
    pause
    exit /b
)

echo.
echo ============================================================
echo  Rebuild complete. Server is running.
echo ============================================================
echo.
pause
