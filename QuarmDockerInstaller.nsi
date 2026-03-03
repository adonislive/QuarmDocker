; ============================================================
; QuarmDockerInstaller.nsi
; QuarmDocker Windows Installer
; Requires: NSIS 3.x with inetc plugin
;
; Build: makensis QuarmDockerInstaller.nsi
;   OR right-click the file -> "Compile NSIS Script"
;
; inetc plugin (needed if Docker Desktop must be downloaded):
;   https://nsis.sourceforge.io/Inetc_plug-in
;   Copy inetc.dll to C:\Program Files (x86)\NSIS\Plugins\x86-ansi\
;
; Place these files in the same folder as this .nsi before building:
;   docker-compose.yml
;   Dockerfile
;   entrypoint.sh         (must have LF line endings, not CRLF)
;   init.sh               (must have LF line endings, not CRLF)
;   eqemu_config.json
;   login.json
;   .gitattributes
;   QuarmDockerServer.exe
;   config\win\start.bat
;   config\win\stop.bat
;   config\win\backup.bat
;   config\win\restore.bat
;   config\win\manage.bat
;   config\win\rebuild.bat
;   config\win\export_characters.bat
;   config\win\status.bat
;   config\win\readme.txt
;   config\backups\.gitkeep
; ============================================================

!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
!include "nsDialogs.nsh"
!include "FileFunc.nsh"

; ============================================================
; INSTALLER METADATA
; ============================================================
Name              "Quarm Docker Server"
OutFile           "QuarmDockerInstaller.exe"
InstallDir        "C:\QuarmDocker"
InstallDirRegKey  HKLM "Software\QuarmDocker" "InstallDir"
RequestExecutionLevel admin
Unicode True
SetCompressor     /SOLID lzma

; ============================================================
; CONSTANTS
; ============================================================
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\QuarmDocker"
!define AUTORUN_KEY   "Software\Microsoft\Windows\CurrentVersion\Run"
!define APP_KEY       "Software\QuarmDocker"

; ============================================================
; VARIABLES
; ============================================================
Var Dialog
Var NetworkMode         ; 0=local     1=LAN
Var SelectedIP
Var SystemRAM_MB
Var WSL2_Memory_MB
Var WSL2_Processors
Var AutoStart           ; 0 or 1
Var InstallLogPath
Var InstallLogHandle
Var WaitCounter

; Dialog control handles
Var Ctrl_RadioLocal
Var Ctrl_RadioLAN
Var Ctrl_AdapterList
Var Ctrl_AdapterLabel
Var Ctrl_DirText
Var Ctrl_ChkAutoStart
Var Ctrl_ProgressText
Var Ctrl_DetailText

; ============================================================
; PAGES
; ============================================================
Page custom PageWelcome_Show
Page custom PagePrereq_Show  PagePrereq_Leave
Page custom PageConfig_Show  PageConfig_Leave
Page custom PageDownload_Show PageDownload_Leave
Page instfiles
Page custom PageComplete_Show PageComplete_Leave

UninstPage custom un.UnPageConfirm_Show un.UnPageConfirm_Leave
UninstPage instfiles

; ============================================================
; LOGGING MACROS
; ============================================================
!macro LogLine text
    FileWrite $InstallLogHandle "${text}$\r$\n"
!macroend

; ============================================================
; PAGE: WELCOME
; ============================================================
Function PageWelcome_Show
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 80u \
        "Welcome to Quarm Docker Server Setup$\r$\n$\r$\n\
This installer will:$\r$\n\
  - Check your computer meets the requirements$\r$\n\
  - Install WSL2 and Docker Desktop if needed$\r$\n\
  - Build the Quarm server (30-45 minutes)$\r$\n\
  - Configure Windows Firewall and Defender$\r$\n\
  - Create a desktop shortcut to Quarm Docker Server$\r$\n$\r$\n\
Your computer will be ready to run a private Quarm server.$\r$\n$\r$\n\
Click Next to begin."
    Pop $0

    nsDialogs::Show
FunctionEnd

; ============================================================
; PAGE: PREREQUISITES
; ============================================================
Function PagePrereq_Show
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 12u "Prerequisite checks will run when you click Next."
    Pop $0
    ${NSD_CreateLabel} 0 18u 100% 140u \
        "The following will be verified:$\r$\n\
  - Windows 10 version 2004 or later$\r$\n\
  - 64-bit (x64) Windows$\r$\n\
  - Minimum 12 GB RAM recommended (8 GB minimum)$\r$\n\
  - WSL2 with Ubuntu (will be installed if missing)$\r$\n\
  - Docker Desktop (will be downloaded if missing)$\r$\n\
  - Docker engine is running$\r$\n$\r$\n\
If WSL2 needs to be installed, one reboot will be required.$\r$\n\
Just run this installer again after rebooting."
    Pop $0

    nsDialogs::Show
FunctionEnd

