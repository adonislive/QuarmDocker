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
;   Copy inetc.dll to C:\Program Files (x86)\NSIS\Plugins\x86-unicode\
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
!define DOCKER_URL    "https://desktop.docker.com/win/main/amd64/Docker%20Desktop%20Installer.exe"

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
  - Install Docker Desktop if needed$\r$\n\
  - Build the Quarm server image from source$\r$\n\
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
  - Windows 10 or later$\r$\n\
  - 64-bit (x64) Windows$\r$\n\
  - 8 GB RAM minimum, 16 GB recommended$\r$\n\
  - Docker Desktop (will be downloaded if missing)$\r$\n\
  - Docker engine is running$\r$\n$\r$\n\
If Docker Desktop needs to be installed, a reboot may be required.$\r$\n\
Just run this installer again after rebooting."
    Pop $0

    nsDialogs::Show
FunctionEnd

Function PagePrereq_Leave
    ; Generate datestamped log filename
    nsExec::ExecToStack 'powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HHmm"'
    Pop $0
    Pop $1
    StrCpy $1 $1 -2   ; trim trailing CR LF
    StrCpy $InstallLogPath "$TEMP\QuarmDocker_install_$1.log"
    FileOpen $InstallLogHandle $InstallLogPath w
    !insertmacro LogLine "QuarmDocker Installer Log"
    !insertmacro LogLine "========================="
    !insertmacro LogLine "Log file: $InstallLogPath"

    ; --- Check 0: Admin elevation ---
    nsExec::ExecToStack 'net session'
    Pop $0
    Pop $1
    ${If} $0 != 0
        !insertmacro LogLine "FAIL: Not running as administrator"
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONSTOP \
            "This installer must be run as Administrator.$\n$\nRight-click the installer and choose 'Run as administrator'."
        Abort
    ${EndIf}
    !insertmacro LogLine "PASS: Running as administrator"

    ; --- Check 1: Windows version ---
    ${If} ${AtLeastWin10}
        !insertmacro LogLine "PASS: Windows 10+"
    ${Else}
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONSTOP \
            "Windows 10 or later is required.$\nPlease upgrade Windows and try again."
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

    ; --- Check 3: RAM ---
    nsExec::ExecToStack 'powershell -NoProfile -Command \
        "[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1MB)"'
    Pop $0
    Pop $SystemRAM_MB
    StrCpy $SystemRAM_MB $SystemRAM_MB -2   ; trim trailing CR LF

    ; Hard stop below 8 GB
    ${If} $SystemRAM_MB < 8000
        !insertmacro LogLine "FAIL: RAM $SystemRAM_MB MB (below 8 GB minimum)"
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONSTOP \
            "Your computer has $SystemRAM_MB MB of RAM.$\n$\n\
8 GB minimum is required to run the Quarm server.$\n\
16 GB or more is recommended."
        Abort
    ${EndIf}

    ; Soft warning between 8-16 GB
    ; Use IntCmp instead of ${If} — MessageBox jump labels break inside ${If} blocks
    IntCmp $SystemRAM_MB 16000 prereq_ram_ok prereq_ram_warn prereq_ram_ok
    prereq_ram_warn:
    !insertmacro LogLine "WARNING: RAM $SystemRAM_MB MB (below 16 GB recommended)"
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "Your computer has $SystemRAM_MB MB of RAM.$\n$\n16 GB or more is recommended for best experience.$\n$\nClick OK to continue, or Cancel to exit." IDOK prereq_ram_continue
    FileClose $InstallLogHandle
    Abort
    prereq_ram_continue:
    prereq_ram_ok:

    ; Calculate WSL2 memory cap (75% of system RAM, clamped 2048-8192)
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

    ; --- Check 3b: Write .wslconfig (before Docker starts) ---
    ; Write directly - simpler and more reliable than PowerShell regex merge
    ; Values take effect next time WSL starts
    StrCpy $1 "$WSL2_Memory_MB"
    StrCpy $1 "$1MB"
    FileOpen $0 "$PROFILE\.wslconfig" w
    FileWrite $0 "[wsl2]$\r$\n"
    FileWrite $0 "memory=$1$\r$\n"
    FileWrite $0 "processors=$WSL2_Processors$\r$\n"
    FileClose $0
    !insertmacro LogLine "wslconfig written: memory=$1  processors=$WSL2_Processors"

    ; --- Check 4 REMOVED: Docker Desktop handles all WSL2 setup ---
    ; Docker Desktop Installer.exe install --accept-license
    ; enables WSL2 features, installs kernel, handles reboot if needed.
    ; docker info succeeding proves WSL2 works. No independent WSL check needed.

    ; --- Check 5: Docker Desktop installed ---
    StrCpy $0 ""
    IfFileExists "$PROGRAMFILES64\Docker\Docker\Docker Desktop.exe" prereq_docker_found
    IfFileExists "$PROGRAMFILES\Docker\Docker\Docker Desktop.exe" prereq_docker_found
    ReadRegStr $0 HKLM "Software\Docker Inc.\Docker Desktop" "InstallDir"
    ${If} $0 != ""
        Goto prereq_docker_found
    ${EndIf}
    ReadRegStr $0 HKCU "Software\Docker Inc.\Docker Desktop" "InstallDir"
    ${If} $0 != ""
        Goto prereq_docker_found
    ${EndIf}
    nsExec::ExecToStack 'cmd /C where docker'
    Pop $0
    Pop $1
    ${If} $0 == 0
        Goto prereq_docker_found
    ${EndIf}

    ; Docker not found - ask user before downloading
    !insertmacro LogLine "Docker Desktop not found"
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "Docker Desktop was not found on this computer.$\n$\nDocker Desktop is required to run the Quarm server.$\nClick OK to download and install it (about 500 MB).$\n$\nClick Cancel to exit if you want to install it yourself first." IDOK prereq_docker_download
    FileClose $InstallLogHandle
    Abort
    prereq_docker_download:
    !insertmacro LogLine "User approved Docker download, starting..."
    MessageBox MB_OK|MB_ICONINFORMATION "Docker Desktop will now be downloaded and installed.$\n$\nThis may take several minutes. A progress window will appear.$\nPlease wait for it to complete."
    inetc::get "${DOCKER_URL}" "$TEMP\DockerDesktopInstaller.exe" /END
    Pop $0
    StrCmp $0 "OK" 0 prereq_docker_dl_fail
    !insertmacro LogLine "Docker Desktop downloaded, launching installer..."
    MessageBox MB_OK|MB_ICONINFORMATION "Download complete. Docker Desktop installer will now run.$\n$\nThis takes several minutes. Please wait for it to finish.$\nIf it asks you to reboot, reboot and run this installer again."
    ExecWait '"$TEMP\DockerDesktopInstaller.exe" install --accept-license' $0
    !insertmacro LogLine "Docker Desktop install exit: $0"
    Delete "$TEMP\DockerDesktopInstaller.exe"
    Goto prereq_docker_found
    prereq_docker_dl_fail:
    !insertmacro LogLine "Docker download failed: $0"
    FileClose $InstallLogHandle
    MessageBox MB_OK|MB_ICONSTOP "Failed to download Docker Desktop.$\n$\nPlease download it manually from https://www.docker.com/products/docker-desktop$\nand run this installer again."
    Abort

    prereq_docker_found:
    !insertmacro LogLine "PASS: Docker Desktop present"

    ; --- Check 6: Docker engine running ---
    nsExec::ExecToStack 'docker info'
    Pop $0
    Pop $1
    ${If} $0 == 0
        Goto prereq_docker_running
    ${EndIf}

    ; Docker not running - tell user to open it or reboot if just installed
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
        "Docker Desktop is not running.$\n$\n\
If Docker Desktop was just installed, you may need to reboot first.$\n\
Otherwise, please open Docker Desktop and wait for it to start.$\n$\n\
Click OK to retry, or Cancel to exit." IDOK prereq_retry_docker
    FileClose $InstallLogHandle
    Abort

    prereq_retry_docker:
    StrCpy $WaitCounter 0
    prereq_docker_wait:
        nsExec::ExecToStack 'docker info'
        Pop $0
        Pop $1
        IntCmp $0 0 prereq_docker_running
        IntOp $WaitCounter $WaitCounter + 1
        IntCmp $WaitCounter 20 prereq_docker_giveup prereq_docker_keepwait prereq_docker_giveup
        prereq_docker_keepwait:
        Sleep 3000
        Goto prereq_docker_wait
    prereq_docker_giveup:
    FileClose $InstallLogHandle
    MessageBox MB_OK|MB_ICONSTOP \
        "Docker Desktop did not start in time.$\n$\n\
If Docker Desktop was just installed, please reboot your computer$\n\
and run this installer again.$\n$\n\
If Docker Desktop was already installed, please open it manually,$\n\
wait for it to finish starting, then run this installer again."
    Abort

    prereq_docker_running:
    !insertmacro LogLine "PASS: Docker engine running"

    ; --- Check 7: Docker context ---
    nsExec::ExecToStack 'docker context use desktop-linux'
    Pop $0
    !insertmacro LogLine "docker context use desktop-linux: exit $0"

