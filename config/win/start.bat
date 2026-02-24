@echo off
cd /d "%~dp0\..\..\"
echo Starting Quarm server...
docker compose up -d
echo Done.
pause
