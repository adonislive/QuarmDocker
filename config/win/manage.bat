@echo off
cd /d "%~dp0..\.."
title Quarm Server Manager

:menu
cls
echo ================================================
echo  QUARM SERVER MANAGER
echo ================================================
echo.
echo  -- Account Management --
echo   1.  Make GM
echo   2.  Remove GM
echo   3.  List Accounts
echo   4.  Reset Password
echo.
echo  -- Player Info --
echo   5.  Who Is Online
echo   6.  Show Recent Logins
echo   7.  Show Last Known IP
echo   8.  List Characters on Account
echo   9.  Character Info
echo  10.  Show Inventory
echo  11.  Show Currency
echo  12.  Show Account for Character
echo.
echo  -- Player Actions --
echo  13.  Move Character to Bind Point
echo  14.  Move Character to Zone
echo  15.  Give Platinum
echo.
echo  -- Corpses --
echo  16.  List All Corpses
echo  17.  Show Corpses by Character
echo.
echo  -- Server --
echo  18.  Server Status
echo.
echo   0.  Exit
echo.
set /p choice=Select an option: 
echo.

if "%choice%"=="0"  exit /b
if "%choice%"=="1"  goto make_gm
if "%choice%"=="2"  goto remove_gm
if "%choice%"=="3"  goto list_accounts
if "%choice%"=="4"  goto reset_password
if "%choice%"=="5"  goto who_online
if "%choice%"=="6"  goto show_recent_logins
if "%choice%"=="7"  goto show_ip_history
if "%choice%"=="8"  goto list_characters
if "%choice%"=="9"  goto character_info
if "%choice%"=="10" goto show_inventory
if "%choice%"=="11" goto show_currency
if "%choice%"=="12" goto show_account_for_character
if "%choice%"=="13" goto move_character_to_bind
if "%choice%"=="14" goto move_character_to_zone
if "%choice%"=="15" goto give_platinum
if "%choice%"=="16" goto list_corpses
if "%choice%"=="17" goto show_corpses_by_character
if "%choice%"=="18" goto server_status

echo Invalid selection. Please enter a number from the menu.
echo.
pause
goto menu

REM ------------------------------------------------
REM COMING SOON
REM ------------------------------------------------
:coming_soon
echo This feature is not yet implemented.
echo.
pause
goto menu


REM ================================================
REM  ACCOUNT MANAGEMENT
REM ================================================

:make_gm
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set acct=
set /p acct=Enter account name: 
if "%acct%"=="" (
    echo No account name entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "UPDATE account SET status=255 WHERE LOWER(name)=LOWER('%acct%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The server may be unavailable.
) else (
    echo Done. %acct% is now a GM.
    echo Note: They must log out and back in for the change to take effect.
    echo Note: If no rows changed, the account name may not exist - check List Accounts.
)
echo.
pause
goto menu

:remove_gm
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set acct=
set /p acct=Enter account name: 
if "%acct%"=="" (
    echo No account name entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "UPDATE account SET status=0 WHERE LOWER(name)=LOWER('%acct%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The server may be unavailable.
) else (
    echo Done. GM status removed from %acct%.
    echo Note: They must log out and back in for the change to take effect.
    echo Note: If no rows changed, the account name may not exist - check List Accounts.
)
echo.
pause
goto menu

:list_accounts
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT a.name, a.status, lsa.LastLoginDate, lsa.LastIPAddress FROM account a LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name ORDER BY lsa.LastLoginDate DESC;" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The server may be unavailable.
) else (
    echo.
    echo Note: status 255 = GM, status 0 = normal player.
)
echo.
pause
goto menu