Function PagePrereq_Leave
    ; Open log file
    StrCpy $InstallLogPath "$TEMP\QuarmDocker_install.log"
    FileOpen $InstallLogHandle $InstallLogPath w
    !insertmacro LogLine "QuarmDocker Installer Log"
    !insertmacro LogLine "========================="

    ; --- Check 0: Verify we have admin privileges ---
    nsExec::ExecToStack 'net session'
    Pop $0
    Pop $1
    ${If} $0 != 0
        !insertmacro LogLine "WARNING: Not running as administrator"
        MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
            "This installer is not running with administrator privileges.$\n$\n\
Some features (firewall rules, Defender exclusions) will not work.$\n\
For best results, close this installer, right-click it, and$\n\
select 'Run as administrator'.$\n$\n\
Click OK to continue anyway, or Cancel to exit." IDOK prereq_admin_continue
        FileClose $InstallLogHandle
        Abort
        prereq_admin_continue:
    ${Else}
        !insertmacro LogLine "PASS: Running as administrator"
    ${EndIf}

    ; --- Check 1: Windows version ---
    ${If} ${AtLeastWin10}
        !insertmacro LogLine "PASS: Windows 10+"
    ${Else}
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONSTOP \
            "Windows 10 version 2004 or later is required.$\nPlease upgrade Windows and try again."
        Abort
    ${EndIf}

    ; --- Check 2: x64 ---
    ${If} ${RunningX64}
        !insertmacro LogLine "PASS: x64"
    ${Else}
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONSTOP \
            "64-bit Windows is required. Docker Desktop only runs on 64-bit Windows."
        Abort
    ${EndIf}

    ; --- Check 3: RAM (for WSL2 memory cap calculation) ---
    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1MB)"'
    Pop $0
    Pop $SystemRAM_MB
    StrCpy $SystemRAM_MB $SystemRAM_MB -2   ; trim trailing CR LF

    IntOp $WSL2_Memory_MB $SystemRAM_MB * 75
    IntOp $WSL2_Memory_MB $WSL2_Memory_MB / 100
    ${If} $WSL2_Memory_MB < 2048
        StrCpy $WSL2_Memory_MB 2048
    ${EndIf}
    ${If} $WSL2_Memory_MB > 8192
        StrCpy $WSL2_Memory_MB 8192
    ${EndIf}

    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "(Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors"'
    Pop $0
    Pop $WSL2_Processors
    StrCpy $WSL2_Processors $WSL2_Processors -2
    ${If} $WSL2_Processors > 4
        StrCpy $WSL2_Processors 4
    ${EndIf}
    ${If} $WSL2_Processors < 1
        StrCpy $WSL2_Processors 2
    ${EndIf}
    !insertmacro LogLine "RAM: $SystemRAM_MB MB  WSL2 cap: $WSL2_Memory_MB MB  procs: $WSL2_Processors"

    ; --- Check 3b: Low RAM warning ---
    ${If} $SystemRAM_MB < 12000
        !insertmacro LogLine "WARNING: Low RAM ($SystemRAM_MB MB) - build may fail"
        MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
            "Your system has $SystemRAM_MB MB of RAM.$\n$\n\
Building the server requires significant memory.$\n\
Systems with less than 12 GB may experience build failures.$\n$\n\
You can continue, but if the build fails, try closing$\n\
other applications and retrying, or use a machine$\n\
with more RAM.$\n$\n\
Continue anyway?" IDOK prereq_ram_ok
        FileClose $InstallLogHandle
        Abort
        prereq_ram_ok:
    ${EndIf}

    ; --- Check 4: WSL2 + Ubuntu distro ---
    ; Step A: Check if WSL feature is enabled at all
    ; wsl --list --quiet returns 0 if WSL is enabled, lists distro names
    nsExec::ExecToStack 'wsl --list --quiet'
    Pop $0
    Pop $1
    ${If} $0 != 0
        ; WSL feature not installed — use ExecWait for real console context
        ; CRITICAL: nsExec silently fails for wsl --install (confirmed by Azure testing)
        !insertmacro LogLine "WSL2 not present - installing WSL2 + Ubuntu..."
        ExecWait 'wsl --install --distribution Ubuntu-24.04' $0
        !insertmacro LogLine "WSL2 + Ubuntu install exit: $0"
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONINFORMATION \
            "WSL2 and Ubuntu are being installed.$\n$\n\
After the setup completes, please restart your computer$\n\
and run this installer again.$\n$\n\
(This is the only reboot required.)"
        Quit
    ${EndIf}

    ; Step B: WSL feature is enabled — check if a distro exists
    ; $1 has the output from wsl --list --quiet (distro names)
    StrCpy $1 $1 -2   ; trim trailing CR LF
    StrLen $2 $1
    ${If} $2 < 2
        ; WSL enabled but no distro — install Ubuntu
        !insertmacro LogLine "WSL2 present but no distro - installing Ubuntu..."
        ExecWait 'wsl --install --distribution Ubuntu-24.04' $0
        !insertmacro LogLine "Ubuntu install exit: $0"
        ; Re-check if distro is now available
        nsExec::ExecToStack 'wsl --list --quiet'
        Pop $0
        Pop $1
        StrCpy $1 $1 -2
        StrLen $2 $1
        ${If} $2 < 2
            ; Still no distro — needs reboot
            FileClose $InstallLogHandle
            MessageBox MB_OK|MB_ICONINFORMATION \
                "Ubuntu has been installed but a restart is required.$\n$\n\
Please restart your computer and run this installer again."
            Quit
        ${EndIf}
    ${EndIf}
    !insertmacro LogLine "PASS: WSL2 + distro present"

    ; --- Check 4b: Write .wslconfig BEFORE Docker starts ---
    ; Writing it here avoids the wsl --shutdown crash during install phase
    StrCpy $1 "$WSL2_Memory_MB"
    StrCpy $1 "$1MB"
    FileOpen $0 "$PROFILE\.wslconfig" w
    FileWrite $0 "[wsl2]$\r$\n"
    FileWrite $0 "memory=$1$\r$\n"
    FileWrite $0 "processors=$WSL2_Processors$\r$\n"
    FileClose $0
    !insertmacro LogLine "wslconfig written: memory=$1  processors=$WSL2_Processors"

    ; --- Check 5: Docker Desktop ---
    IfFileExists "$PROGRAMFILES64\Docker\Docker\Docker Desktop.exe" prereq_docker_found
    IfFileExists "$PROGRAMFILES\Docker\Docker\Docker Desktop.exe"   prereq_docker_found
    ; Check registry install path
    ReadRegStr $0 HKLM "Software\Docker Inc.\Docker Desktop" "InstallDir"
    ${If} $0 != ""
        IfFileExists "$0\Docker Desktop.exe" prereq_docker_found
    ${EndIf}
    ; Check alternate registry key
    ReadRegStr $0 HKCU "Software\Docker Inc.\Docker Desktop" "InstallDir"
    ${If} $0 != ""
        IfFileExists "$0\Docker Desktop.exe" prereq_docker_found
    ${EndIf}
    ; Check if docker.exe is on PATH (Docker Desktop adds it)
    nsExec::ExecToStack 'cmd /C where docker'
    Pop $0
    Pop $1
    ${If} $0 = 0
        Goto prereq_docker_found
    ${EndIf}

    ; Not found - download and install
    !insertmacro LogLine "Docker Desktop not found - downloading..."
    inetc::get \
        "https://desktop.docker.com/win/main/amd64/Docker%20Desktop%20Installer.exe" \
        "$TEMP\DockerDesktopInstaller.exe" /END
    Pop $0
    ${If} $0 != "OK"
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONSTOP \
            "Could not download Docker Desktop.$\nCheck your internet connection and try again."
        Abort
    ${EndIf}
    ExecWait '"$TEMP\DockerDesktopInstaller.exe" install --quiet --accept-license' $0
    Delete "$TEMP\DockerDesktopInstaller.exe"
    ${If} $0 != 0
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONSTOP \
            "Docker Desktop installation failed.$\nPlease install it from docker.com and run this installer again."
        Abort
    ${EndIf}
    !insertmacro LogLine "Docker Desktop installed"

    prereq_docker_found:
    !insertmacro LogLine "PASS: Docker Desktop present"

    ; --- Check 6: Docker engine running (retry loop) ---
    nsExec::ExecToStack 'docker info'
    Pop $0
    Pop $1
    IntCmp $0 0 prereq_docker_ok
    MessageBox MB_OK|MB_ICONEXCLAMATION \
        "Docker Desktop is not running.$\n$\n\
Please open Docker Desktop and wait for it to start,$\n\
then click Next again."
    FileClose $InstallLogHandle
    Abort
    prereq_docker_ok:
    !insertmacro LogLine "PASS: Docker engine running"

    ; --- Check 6b: Enable Docker WSL integration for Ubuntu ---
    ; Docker Desktop must have WSL integration enabled for the Ubuntu distro
    !insertmacro LogLine "Configuring Docker WSL integration..."
    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "$$settingsPath = [System.IO.Path]::Combine($$env:APPDATA, ''Docker'', ''settings-store.json''); \
        if (Test-Path $$settingsPath) { \
            $$j = Get-Content $$settingsPath -Raw | ConvertFrom-Json; \
            if (-not $$j.PSObject.Properties[''integratedWslDistros'']) { \
                $$j | Add-Member -NotePropertyName ''integratedWslDistros'' -NotePropertyValue (New-Object PSObject) -Force \
            }; \
            $$j.integratedWslDistros | Add-Member -NotePropertyName ''Ubuntu-24.04'' -NotePropertyValue $$true -Force; \
            $$j | ConvertTo-Json -Depth 10 | Set-Content $$settingsPath -Encoding UTF8; \
            Write-Output ''OK'' \
        } else { Write-Output ''NOFILE'' }"'
    Pop $0
    Pop $1
    StrCpy $1 $1 -2
    !insertmacro LogLine "Docker WSL integration config: $1 (exit: $0)"
    ${If} $1 == "OK"
        ; Restart Docker Desktop to pick up the new setting
        !insertmacro LogLine "Restarting Docker to apply WSL integration..."
        nsExec::ExecToStack 'powershell -NoProfile -Command \
            "Stop-Process -Name ''Docker Desktop'' -Force -ErrorAction SilentlyContinue; \
            Start-Sleep -Seconds 3; \
            Start-Process ''$PROGRAMFILES64\Docker\Docker\Docker Desktop.exe''"'
        Pop $0
        Pop $1
        ; Wait for Docker to come back up
        Sleep 5000
        StrCpy $WaitCounter 0
        prereq_wsl_docker_wait:
            nsExec::ExecToStack 'docker info'
            Pop $0
            Pop $1
            IntCmp $0 0 prereq_wsl_docker_ready
            IntOp $WaitCounter $WaitCounter + 1
            IntCmp $WaitCounter 40 prereq_wsl_docker_timeout prereq_wsl_docker_keepwait prereq_wsl_docker_timeout
            prereq_wsl_docker_timeout:
            MessageBox MB_OK|MB_ICONEXCLAMATION \
                "Docker is taking a long time to restart after WSL configuration.$\n$\n\
Please wait for Docker Desktop to fully start, then click Next again."
            FileClose $InstallLogHandle
            Abort
            prereq_wsl_docker_keepwait:
            Sleep 3000
            Goto prereq_wsl_docker_wait
        prereq_wsl_docker_ready:
        !insertmacro LogLine "Docker restarted with WSL integration"
    ${Else}
        ; Could not find or edit settings file — warn user with manual steps
        !insertmacro LogLine "WARNING: Could not auto-configure Docker WSL integration"
        MessageBox MB_OK|MB_ICONEXCLAMATION \
            "Could not automatically enable Docker WSL integration.$\n$\n\
Please do this manually:$\n\
1. Open Docker Desktop$\n\
2. Click the gear icon (Settings)$\n\
3. Go to Resources > WSL integration$\n\
4. Enable the toggle for Ubuntu-24.04$\n\
5. Click Apply & restart$\n$\n\
Then click Next to continue."
    ${EndIf}

    ; --- Check 7: Set Docker context ---
    nsExec::ExecToStack 'docker context use desktop-linux'
    Pop $0
    Pop $1
    !insertmacro LogLine "docker context use desktop-linux: exit $0"