FunctionEnd

; ============================================================
; HELPER FUNCTIONS FOR CONFIG PAGE
; ============================================================
Function Config_BrowseDir
    nsDialogs::SelectFolderDialog "Select installation directory" $INSTDIR
    Pop $0
    ${If} $0 != error
        ${NSD_SetText} $Ctrl_DirText $0
    ${EndIf}
FunctionEnd

Function Config_ShowAdapters
    ShowWindow $Ctrl_AdapterLabel ${SW_SHOW}
    ShowWindow $Ctrl_AdapterList  ${SW_SHOW}
    nsExec::ExecToStack "powershell -NoProfile -Command \
        \"Get-NetIPAddress -AddressFamily IPv4 | \
        Where-Object { $$_.IPAddress -ne '127.0.0.1' } | \
        ForEach-Object { \
            $$a = (Get-NetAdapter -InterfaceIndex $$_.InterfaceIndex -EA SilentlyContinue).Name; \
            if($$a) { $$a + ' - ' + $$_.IPAddress } \
        } | Out-File -FilePath '$TEMP\qd_adapters.txt' -Encoding UTF8\""
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
; PAGE: DOWNLOAD / BUILD (Show - informational only)
; ============================================================
Function PageDownload_Show
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 14u "Installing..."
    Pop $Ctrl_ProgressText

    ${NSD_CreateLabel} 0 18u 100% 160u \
        "Click Next to begin the installation.$\r$\n$\r$\n\
The installer will:$\r$\n\
  - Add Windows Firewall rules$\r$\n\
  - Add Windows Defender exclusions$\r$\n\
  - Copy project files$\r$\n\
  - Build the server image from source$\r$\n\
  - Start the server$\r$\n$\r$\n\
Building from source takes 40-55 minutes.$\r$\nYou will see build progress on the next screen."
    Pop $Ctrl_DetailText

    nsDialogs::Show
