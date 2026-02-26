@echo off
cd /d "%~dp0\..\..\"
docker inspect quarm-server >nul 2>&1
if %errorlevel% neq 0 (
    echo Server is not running. Backup skipped.
    pause
    exit /b
)
if not exist config\backups mkdir config\backups
for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HHmm"') do set datestamp=%%I
docker exec quarm-server mariadb-dump quarm > config\backups\backup_%datestamp%.sql
for %%A in (config\backups\backup_%datestamp%.sql) do set filesize=%%~zA
if %filesize% LSS 100 (
    echo WARNING: Backup file appears empty or failed.
    pause
    exit /b
)
echo Done. Saved as config\backups\backup_%datestamp%.sql (%filesize% bytes).
pause