FunctionEnd

; ============================================================
; CONFIG PAGE CALLBACKS - must be top-level Functions
; ============================================================

Function Config_BrowseDir
    nsDialogs::SelectFolderDialog "Select installation folder" $INSTDIR
    Pop $0
    ${If} $0 != error
        ${NSD_SetText} $Ctrl_DirText $0
    ${EndIf}
FunctionEnd

Function Config_ShowAdapters
    ShowWindow $Ctrl_AdapterLabel ${SW_SHOW}
    ShowWindow $Ctrl_AdapterList  ${SW_SHOW}
    ; Write adapters to temp file - one per line
    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "Get-NetIPAddress -AddressFamily IPv4 | \
        Where-Object { $$_.IPAddress -ne ''127.0.0.1'' } | \
        ForEach-Object { \
            $$a = Get-NetAdapter -InterfaceIndex $$_.InterfaceIndex -ErrorAction SilentlyContinue; \
            if ($$a) { $$a.Name + '' - '' + $$_.IPAddress } \
        } | Set-Content -Path ''$TEMP\qd_adapters.txt'' -Encoding UTF8"'
    Pop $0
    Pop $1
    SendMessage $Ctrl_AdapterList ${LB_RESETCONTENT} 0 0
    ClearErrors
    FileOpen $0 "$TEMP\qd_adapters.txt" r
    ${If} ${Errors}
        SendMessage $Ctrl_AdapterList ${LB_ADDSTRING} 0 "STR:No adapters found"
        Return
    ${EndIf}
    adapter_read_loop:
        FileRead $0 $1
        IfErrors adapter_read_done
        ; Trim trailing CR LF
        StrCpy $1 $1 -1
        StrCpy $2 $1 1 -1
        ${If} $2 == "$\r"
            StrCpy $1 $1 -1
        ${EndIf}
        ${If} $1 != ""
            SendMessage $Ctrl_AdapterList ${LB_ADDSTRING} 0 "STR:$1"
        ${EndIf}
        Goto adapter_read_loop
    adapter_read_done:
    FileClose $0
    Delete "$TEMP\qd_adapters.txt"