FunctionEnd

; ============================================================
; PAGE: DOWNLOAD / BUILD (Leave - pre-build work happens here)
; Build itself moved to Section "Main" for instfiles progress.
; ============================================================
Function PageDownload_Leave

    ; ---- Pre-build Docker readiness check ----
    ${NSD_SetText} $Ctrl_ProgressText "Verifying Docker is ready..."
    StrCpy $WaitCounter 0
    dl_prebuild_wait:
        nsExec::ExecToStack 'docker info'
        Pop $0
        Pop $1
        IntCmp $0 0 dl_prebuild_ready
        IntOp $WaitCounter $WaitCounter + 1
        IntCmp $WaitCounter 20 dl_prebuild_fail dl_prebuild_keepwait dl_prebuild_fail
        dl_prebuild_keepwait:
        Sleep 3000
        Goto dl_prebuild_wait
    dl_prebuild_fail:
    MessageBox MB_OK|MB_ICONSTOP \
        "Docker is not responding.$\nPlease make sure Docker Desktop is running and try again."
    Abort
    dl_prebuild_ready:
    !insertmacro LogLine "Docker ready (pre-build check)"

    nsExec::ExecToStack 'docker context use desktop-linux'
    Pop $0

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

    ${NSD_SetText} $Ctrl_ProgressText "Files copied. Click Next to build the server."
    ${NSD_SetText} $Ctrl_DetailText "The build takes 40-55 minutes. You will see progress on the next screen."
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

    ${NSD_CreateLabel} 0 20u 100% 10u "Step 1 - Copy text below into eqhost.txt in TAKP folder:"
    Pop $0
    nsDialogs::CreateControl EDIT \
        ${DEFAULT_STYLES}|${WS_TABSTOP}|${ES_MULTILINE}|${WS_VSCROLL} \
        ${WS_EX_WINDOWEDGE} \
        0 30u 100% 64u \
        "[Registration Servers]$\r$\n{$\r$\n$\"$SelectedIP:6000$\"$\r$\n}$\r$\n[Login Servers]$\r$\n{$\r$\n$\"$SelectedIP:6000$\"$\r$\n}"
    Pop $0
    SendMessage $0 ${EM_SETREADONLY} 1 0

    ${NSD_CreateLabel} 0 96u 100% 10u "Step 2 - Open Quarm Docker Server from the desktop shortcut, go to Admin Tools."
    Pop $0
    ${NSD_CreateLabel} 0 112u 100% 14u \
        "Setup log: $INSTDIR\install.log (include this if you need support)"
    Pop $0

    nsDialogs::Show