:reset_password
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set acct=
set /p acct=Enter account name: 
if "%acct%"=="" (
    echo No account name entered.
    echo.
    pause
    goto menu
)
REM Use PowerShell to read the password and compute SHA1 hash.
REM This safely handles any special characters in the password.
REM Only the resulting hex hash (0-9a-f, no special chars) is passed to mariadb.
set passhash=
for /f "delims=" %%H in ('powershell -NoProfile -Command "$pass = Read-Host \"Enter new password\"; if ($pass -eq \"\") { exit 1 }; $sha1 = [System.Security.Cryptography.SHA1]::Create(); [BitConverter]::ToString($sha1.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($pass))).Replace(\"-\",\"\").ToLower()"') do set "passhash=%%H"
if "%passhash%"=="" (
    echo No password entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "UPDATE tblLoginServerAccounts SET AccountPassword='%passhash%' WHERE LOWER(AccountName)=LOWER('%acct%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The server may be unavailable.
) else (
    echo Done. Password updated for %acct%.
    echo Note: If no rows changed, the account name may not exist - check List Accounts.
    echo Note: The account must log out and back in for the change to take effect.
)
set passhash=
echo.
pause
goto menu

REM ================================================
REM  PLAYER INFO
REM ================================================

:who_online
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT cd.name, cd.level, cd.class, cd.race, z.short_name AS zone FROM character_data cd LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id WHERE cd.last_login > UNIX_TIMESTAMP(NOW() - INTERVAL 1 DAY) ORDER BY cd.last_login DESC;" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The server may be unavailable.
) else (
    echo.
    echo Note: Shows characters active in the last 24 hours. class and race are numeric IDs.
)
echo.
pause
goto menu

:show_recent_logins
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT a.name, lsa.LastLoginDate, lsa.LastIPAddress FROM account a LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name ORDER BY lsa.LastLoginDate DESC LIMIT 20;" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The server may be unavailable.
)
echo.
pause
goto menu

:show_ip_history
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set acct=
set /p acct=Enter account name: 
if "%acct%"=="" (
    echo No account name entered.
    echo.
    pause
    goto menu
)
echo Note: Only the most recent IP address is stored per account.
echo.
docker exec quarm-server mariadb -e "SELECT a.name, lsa.LastIPAddress, lsa.LastLoginDate FROM account a LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name WHERE LOWER(a.name)=LOWER('%acct%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The account may not exist.
)
echo.
pause
goto menu