FunctionEnd

Function Config_HideAdapters
    ShowWindow $Ctrl_AdapterLabel ${SW_HIDE}
    ShowWindow $Ctrl_AdapterList  ${SW_HIDE}
FunctionEnd

; ============================================================
; PAGE: CONFIGURATION
; ============================================================
Function PageConfig_Show
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 12u "Installation Directory:"
    Pop $0

    ${NSD_CreateDirRequest} 0 14u 72% 14u $INSTDIR
    Pop $Ctrl_DirText

    ${NSD_CreateBrowseButton} 74% 14u 25% 14u "Browse..."
    Pop $0
    ${NSD_OnClick} $0 Config_BrowseDir

    ${NSD_CreateLabel} 0 36u 100% 12u "Network:"
    Pop $0

    ${NSD_CreateRadioButton} 8u 50u 100% 14u \
        "Local only - only this computer can connect (recommended)"
    Pop $Ctrl_RadioLocal
    ${NSD_SetState} $Ctrl_RadioLocal ${BST_CHECKED}

    ${NSD_CreateRadioButton} 8u 66u 100% 14u \
        "LAN - other computers on my network can connect"
    Pop $Ctrl_RadioLAN

    ${NSD_CreateLabel} 8u 84u 90% 12u "Select your network adapter:"
    Pop $Ctrl_AdapterLabel
    ShowWindow $Ctrl_AdapterLabel ${SW_HIDE}

    ${NSD_CreateListBox} 8u 98u 90% 52u ""
    Pop $Ctrl_AdapterList
    ShowWindow $Ctrl_AdapterList ${SW_HIDE}

    ${NSD_CreateCheckbox} 0 160u 100% 14u \
        "Start Quarm Docker Server automatically with Windows"
    Pop $Ctrl_ChkAutoStart

    ${NSD_OnClick} $Ctrl_RadioLAN  Config_ShowAdapters
    ${NSD_OnClick} $Ctrl_RadioLocal Config_HideAdapters

    nsDialogs::Show
FunctionEnd

