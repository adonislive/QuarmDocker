@echo off
cd /d "%~dp0\..\..\"

docker inspect quarm-server >nul 2>&1
if %errorlevel% neq 0 (
    echo Server is not running. Please start the server before restoring.
    pause
    exit /b
)

echo WARNING: This will overwrite all current characters and accounts.
echo.
set /p confirm=Are you sure you want to restore? (Y/N): 
if /i not "%confirm%"=="Y" (
    echo Restore cancelled.
    pause
    exit /b
)

echo.
for /f "tokens=*" %%F in ('dir /b /o-d config\backups\backup_*.sql 2^>nul') do (
    set newest=%%F
    goto found
)

echo No backups found.
pause
exit /b

:found
echo Most recent backup: %newest%
echo.
set /p choice=Restore this backup? (Y/N): 
if /i "%choice%"=="Y" (
    docker exec -i quarm-server mariadb quarm < config\backups\%newest%
    echo Done. Restored from config\backups\%newest%
    pause
    exit /b
)

echo.
echo Available backups:
echo.
dir /b /o-d config\backups\backup_*.sql
echo.
set /p filename=Enter filename to restore (e.g. backup_2026-02-23_1430.sql): 
if not exist "config\backups\%filename%" (
    echo ERROR: File not found. Please check the filename and try again.
    pause
    exit /b
)
docker exec -i quarm-server mariadb quarm < config\backups\%filename%
echo Done. Restored from config\backups\%filename%
pause
