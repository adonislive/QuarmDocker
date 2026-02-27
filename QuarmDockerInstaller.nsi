; ============================================================
; QuarmDockerInstaller.nsi
; QuarmDocker Windows Installer
; Requires: NSIS 3.x with MUI2 and inetc plugin
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
!define IMAGE_PRIMARY  "adonislive/quarm:latest"
!define IMAGE_FALLBACK "adonislive/quarm:latest"

; ============================================================
; VARIABLES
; ============================================================
Var Dialog
Var InstallMode         ; 0=prebuilt  1=build from source
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
Var Ctrl_RadioPrebuilt
Var Ctrl_RadioSource
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
Page custom PageInstPath_Show PageInstPath_Leave
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
  - Download or build the Quarm server image$\r$\n\
  - Configure Windows Firewall and Defender$\r$\n\
  - Create a desktop shortcut to Quarm Server Manager$\r$\n$\r$\n\
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
  - WSL2 (will be installed automatically if missing)$\r$\n\
  - Docker Desktop (will be downloaded if missing)$\r$\n\
  - Docker engine is running$\r$\n$\r$\n\
If WSL2 needs to be installed, a reboot will be required.$\r$\n\
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

    ; --- Check 4: WSL2 ---
    ; wsl --list returns 0 if WSL feature is enabled (even with no distros)
    ; wsl --status is unreliable - returns non-zero on many systems even when WSL is present
    nsExec::ExecToStack 'wsl --list'
    Pop $0
    Pop $1
    ${If} $0 != 0
        nsExec::ExecToStack 'wsl --install --no-distribution'
        Pop $0
        Pop $1
        !insertmacro LogLine "WSL2 install exit: $0"
        FileClose $InstallLogHandle
        MessageBox MB_OK|MB_ICONINFORMATION \
            "WSL2 has been installed.$\n$\n\
Please restart your computer and run this installer again.$\n\
Your selections do not need to be re-entered."
        Quit
    ${Else}
        !insertmacro LogLine "PASS: WSL2 present"
    ${EndIf}

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
    MessageBox MB_OK|MB_ICONEXCLAMATION "Docker Desktop is not running.$\n$\nPlease open Docker Desktop, wait for it to start, then click Next again."
    FileClose $InstallLogHandle
    Abort
    prereq_docker_ok:
    !insertmacro LogLine "PASS: Docker engine running"

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
        "Start Quarm Server Manager automatically with Windows"
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
; PAGE: INSTALLATION PATH CHOICE
; ============================================================
Function PageInstPath_Show
    nsDialogs::Create 1018
    Pop $Dialog
    ${If} $Dialog == error
        Abort
    ${EndIf}

    ${NSD_CreateLabel} 0 0 100% 14u "How would you like to get the Quarm server?"
    Pop $0

    ${NSD_CreateRadioButton} 8u 18u 100% 14u \
        "Download pre-built server (recommended) - about 5 minutes"
    Pop $Ctrl_RadioPrebuilt
    ${NSD_SetState} $Ctrl_RadioPrebuilt ${BST_CHECKED}
    ${NSD_CreateLabel} 20u 34u 85% 12u \
        "Downloads a ready-to-run image. Stable, tested, no compilation needed."
    Pop $0

    ${NSD_CreateRadioButton} 8u 52u 100% 14u \
        "Build from source - 30 to 45 minutes (advanced users)"
    Pop $Ctrl_RadioSource
    ${NSD_CreateLabel} 20u 68u 85% 24u \
        "Compiles the latest Quarm server code from SecretsOTheP/EQMacEmu.$\r$\n\
For users who want the most current version."
    Pop $0

    ${NSD_CreateLabel} 0 106u 100% 36u \
        "Note: A pre-built image is a completely valid choice.$\r$\n\
Many users run a stable known-good image indefinitely.$\r$\n\
Build-from-source is available for those who want the absolute latest code."
    Pop $0

    nsDialogs::Show
FunctionEnd

Function PageInstPath_Leave
    ${NSD_GetState} $Ctrl_RadioSource $0
    ${If} $0 = ${BST_CHECKED}
        StrCpy $InstallMode 1
        !insertmacro LogLine "Mode: build from source"
    ${Else}
        StrCpy $InstallMode 0
        !insertmacro LogLine "Mode: pre-built image"
    ${EndIf}
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
  - Configure WSL2 memory settings$\r$\n\
  - Add Windows Firewall rules$\r$\n\
  - Add Windows Defender exclusions$\r$\n\
  - Copy project files$\r$\n\
  - Download or build the server image$\r$\n\
  - Start the server$\r$\n$\r$\n\
If building from source this will take 30-45 minutes.$\r$\nDo not close this window."
    Pop $Ctrl_DetailText

    nsDialogs::Show
FunctionEnd