Function PageConfig_Leave
    ${NSD_GetText} $Ctrl_DirText $INSTDIR
    ${If} $INSTDIR == ""
        MessageBox MB_OK|MB_ICONEXCLAMATION "Please select an installation directory."
        Abort
    ${EndIf}

    ; Disk space check: 10 GB minimum
    ${GetRoot} "$INSTDIR" $0
    ${DriveSpace} "$0\" "/D=F /S=M" $1
    ${If} $1 < 10240
        MessageBox MB_OK|MB_ICONSTOP \
            "Not enough free space on $0.$\n\
At least 10 GB is required.$\nCurrently available: $1 MB"
        Abort
    ${EndIf}

    ; Network mode
    ${NSD_GetState} $Ctrl_RadioLAN $0
    ${If} $0 = ${BST_CHECKED}
        StrCpy $NetworkMode 1
        SendMessage $Ctrl_AdapterList ${LB_GETCURSEL} 0 0 $1
        ${If} $1 = -1
            MessageBox MB_OK|MB_ICONEXCLAMATION \
                "Please select a network adapter for LAN mode."
            Abort
        ${EndIf}
        ; Get selected item text via Win32 SendMessage
        System::Call 'user32::SendMessageW(p $Ctrl_AdapterList, i ${LB_GETTEXT}, i $1, t .r2)'
        ; Extract IP = text after " - "
        StrLen $3 $2
        StrCpy $SelectedIP ""
        StrCpy $4 0
        config_find_sep:
            ${If} $4 >= $3
                StrCpy $SelectedIP $2   ; fallback: use whole string
                Goto config_sep_done
            ${EndIf}
            StrCpy $5 $2 3 $4
            ${If} $5 == " - "
                IntOp $4 $4 + 3
                StrCpy $SelectedIP $2 "" $4
                Goto config_sep_done
            ${EndIf}
            IntOp $4 $4 + 1
            Goto config_find_sep
        config_sep_done:
        !insertmacro LogLine "LAN mode, adapter IP: $SelectedIP"
    ${Else}
        StrCpy $NetworkMode 0
        StrCpy $SelectedIP "127.0.0.1"
        !insertmacro LogLine "Local only mode"
    ${EndIf}

    ${NSD_GetState} $Ctrl_ChkAutoStart $0
    ${If} $0 = ${BST_CHECKED}
        StrCpy $AutoStart 1
    ${Else}
        StrCpy $AutoStart 0
    ${EndIf}

    !insertmacro LogLine "InstallDir: $INSTDIR  IP: $SelectedIP  AutoStart: $AutoStart"
FunctionEnd

; ============================================================
; PAGE: BUILD / INSTALL (Show - informational only)
; ============================================================
Function PageDownload_Show
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 14u "Starting the server..."
    Pop $Ctrl_ProgressText

    ${NSD_CreateLabel} 0 18u 100% 160u \
        "Click Next to begin the installation.$\r$\n$\r$\n\
The installer will:$\r$\n\
  - Add Windows Firewall rules$\r$\n\
  - Add Windows Defender exclusions$\r$\n\
  - Copy project files$\r$\n\
  - Build the server from source (30-45 minutes)$\r$\n\
  - Start the server$\r$\n$\r$\n\
The build takes 30-45 minutes. Do not close this window.$\r$\n\
The window may appear frozen during compilation - this is normal."
    Pop $Ctrl_DetailText

    nsDialogs::Show
FunctionEnd