FunctionEnd

Function PageComplete_Leave
FunctionEnd

; ============================================================
; SECTION: INSTALL
; Build, start, shortcuts, registry — runs on the instfiles page
; so docker compose build output streams via nsExec::ExecToLog.
; ============================================================
Section "Main" SecMain
    SetOutPath "$INSTDIR"

    ; ---- Start install timer ----
    System::Call 'kernel32::GetTickCount() i .s'
    Pop $7

    ; ---- Check port 3306 conflict ----
    DetailPrint "Checking for port 3306 conflict..."
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
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "Port 3306 is in use (process: $2).$\n$\nThis port is needed by the Quarm server database.$\nClick OK to stop and disable the conflicting service automatically." IDOK dl_stop_mysql
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

    ; ---- Build from source ----
    DetailPrint "Building from source (40-55 minutes)..."
    DetailPrint "Docker build output will appear below."
    !insertmacro LogLine "Starting docker compose build..."
    Delete "$TEMP\docker_output.log"

    ; Build attempt 1
    nsExec::ExecToLog "powershell -NoProfile -Command $\"docker compose build 2>&1 | Tee-Object -Append -FilePath '$TEMP\docker_output.log'; exit $$LASTEXITCODE$\""
    Pop $0
    !insertmacro LogLine "docker compose build exit: $0"
    IntCmp $0 0 dl_build_ok

    ; Build attempt 2
    MessageBox MB_OKCANCEL|MB_ICONSTOP "Build failed.$\n$\nClick OK to retry - Docker resumes from where it stopped.$\n(The build cache preserves completed steps.)$\nLog: $InstallLogPath" IDOK dl_retry_build_2
    Abort
    dl_retry_build_2:
    !insertmacro LogLine "Retry build attempt 2..."
    nsExec::ExecToLog "powershell -NoProfile -Command $\"docker compose build 2>&1 | Tee-Object -Append -FilePath '$TEMP\docker_output.log'; exit $$LASTEXITCODE$\""
    Pop $0
    !insertmacro LogLine "Retry build 2 exit: $0"
    IntCmp $0 0 dl_build_ok

    ; Build attempt 3
    MessageBox MB_OKCANCEL|MB_ICONSTOP "Build failed again.$\n$\nClick OK to try one more time.$\nDocker will resume from where it stopped.$\n$\nIf this keeps failing, check the log at:$\n$InstallLogPath" IDOK dl_retry_build_3
    Abort
    dl_retry_build_3:
    !insertmacro LogLine "Retry build attempt 3..."
    nsExec::ExecToLog "powershell -NoProfile -Command $\"docker compose build 2>&1 | Tee-Object -Append -FilePath '$TEMP\docker_output.log'; exit $$LASTEXITCODE$\""
    Pop $0
    !insertmacro LogLine "Retry build 3 exit: $0"
    IntCmp $0 0 dl_build_ok
    MessageBox MB_OK|MB_ICONSTOP "Build failed after 3 attempts.$\n$\nLog: $InstallLogPath"
    Abort

    dl_build_ok:

    ; ---- First Start ----
    DetailPrint "Starting the server..."
    nsExec::ExecToLog "powershell -NoProfile -Command $\"docker compose up -d 2>&1 | Tee-Object -Append -FilePath '$TEMP\docker_output.log'; exit $$LASTEXITCODE$\""
    Pop $0
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

    ; Append docker output to install log
    !insertmacro LogLine "=== Docker output ==="
    FileClose $InstallLogHandle
    nsExec::ExecToStack 'cmd /c type "$TEMP\docker_output.log" >> "$InstallLogPath"'
    Pop $0
    Pop $1
    FileOpen $InstallLogHandle $InstallLogPath a

    ; ---- Report install time ----
    System::Call 'kernel32::GetTickCount() i .s'
    Pop $8
    IntOp $9 $8 - $7
    IntOp $9 $9 / 1000
    IntOp $R0 $9 / 60
    IntOp $R1 $9 % 60
    !insertmacro LogLine "Total install time: $R0 minutes $R1 seconds"
    !insertmacro LogLine "=== Installation complete ==="
    FileClose $InstallLogHandle
    CopyFiles /SILENT "$InstallLogPath" "$INSTDIR\install.log"

    DetailPrint "Installation complete."
    DetailPrint "Total install time: $R0 minutes $R1 seconds"
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