; ============================================================
; PAGE: DOWNLOAD / BUILD (Leave - all install work happens here)
; ============================================================
Function PageDownload_Leave

    ; ---- WSL2 memory configuration ----
    ; Write .wslconfig directly - simpler and more reliable than PowerShell regex merge
    ${NSD_SetText} $Ctrl_ProgressText "Configuring WSL2 memory settings..."
    StrCpy $1 "$WSL2_Memory_MB"
    StrCpy $1 "$1MB"
    FileOpen $0 "$PROFILE\.wslconfig" w
    FileWrite $0 "[wsl2]$\r$\n"
    FileWrite $0 "memory=$1$\r$\n"
    FileWrite $0 "processors=$WSL2_Processors$\r$\n"
    FileClose $0
    !insertmacro LogLine "wslconfig written: memory=$1  processors=$WSL2_Processors"

    nsExec::ExecToStack 'wsl --shutdown'
    Pop $0
    !insertmacro LogLine "wsl --shutdown exit: $0"
    Sleep 3000

    nsExec::ExecToStack 'docker context use desktop-linux'
    Pop $0

    ; Wait for Docker to come back up
    ${NSD_SetText} $Ctrl_ProgressText "Waiting for Docker to restart..."
    StrCpy $WaitCounter 0
    dl_wait_docker:
        nsExec::ExecToStack 'docker info'
        Pop $0
        Pop $1
        IntCmp $0 0 dl_docker_ready
        IntOp $WaitCounter $WaitCounter + 1
        IntCmp $WaitCounter 20 dl_show_retry dl_keep_waiting dl_show_retry
        dl_show_retry:
        MessageBox MB_OK|MB_ICONEXCLAMATION "Docker is taking a long time to restart.$\nPlease wait for Docker to start, then click Next again."
        Abort
        dl_keep_waiting:
        Sleep 3000
        Goto dl_wait_docker
    dl_docker_ready:
    !insertmacro LogLine "Docker ready"

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

    ; ---- Download or Build ----
    ; Use IntCmp instead of ${If}/${Else} so MessageBox jumps work correctly
    IntCmp $InstallMode 0 dl_prebuilt dl_prebuilt dl_buildsource

    dl_prebuilt:
    ${NSD_SetText} $Ctrl_ProgressText "Downloading server image (about 5 minutes)..."
    !insertmacro LogLine "Pulling: ${IMAGE_PRIMARY}"
    nsExec::ExecToStack 'docker pull ${IMAGE_PRIMARY}'
    Pop $0
    Pop $1
    !insertmacro LogLine "docker pull primary exit: $0"
    IntCmp $0 0 dl_tag_primary
    ${NSD_SetText} $Ctrl_ProgressText "Primary source failed. Trying alternate..."
    !insertmacro LogLine "Trying fallback: ${IMAGE_FALLBACK}"
    nsExec::ExecToStack 'docker pull ${IMAGE_FALLBACK}'
    Pop $0
    Pop $1
    !insertmacro LogLine "docker pull fallback exit: $0"
    IntCmp $0 0 dl_tag_fallback
    MessageBox MB_OK|MB_ICONSTOP "Failed to download the server image.$\n$\nCheck your internet connection.$\nLog: $InstallLogPath"
    Abort
    dl_tag_fallback:
    nsExec::ExecToStack 'docker tag ${IMAGE_FALLBACK} quarm-server'
    Pop $0
    Goto dl_image_done
    dl_tag_primary:
    nsExec::ExecToStack 'docker tag ${IMAGE_PRIMARY} quarm-server'
    Pop $0
    Goto dl_image_done

    dl_buildsource:
    ; Check port 3306 conflict
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
    ${NSD_SetText} $Ctrl_ProgressText "Building from source (30-45 minutes)..."
    ${NSD_SetText} $Ctrl_DetailText "Compiling the Quarm server. This takes 30-45 minutes.$\r$\nDO NOT CLOSE THIS WINDOW.$\r$\nThe window will appear frozen - this is normal."
    !insertmacro LogLine "Starting docker compose build..."
    SetOutPath "$INSTDIR"
    nsExec::ExecToStack 'docker compose build'
    Pop $0
    Pop $1
    FileWrite $InstallLogHandle "$1"
    !insertmacro LogLine "docker compose build exit: $0"
    IntCmp $0 0 dl_build_ok
    MessageBox MB_OKCANCEL|MB_ICONSTOP "Build failed.$\n$\nClick OK to retry - Docker resumes from where it stopped.$\n(The build cache preserves completed steps.)$\nLog: $InstallLogPath" IDOK dl_retry_build
    Abort
    dl_retry_build:
    nsExec::ExecToStack 'docker compose build'
    Pop $0
    Pop $1
    FileWrite $InstallLogHandle "$1"
    !insertmacro LogLine "Retry build exit: $0"
    IntCmp $0 0 dl_build_ok
    MessageBox MB_OK|MB_ICONSTOP "Build failed again.$\nLog: $InstallLogPath"
    Abort
    dl_build_ok:

    dl_image_done:

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
    CreateShortcut "$DESKTOP\Quarm Server Manager.lnk" "$INSTDIR\QuarmDockerServer.exe"
    CreateDirectory "$SMPROGRAMS\Quarm Docker Server"
    CreateShortcut "$SMPROGRAMS\Quarm Docker Server\Quarm Server Manager.lnk" \
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
        "Open Quarm Server Manager, go to the Advanced tab, use Make GM."
    Pop $0
    ${NSD_CreateLabel} 0 172u 100% 12u \
        "A desktop shortcut to Quarm Server Manager has been created."
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

    Delete "$DESKTOP\Quarm Server Manager.lnk"
    RMDir /r "$SMPROGRAMS\Quarm Docker Server"
    SetOutPath "$TEMP"
    RMDir /r "$INSTDIR"

    SetAutoClose true
SectionEnd