; ============================================================
; PAGE: BUILD / INSTALL (Leave - all install work happens here)
; ============================================================
Function PageDownload_Leave

    !insertmacro LogLine "Mode: build from source"

    ; ---- Firewall rules ----
    ${NSD_SetText} $Ctrl_ProgressText "Writing Windows Firewall rules..."
    nsExec::ExecToStack 'powershell -NoProfile -ExecutionPolicy Bypass -Command \
        "@(''Quarm-Login-UDP'',''Quarm-Login-TCP'',''Quarm-World-UDP'',''Quarm-World-TCP'', \
        ''Quarm-UCS-UDP'',''Quarm-DB-TCP'',''Quarm-Zones-UDP'',''Quarm-Zones-TCP'') | \
        ForEach-Object { Remove-NetFirewallRule -DisplayName $$_ -EA SilentlyContinue }; \
        New-NetFirewallRule -DisplayName ''Quarm-Login-UDP''  -Direction Inbound -Protocol UDP -LocalPort 6000       -Action Allow | Out-Null; \
        New-NetFirewallRule -DisplayName ''Quarm-Login-TCP''  -Direction Inbound -Protocol TCP -LocalPort 5998       -Action Allow | Out-Null; \
        New-NetFirewallRule -DisplayName ''Quarm-World-UDP''  -Direction Inbound -Protocol UDP -LocalPort 9000       -Action Allow | Out-Null; \
        New-NetFirewallRule -DisplayName ''Quarm-World-TCP''  -Direction Inbound -Protocol TCP -LocalPort 9000       -Action Allow | Out-Null; \
        New-NetFirewallRule -DisplayName ''Quarm-UCS-UDP''    -Direction Inbound -Protocol UDP -LocalPort 7778       -Action Allow | Out-Null; \
        New-NetFirewallRule -DisplayName ''Quarm-DB-TCP''     -Direction Inbound -Protocol TCP -LocalPort 3306       -Action Allow | Out-Null; \
        New-NetFirewallRule -DisplayName ''Quarm-Zones-UDP''  -Direction Inbound -Protocol UDP -LocalPort 7000-7400  -Action Allow | Out-Null; \
        New-NetFirewallRule -DisplayName ''Quarm-Zones-TCP''  -Direction Inbound -Protocol TCP -LocalPort 7000-7400  -Action Allow | Out-Null"'
    Pop $0
    Pop $1
    !insertmacro LogLine "Firewall rules exit: $0"

    ; ---- Defender exclusions ----
    ${NSD_SetText} $Ctrl_ProgressText "Adding Windows Defender exclusions..."
    nsExec::ExecToStack 'powershell -NoProfile -ExecutionPolicy Bypass -Command \
        "Add-MpPreference -ExclusionPath \
            ''$APPDATA\Docker'',''$LOCALAPPDATA\Docker'',''$INSTDIR'' \
            -ErrorAction SilentlyContinue"'
    Pop $0
    Pop $1
    !insertmacro LogLine "Defender exclusions exit: $0"

    ; ---- Copy project files ----
    ${NSD_SetText} $Ctrl_ProgressText "Copying project files..."

    SetOutPath "$INSTDIR"
    File "docker-compose.yml"
    File "Dockerfile"
    File "entrypoint.sh"
    File "init.sh"
    File "config\eqemu_config.json"
    File "config\login.json"
    File "QuarmDockerServer.exe"

    SetOutPath "$INSTDIR\config\win"
    File "config\win\start.bat"
    File "config\win\stop.bat"
    File "config\win\backup.bat"
    File "config\win\restore.bat"
    File "config\win\manage.bat"
    File "config\win\rebuild.bat"
    File "config\win\export_characters.bat"
    File "config\win\status.bat"
    File "config\win\readme.txt"

    SetOutPath "$INSTDIR\config\backups"
    File "config\backups\.gitkeep"

    SetOutPath "$INSTDIR"
    FileOpen $0 "$INSTDIR\.env" w
    FileWrite $0 "SERVER_ADDRESS=$SelectedIP$\n"
    FileClose $0
    !insertmacro LogLine ".env written: SERVER_ADDRESS=$SelectedIP"

    ; ---- Verify Docker is still running before build ----
    ${NSD_SetText} $Ctrl_ProgressText "Verifying Docker is ready..."
    nsExec::ExecToStack 'docker info'
    Pop $0
    Pop $1
    ${If} $0 != 0
        !insertmacro LogLine "Docker not ready before build - waiting..."
        StrCpy $WaitCounter 0
        dl_prebuild_wait:
            Sleep 3000
            nsExec::ExecToStack 'docker info'
            Pop $0
            Pop $1
            IntCmp $0 0 dl_prebuild_ready
            IntOp $WaitCounter $WaitCounter + 1
            IntCmp $WaitCounter 20 dl_prebuild_timeout dl_prebuild_keepwait dl_prebuild_timeout
            dl_prebuild_timeout:
            MessageBox MB_OK|MB_ICONEXCLAMATION \
                "Docker Desktop is not responding.$\n$\n\
Please make sure Docker Desktop is running, then click Next again."
            Abort
            dl_prebuild_keepwait:
            Goto dl_prebuild_wait
        dl_prebuild_ready:
        !insertmacro LogLine "Docker ready (after wait)"
    ${EndIf}
    nsExec::ExecToStack 'docker context use desktop-linux'
    Pop $0

    ; ---- Check port 3306 conflict ----
    ${NSD_SetText} $Ctrl_ProgressText "Checking for port 3306 conflict..."
    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "(Test-NetConnection -ComputerName 127.0.0.1 -Port 3306 \
        -InformationLevel Quiet -WarningAction SilentlyContinue).TcpTestSucceeded"'
    Pop $0
    Pop $1
    StrCpy $1 $1 -2
    StrCmp $1 "True" 0 dl_port_ok
    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "((Get-NetTCPConnection -LocalPort 3306 -EA SilentlyContinue | \
        Select-Object -First 1 | ForEach-Object { \
        (Get-Process -Id $$_.OwningProcess -EA SilentlyContinue).Name }))"'
    Pop $0
    Pop $2
    StrCpy $2 $2 -2
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
        "Port 3306 is in use (process: $2).$\n$\n\
This port is needed by the Quarm server database.$\n\
Click OK to stop and disable the conflicting service automatically." IDOK dl_stop_mysql
    MessageBox MB_OK|MB_ICONSTOP "Port 3306 is in use. Free this port and run the installer again."
    Abort
    dl_stop_mysql:
    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "Stop-Service MySQL -EA SilentlyContinue; \
        Stop-Service MariaDB -EA SilentlyContinue; \
        Set-Service MySQL -StartupType Disabled -EA SilentlyContinue; \
        Set-Service MariaDB -StartupType Disabled -EA SilentlyContinue"'
    Pop $0
    Pop $1
    Sleep 2000
    dl_port_ok:

    ; ---- Build attempt 1 ----
    ${NSD_SetText} $Ctrl_ProgressText "Building from source (30-45 minutes)..."
    ${NSD_SetText} $Ctrl_DetailText \
        "Compiling the Quarm server. This takes 30-45 minutes.$\r$\n\
DO NOT CLOSE THIS WINDOW.$\r$\n\
The window will appear frozen - this is normal."
    !insertmacro LogLine "Starting docker compose build..."
    SetOutPath "$INSTDIR"
    nsExec::ExecToStack 'docker compose build'
    Pop $0
    Pop $1
    FileWrite $InstallLogHandle "$1"
    !insertmacro LogLine "docker compose build exit: $0"
    IntCmp $0 0 dl_build_ok

    ; ---- Build attempt 2 ----
    MessageBox MB_OKCANCEL|MB_ICONSTOP \
        "Build failed (attempt 1 of 3).$\n$\n\
This can happen if Docker was recently installed or restarted.$\n\
Click OK to retry - Docker resumes from where it stopped.$\n\
(The build cache preserves completed steps.)$\n$\n\
Log: $InstallLogPath" IDOK dl_retry_build_2
    Abort
    dl_retry_build_2:
    !insertmacro LogLine "Retry build attempt 2..."
    nsExec::ExecToStack 'docker compose build'
    Pop $0
    Pop $1
    FileWrite $InstallLogHandle "$1"
    !insertmacro LogLine "Retry build 2 exit: $0"
    IntCmp $0 0 dl_build_ok

    ; ---- Build attempt 3 ----
    MessageBox MB_OKCANCEL|MB_ICONSTOP \
        "Build failed (attempt 2 of 3).$\n$\n\
Click OK for one more attempt.$\n\
If this fails, try restarting your computer and$\n\
running this installer again.$\n$\n\
Log: $InstallLogPath" IDOK dl_retry_build_3
    Abort
    dl_retry_build_3:
    !insertmacro LogLine "Retry build attempt 3..."
    nsExec::ExecToStack 'docker compose build'
    Pop $0
    Pop $1
    FileWrite $InstallLogHandle "$1"
    !insertmacro LogLine "Retry build 3 exit: $0"
    IntCmp $0 0 dl_build_ok

    ; All 3 attempts failed
    MessageBox MB_OK|MB_ICONSTOP \
        "Build failed after 3 attempts.$\n$\n\
Suggestions:$\n\
- Restart your computer and run this installer again$\n\
- Close other applications to free memory$\n\
- Check the log: $InstallLogPath$\n$\n\
For help: https://github.com/adonislive/QuarmDocker/issues"
    Abort
    dl_build_ok:

    ; ---- First Start ----
    ${NSD_SetText} $Ctrl_ProgressText "Starting the server..."
    SetOutPath "$INSTDIR"
    nsExec::ExecToStack 'docker compose up -d'
    Pop $0
    Pop $1
    !insertmacro LogLine "docker compose up -d exit: $0"
    ${If} $0 != 0
        MessageBox MB_OK|MB_ICONSTOP \
            "Failed to start the server.$\nLog: $InstallLogPath"
        Abort
    ${EndIf}

    ; Sentinel file
    FileOpen $0 "$INSTDIR\.setup_complete" w
    FileWrite $0 "1"
    FileClose $0
    !insertmacro LogLine "Sentinel file written"

    ; Auto-start registry
    ${If} $AutoStart = 1
        WriteRegStr HKCU ${AUTORUN_KEY} "QuarmDockerServer" \
            "$INSTDIR\QuarmDockerServer.exe"
        !insertmacro LogLine "Auto-start entry written"
    ${EndIf}

    ; Shortcuts
    CreateShortcut "$DESKTOP\Quarm Docker Server.lnk" "$INSTDIR\QuarmDockerServer.exe"
    CreateDirectory "$SMPROGRAMS\Quarm Docker Server"
    CreateShortcut "$SMPROGRAMS\Quarm Docker Server\Quarm Docker Server.lnk" \
        "$INSTDIR\QuarmDockerServer.exe"
    CreateShortcut "$SMPROGRAMS\Quarm Docker Server\Uninstall.lnk" \
        "$INSTDIR\Uninstall.exe"

    ; Add/Remove Programs
    WriteRegStr   HKLM ${UNINSTALL_KEY} "DisplayName"     "Quarm Docker Server"
    WriteRegStr   HKLM ${UNINSTALL_KEY} "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr   HKLM ${UNINSTALL_KEY} "InstallLocation" "$INSTDIR"
    WriteRegStr   HKLM ${UNINSTALL_KEY} "DisplayVersion"  "1.0"
    WriteRegStr   HKLM ${UNINSTALL_KEY} "Publisher"       "QuarmDocker"
    WriteRegStr   HKLM ${UNINSTALL_KEY} "URLInfoAbout"    "https://github.com/adonislive/QuarmDocker"
    WriteRegDWORD HKLM ${UNINSTALL_KEY} "NoModify" 1
    WriteRegDWORD HKLM ${UNINSTALL_KEY} "NoRepair"  1
    WriteRegStr   HKLM ${APP_KEY} "InstallDir" "$INSTDIR"

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Close log LAST - no LogLine calls after this
    !insertmacro LogLine "=== Installation complete ==="
    FileClose $InstallLogHandle
    CopyFiles /SILENT "$InstallLogPath" "$INSTDIR\install.log"

    ${NSD_SetText} $Ctrl_ProgressText "Installation complete."
    ${NSD_SetText} $Ctrl_DetailText "The server is installed and running. Click Next to finish."