:list_characters
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set acct=
set /p acct=Enter account name: 
if "%acct%"=="" (
    echo No account name entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT cd.name, cd.level, cd.class, cd.race, z.short_name AS zone FROM character_data cd JOIN account a ON cd.account_id=a.id LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id WHERE LOWER(a.name)=LOWER('%acct%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The account may not exist.
)
echo.
pause
goto menu

:character_info
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set charname=
set /p charname=Enter character name: 
if "%charname%"=="" (
    echo No character name entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT cd.name, cd.level, cd.class, cd.race, z.short_name AS zone, cd.cur_hp, cd.mana, cd.endurance, cd.str, cd.sta, cd.agi, cd.dex, cd.int, cd.wis, cd.cha, cd.exp, cd.aa_points, cd.aa_points_spent, cd.aa_exp, cd.e_percent_to_aa, CONCAT(FLOOR(cd.time_played/3600), 'h ', FLOOR((cd.time_played MOD 3600)/60), 'm') AS time_played, FROM_UNIXTIME(cd.last_login) AS last_login FROM character_data cd LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id WHERE LOWER(cd.name)=LOWER('%charname%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The character may not exist.
)
echo.
pause
goto menu

:show_inventory
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set charname=
set /p charname=Enter character name: 
if "%charname%"=="" (
    echo No character name entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT ci.slotid, i.name, ci.charges FROM character_inventory ci JOIN items i ON i.id=ci.itemid JOIN character_data cd ON cd.id=ci.id WHERE LOWER(cd.name)=LOWER('%charname%') ORDER BY ci.slotid;" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The character may not exist.
) else (
    echo.
    echo Note: slotid is numeric. Equipped items are slots 0-21.
)
echo.
pause
goto menu

:show_currency
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set charname=
set /p charname=Enter character name: 
if "%charname%"=="" (
    echo No character name entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT cc.platinum, cc.gold, cc.silver, cc.copper, cc.platinum_bank, cc.gold_bank, cc.silver_bank, cc.copper_bank, cc.platinum_cursor, cc.gold_cursor, cc.silver_cursor, cc.copper_cursor FROM character_currency cc JOIN character_data cd ON cd.id=cc.id WHERE LOWER(cd.name)=LOWER('%charname%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The character may not exist.
)
echo.
pause
goto menu

:show_account_for_character
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set charname=
set /p charname=Enter character name: 
if "%charname%"=="" (
    echo No character name entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT a.name AS account, a.status, cd.name AS character_name, cd.level, cd.class, cd.race FROM character_data cd JOIN account a ON cd.account_id=a.id WHERE LOWER(cd.name)=LOWER('%charname%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The character may not exist.
)
echo.
pause
goto menu

REM ================================================
REM  PLAYER ACTIONS
REM ================================================

:move_character_to_bind
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set charname=
set /p charname=Enter character name: 
if "%charname%"=="" (
    echo No character name entered.
    echo.
    pause
    goto menu
)
echo.
echo Current location for %charname%:
docker exec quarm-server mariadb -e "SELECT cd.name, z.short_name AS current_zone, z2.short_name AS bind_zone FROM character_data cd LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id LEFT JOIN character_bind cb ON cb.id=cd.id AND cb.is_home=0 LEFT JOIN zone z2 ON z2.zoneidnumber=cb.zone_id WHERE LOWER(cd.name)=LOWER('%charname%');" quarm
echo.
echo WARNING: The character must be logged OUT for this to work.
echo If they are logged in the server will overwrite this change on logout.
echo.
set confirm=
set /p confirm=Are they logged out? (Y/N): 
if /i not "%confirm%"=="Y" (
    echo Cancelled.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "UPDATE character_data cd JOIN character_bind cb ON cb.id=cd.id JOIN zone z ON z.zoneidnumber=cb.zone_id SET cd.zone_id=cb.zone_id, cd.x=cb.x, cd.y=cb.y, cd.z=cb.z, cd.heading=cb.heading WHERE LOWER(cd.name)=LOWER('%charname%') AND cb.is_home=0;" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The character may not exist.
) else (
    echo Done. %charname% has been moved to their bind point.
    echo They can now log in safely.
)
echo.
pause
goto menu

:move_character_to_zone
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set charname=
set /p charname=Enter character name: 
if "%charname%"=="" (
    echo No character name entered.
    echo.
    pause
    goto menu
)
echo.
echo Enter the zone short name e.g. qeynos, commons, unrest, nektulos
echo Use the short name not the full name.
echo.
set zonename=
set /p zonename=Enter zone short name (or part of it to search): 
if "%zonename%"=="" (
    echo No zone name entered.
    echo.
    pause
    goto menu
)
echo.
echo Matching zones:
docker exec quarm-server mariadb -e "SELECT short_name, long_name FROM zone WHERE LOWER(short_name) LIKE LOWER('%%%zonename%%%') OR LOWER(long_name) LIKE LOWER('%%%zonename%%%') ORDER BY short_name LIMIT 10;" quarm
echo.
set zonename=
set /p zonename=Enter exact zone short name to confirm (or leave blank to cancel): 
if "%zonename%"=="" (
    echo Cancelled.
    echo.
    pause
    goto menu
)
echo.
echo WARNING: The character must be logged OUT for this to work.
echo If they are logged in the server will overwrite this change on logout.
echo.
set confirm=
set /p confirm=Are they logged out? (Y/N): 
if /i not "%confirm%"=="Y" (
    echo Cancelled.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "UPDATE character_data cd JOIN zone z ON LOWER(z.short_name)=LOWER('%zonename%') SET cd.zone_id=z.zoneidnumber, cd.x=z.safe_x, cd.y=z.safe_y, cd.z=z.safe_z, cd.heading=z.safe_heading WHERE LOWER(cd.name)=LOWER('%charname%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The character or zone may not exist.
) else (
    echo Done. %charname% has been moved to %zonename%.
    echo Note: If no rows changed, check that both the character name and zone short name are correct.
    echo They can now log in safely.
)
echo.
pause
goto menu

:give_platinum
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set charname=
set /p charname=Enter character name: 
if "%charname%"=="" (
    echo No character name entered.
    echo.
    pause
    goto menu
)
set amount=
set /p amount=Enter amount of platinum to add: 
if "%amount%"=="" (
    echo No amount entered.
    echo.
    pause
    goto menu
)
REM Validate that amount is a positive number
echo %amount%| findstr /r "^[0-9][0-9]*$" >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Amount must be a positive whole number.
    echo.
    pause
    goto menu
)
echo.
echo Current currency for %charname%:
docker exec quarm-server mariadb -e "SELECT cc.platinum AS plat_carried, cc.platinum_bank AS plat_bank FROM character_currency cc JOIN character_data cd ON cd.id=cc.id WHERE LOWER(cd.name)=LOWER('%charname%');" quarm
echo.
echo WARNING: The character should be logged OUT for this to work reliably.
echo If they are logged in the server may overwrite the change on logout.
echo.
set confirm=
set /p confirm=Continue? (Y/N): 
if /i not "%confirm%"=="Y" (
    echo Cancelled.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "UPDATE character_currency cc JOIN character_data cd ON cd.id=cc.id SET cc.platinum=cc.platinum+%amount% WHERE LOWER(cd.name)=LOWER('%charname%');" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The character may not exist.
) else (
    echo Done. Added %amount% platinum to %charname%.
    echo Note: If no rows changed, the character name may not exist - check List Characters.
)
echo.
pause
goto menu

REM ================================================
REM  CORPSES
REM ================================================

:list_corpses
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT cc.charname, z.short_name AS zone, cc.time_of_death, cc.is_rezzed, cc.is_buried FROM character_corpses cc LEFT JOIN zone z ON z.zoneidnumber=cc.zone_id ORDER BY cc.time_of_death DESC;" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The server may be unavailable.
)
echo.
pause
goto menu

:show_corpses_by_character
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
set charname=
set /p charname=Enter character name: 
if "%charname%"=="" (
    echo No character name entered.
    echo.
    pause
    goto menu
)
docker exec quarm-server mariadb -e "SELECT cc.charname, z.short_name AS zone, cc.time_of_death, cc.is_rezzed, cc.is_buried, cc.platinum, cc.gold, cc.silver, cc.copper FROM character_corpses cc LEFT JOIN zone z ON z.zoneidnumber=cc.zone_id WHERE LOWER(cc.charname)=LOWER('%charname%') ORDER BY cc.time_of_death DESC;" quarm
if %errorlevel% neq 0 (
    echo ERROR: Query failed. The character may not exist or has no corpses.
)
echo.
pause
goto menu

REM ================================================
REM  SERVER
REM ================================================

:server_status
docker exec quarm-server echo ok >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Cannot reach the quarm-server container.
    echo Make sure the server is running - use start.bat to start it.
    echo.
    pause
    goto menu
)
echo Checking server processes...
echo.
docker exec quarm-server bash -c "pgrep -x mariadbd >/dev/null 2>&1 && echo 'mariadb    : RUNNING' || echo 'mariadb    : DOWN'; pgrep -x loginserver >/dev/null 2>&1 && echo 'loginserver: RUNNING' || echo 'loginserver: DOWN'; pgrep -x world >/dev/null 2>&1 && echo 'world      : RUNNING' || echo 'world      : DOWN'; pgrep -x eqlaunch >/dev/null 2>&1 && echo 'eqlaunch   : RUNNING' || echo 'eqlaunch   : DOWN'; pgrep -x queryserv >/dev/null 2>&1 && echo 'queryserv  : RUNNING' || echo 'queryserv  : DOWN'; pgrep -x ucs >/dev/null 2>&1 && echo 'ucs        : RUNNING' || echo 'ucs        : DOWN'; echo zone processes running: ; pgrep -c zone"
if %errorlevel% neq 0 (
    echo ERROR: Could not check process status.
)
echo.
pause
goto menu
