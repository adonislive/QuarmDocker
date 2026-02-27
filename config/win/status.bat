@echo off
cd /d "%~dp0\..\..\"

echo ============================================================
echo  Quarm Server Status
echo ============================================================
echo.

REM --- Container state ---
docker inspect quarm-server >nul 2>&1
if %errorlevel% neq 0 (
    echo  Status:  STOPPED  (container not found)
    echo.
    goto show_network
)

for /f %%S in ('docker inspect -f "{{.State.Status}}" quarm-server 2^>nul') do set cstate=%%S
if /i "%cstate%"=="running" (
    echo  Status:  RUNNING
) else (
    echo  Status:  %cstate%
    echo.
    goto show_network
)

REM --- Uptime ---
for /f %%T in ('docker inspect -f "{{.State.StartedAt}}" quarm-server 2^>nul') do set started=%%T
for /f %%U in ('powershell -NoProfile -Command "$s=[datetime]::Parse('%started%').ToLocalTime(); $d=(Get-Date)-$s; if($d.TotalHours -ge 1){'{0}h {1}m' -f [int]$d.TotalHours,$d.Minutes}else{'{0}m' -f $d.Minutes}" 2^>nul') do set uptime=%%U
if not "%uptime%"=="" (
    echo  Uptime:  %uptime%
)
echo.

REM --- Process list ---
echo  Services:
echo.
for /f %%P in ('docker exec quarm-server ps -eo comm 2^>nul') do (
    set pname=%%P
    if "%%P"=="mariadbd"    echo    Database           [running]
    if "%%P"=="loginserver" echo    Login Server       [running]
    if "%%P"=="world"       echo    World Server       [running]
    if "%%P"=="eqlaunch"    echo    Zone Launcher      [running]
    if "%%P"=="queryserv"   echo    Query Server       [running]
    if "%%P"=="ucs"         echo    Chat Server        [running]
)

REM --- Zone process count ---
for /f %%Z in ('docker exec quarm-server ps -eo comm 2^>nul ^| find /c "zone"') do set zonecount=%%Z
if not "%zonecount%"=="0" (
    echo    Zone Processes     [%zonecount% running]
)
echo.

:show_network
REM --- Network setting ---
if exist .env (
    for /f "tokens=2 delims==" %%A in ('findstr /i "SERVER_ADDRESS" .env 2^>nul') do set serverip=%%A
)
if not "%serverip%"=="" (
    echo  Network: %serverip%
) else (
    echo  Network: 127.0.0.1 (local only)
)
echo.
echo ============================================================