FunctionEnd

; ============================================================
; PAGE: COMPLETION
; ============================================================
Function PageComplete_Show
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 14u "Quarm Docker Server is installed and running."
    Pop $0

    ${NSD_CreateLabel} 0 20u 100% 10u "Step 1 - Configure your EQ client eqhost.txt:"
    Pop $0

    StrCpy $1 "Not found - copy the text below to eqhost.txt in your TAKP client folder."
    IfFileExists "C:\TAKP\eqhost.txt"       complete_found_eqhost
    IfFileExists "C:\EverQuest\eqhost.txt"  complete_found_eqhost
    IfFileExists "C:\Games\TAKP\eqhost.txt" complete_found_eqhost
    Goto complete_no_eqhost
    complete_found_eqhost:
    StrCpy $1 "Found! Click the button to write your server address automatically."
    complete_no_eqhost:

    ${NSD_CreateLabel} 8u 32u 90% 12u "$1"
    Pop $0
    ${NSD_CreateButton} 8u 46u 190u 16u "Write eqhost.txt Automatically"
    Pop $0
    ${NSD_OnClick} $0 Complete_WriteEQHost

    ${NSD_CreateLabel} 0 68u 100% 10u "eqhost.txt content:"
    Pop $0
    ${NSD_CreateText} 0 80u 100% 56u \
        "[Registration Servers]$\r$\n{$\r$\n$\"$SelectedIP:6000$\"$\r$\n}$\r$\n[Login Servers]$\r$\n{$\r$\n$\"$SelectedIP:6000$\"$\r$\n}"
    Pop $0
    SendMessage $0 ${EM_SETREADONLY} 1 0

    ${NSD_CreateLabel} 0 142u 100% 10u "Step 2 - Make yourself a GM:"
    Pop $0
    ${NSD_CreateLabel} 8u 154u 90% 12u \
        "Open Quarm Docker Server, go to the Admin Tools tab, use Make GM."
    Pop $0
    ${NSD_CreateLabel} 0 172u 100% 12u \
        "A desktop shortcut to Quarm Docker Server has been created."
    Pop $0
    ${NSD_CreateLabel} 0 188u 100% 14u \
        "Setup log: $INSTDIR\install.log (include this if you need support)"
    Pop $0

    nsDialogs::Show
