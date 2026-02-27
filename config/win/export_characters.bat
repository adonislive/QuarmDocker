@echo off
cd /d "%~dp0\..\..\"

echo ============================================================
echo  Quarm Server - Export Characters
echo ============================================================
echo.
echo  Exports player accounts and character data only.
echo  Output file is prefixed chars_ to distinguish from
echo  full backups. Use this to transfer characters to a
echo  different server version.
echo.
echo  NOTE: Items carried by characters may not transfer if they
echo  no longer exist in the destination server version. Bank or
echo  drop important items before transferring if unsure.
echo ============================================================
echo.

docker inspect quarm-server >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Server is not running.
    echo Please start the server before exporting characters.
    pause
    exit /b
)

if not exist config\backups mkdir config\backups

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HHmm"') do set datestamp=%%I
set outfile=config\backups\chars_%datestamp%.sql

echo Exporting character data...
docker exec quarm-server mariadb-dump --replace --tables quarm ^
    account ^
    tblLoginServerAccounts ^
    character_data ^
    character_inventory ^
    character_currency ^
    character_bind ^
    character_skills ^
    character_spells ^
    character_languages ^
    character_corpses ^
    > %outfile%

if %errorlevel% neq 0 (
    echo ERROR: Export command failed.
    pause
    exit /b
)

for %%A in (%outfile%) do set filesize=%%~zA
if %filesize% LSS 1000 (
    echo WARNING: Export file appears too small - export may have failed.
    echo File: %outfile% (%filesize% bytes)
    pause
    exit /b
)

echo.
echo Done. Saved as %outfile% (%filesize% bytes)
echo.
pause
