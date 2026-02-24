@echo off
cd /d "%~dp0\..\..\"
docker inspect quarm-server >nul 2>&1
if %errorlevel% neq 0 (
    echo Server is not running. Nothing to stop.
    pause
    exit /b
)
echo Backing up before shutdown...
if not exist config\backups mkdir config\backups
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set dt=%%I
set datestamp=%dt:~0,4%-%dt:~4,2%-%dt:~6,2%_%dt:~8,2%%dt:~10,2%
docker exec quarm-server mariadb-dump quarm > config\backups\backup_%datestamp%.sql
for %%A in (config\backups\backup_%datestamp%.sql) do set filesize=%%~zA
if %filesize% LSS 100 (
    echo WARNING: Backup file appears empty or failed. Aborting shutdown.
    pause
    exit /b
)
echo Backup saved as config\backups\backup_%datestamp%.sql (%filesize% bytes).
echo Stopping server...
docker compose down
echo Done.
pause