FunctionEnd

; Callback must be a top-level Function (not a label inside PageComplete_Show)
Function Complete_WriteEQHost
    StrCpy $0 "[Registration Servers]$\n{$\n$\"$SelectedIP:6000$\"$\n}$\n[Login Servers]$\n{$\n$\"$SelectedIP:6000$\"$\n}$\n"
    IfFileExists "C:\TAKP\eqhost.txt"       complete_write_takp
    IfFileExists "C:\EverQuest\eqhost.txt"  complete_write_eq
    IfFileExists "C:\Games\TAKP\eqhost.txt" complete_write_games
    nsDialogs::SelectFolderDialog "Find your TAKP client folder" "C:\"
    Pop $1
    ${If} $1 != error
        FileOpen $2 "$1\eqhost.txt" w
        FileWrite $2 $0
        FileClose $2
        MessageBox MB_OK "eqhost.txt written to $1"
    ${EndIf}
    Return
    complete_write_takp:
        FileOpen $2 "C:\TAKP\eqhost.txt" w
        FileWrite $2 $0
        FileClose $2
        MessageBox MB_OK "eqhost.txt written to C:\TAKP\"
        Return
    complete_write_eq:
        FileOpen $2 "C:\EverQuest\eqhost.txt" w
        FileWrite $2 $0
        FileClose $2
        MessageBox MB_OK "eqhost.txt written to C:\EverQuest\"
        Return
    complete_write_games:
        FileOpen $2 "C:\Games\TAKP\eqhost.txt" w
        FileWrite $2 $0
        FileClose $2
        MessageBox MB_OK "eqhost.txt written to C:\Games\TAKP\"
FunctionEnd

Function PageComplete_Leave
FunctionEnd

; ============================================================
; SECTION: INSTALL
; Required for NSIS to produce installer+uninstaller binaries.
; The actual file work is in PageDownload_Leave above.
; ============================================================
Section "Main" SecMain
    SetOutPath "$INSTDIR"
    WriteRegStr HKLM "${APP_KEY}" "InstallDir" "$INSTDIR"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

; ============================================================
; UNINSTALLER
; ============================================================
Var UnDelVolume

Function un.UnPageConfirm_Show
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 44u \
        "This will uninstall Quarm Docker Server.$\r$\n$\r$\n\
WILL be removed:$\r$\n\
  Desktop and Start Menu shortcuts$\r$\n\
  Windows Firewall rules added by this installer$\r$\n\
  Installation folder and all project files$\r$\n\
  Registry entries (auto-start, Add/Remove Programs)"
    Pop $0

    ${NSD_CreateLabel} 0 48u 100% 26u \
        "Will NOT be removed by default:$\r$\n\
  Docker image (quarm-server)$\r$\n\
  quarm-data volume - this contains all your character data$\r$\n\
  Docker Desktop"
    Pop $0

    ${NSD_CreateCheckbox} 0 78u 100% 14u \
        "Also delete quarm-data volume (ALL character data will be permanently lost)"
    Pop $UnDelVolume

    nsDialogs::Show
FunctionEnd

Function un.UnPageConfirm_Leave
    ${NSD_GetState} $UnDelVolume $1
    IntCmp $1 ${BST_CHECKED} un_is_checked un_not_checked un_not_checked
    un_is_checked:
    MessageBox MB_OKCANCEL|MB_ICONSTOP|MB_DEFBUTTON2 "FINAL WARNING$\n$\nYou are about to permanently delete all character data.$\nThis CANNOT be undone.$\n$\nClick OK to confirm deletion." IDOK un_del_confirmed
    SendMessage $UnDelVolume ${BM_SETCHECK} ${BST_UNCHECKED} 0
    Abort
    un_del_confirmed:
    WriteRegDWORD HKCU ${APP_KEY} "UndelVolume" 1
    Return
    un_not_checked:
    WriteRegDWORD HKCU ${APP_KEY} "UndelVolume" 0
FunctionEnd

Section "Uninstall"
    SetOutPath "$INSTDIR"
    nsExec::ExecToStack 'docker compose down'
    Pop $0

    ReadRegDWORD $0 HKCU ${APP_KEY} "UndelVolume"
    ${If} $0 = 1
        nsExec::ExecToStack 'docker volume rm quarm-data'
        Pop $0
        nsExec::ExecToStack 'docker rmi quarm-server'
        Pop $0
    ${EndIf}

    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "@(''Quarm-Login-UDP'',''Quarm-Login-TCP'',''Quarm-World-UDP'',''Quarm-World-TCP'', \
        ''Quarm-UCS-UDP'',''Quarm-DB-TCP'',''Quarm-Zones-UDP'',''Quarm-Zones-TCP'') | \
        ForEach-Object { Remove-NetFirewallRule -DisplayName $$_ -EA SilentlyContinue }"'
    Pop $0

    DeleteRegValue HKCU ${AUTORUN_KEY} "QuarmDockerServer"
    DeleteRegKey HKLM ${UNINSTALL_KEY}
    DeleteRegKey HKLM ${APP_KEY}
    DeleteRegKey HKCU ${APP_KEY}

    Delete "$DESKTOP\Quarm Docker Server.lnk"
    RMDir /r "$SMPROGRAMS\Quarm Docker Server"
    SetOutPath "$TEMP"
    RMDir /r "$INSTDIR"

    SetAutoClose true
SectionEnd
