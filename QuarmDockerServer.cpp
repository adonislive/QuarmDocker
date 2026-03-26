// ============================================================
// QuarmDockerServer.cpp
// Quarm Docker Server - Native Win32 x64 Launcher
//
// Compile (MSVC x64 Developer Command Prompt):
//   cl /O2 /W3 /EHsc /std:c++17 /DUNICODE /D_UNICODE ^
//      /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 ^
//      QuarmDockerServer.cpp /Fe:QuarmDockerServer.exe ^
//      /link /SUBSYSTEM:WINDOWS /MACHINE:X64 ^
//      user32.lib gdi32.lib comctl32.lib shell32.lib ^
//      shlwapi.lib comdlg32.lib advapi32.lib
// ============================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <filesystem>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' " \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "resource.h"

namespace fs = std::filesystem;

// ============================================================
// CONSTANTS
// ============================================================

static const wchar_t* APP_CLASS   = L"QuarmServerManager";
static const wchar_t* APP_TITLE   = L"Quarm Docker Server";
static const wchar_t* PANEL_CLASS = L"QSMPanel";
static const wchar_t* CONTAINER   = L"quarm-server";
static const int      NUM_TABS    = 8;
static const int      POLL_MS     = 5000;

// Tab labels
static const wchar_t* TAB_LABELS[NUM_TABS] = {
    L"Status", L"Admin Tools", L"Player Tools",
    L"Backup & Restore", L"Log Viewer", L"Network", L"Advanced",
    L"Game Tools"
};

// Panel index constants for readability
#define TAB_STATUS   0
#define TAB_ADMIN    1
#define TAB_PLAYER   2
#define TAB_BACKUP   3
#define TAB_LOG      4
#define TAB_NETWORK  5
#define TAB_ADVANCED 6
#define TAB_GAME     7

// --- Game Tools tab control IDs (add to resource.h) ---
#define IDC_GAME_ITEM_SEARCH     5001
#define IDC_BTN_ITEM_SEARCH      5002
#define IDC_GAME_ITEM_ID         5003
#define IDC_GAME_CHAR_NAME       5004
#define IDC_BTN_GIVE_ITEM        5005
#define IDC_GAME_RESULT          5006
#define IDC_GAME_ERA_COMBO       5007
#define IDC_BTN_SET_ERA          5008
#define IDC_GAME_ZONE_COMBO      5009
#define IDC_BTN_SET_ZONE_COUNT   5010
#define IDC_GAME_ERA_CURRENT     5011
#define IDC_GAME_ZONE_CURRENT    5012

// Process name translation table
struct ProcEntry { const char* proc; const wchar_t* label; };
static const ProcEntry PROC_TABLE[] = {
    { "mariadbd",    L"Database     " },
    { "loginserver", L"Login Server " },
    { "world",       L"World Server " },
    { "eqlaunch",    L"Zone Launcher" },
    { "queryserv",   L"Query Server " },
    { "ucs",         L"Chat Server  " },
    { nullptr, nullptr }
};

// Registry key for auto-start
static const wchar_t* AUTOSTART_KEY =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* AUTOSTART_NAME = L"QuarmDockerServer";

// ============================================================
// GLOBAL STATE
// ============================================================

static HINSTANCE g_hInst       = nullptr;
static HWND      g_hwndMain    = nullptr;
static HWND      g_hwndTab     = nullptr;
static HWND      g_hwndStatus  = nullptr;   // status bar
static HWND      g_hwndPanels[NUM_TABS] = {};
static HFONT     g_hFont       = nullptr;
static HFONT     g_hFontBold   = nullptr;
static HFONT     g_hFontMono   = nullptr;

static wchar_t   g_installDir[MAX_PATH] = {};
static bool      g_serverRunning  = false;
static bool      g_operationBusy  = false;
static std::atomic<bool> g_stopPolling{ false };

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), n);
    if (!ws.empty() && ws.back() == 0) ws.pop_back();
    return ws;
}

static std::string RunCommand(const std::wstring& cmd,
                               const std::wstring& workDir = L"",
                               DWORD* exitCode = nullptr) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return "";
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput  = hWrite;
    si.hStdError   = hWrite;
    si.hStdInput   = INVALID_HANDLE_VALUE;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring cmdBuf = cmd;
    LPCWSTR wd = workDir.empty() ? nullptr : workDir.c_str();

    bool ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, wd, &si, &pi) != 0;
    CloseHandle(hWrite);

    std::string result;
    if (ok) {
        char buf[8192];
        DWORD bytes = 0;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytes, nullptr) && bytes > 0) {
            buf[bytes] = 0;
            result += buf;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        if (exitCode) GetExitCodeProcess(pi.hProcess, exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hRead);
    return result;
}

static void RunCommandStreaming(const std::wstring& cmd,
                                 const std::wstring& workDir,
                                 std::function<void(const std::string&)> lineCallback,
                                 DWORD* exitCode = nullptr) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput  = hWrite;
    si.hStdError   = hWrite;
    si.hStdInput   = INVALID_HANDLE_VALUE;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring cmdBuf = cmd;
    LPCWSTR wd = workDir.empty() ? nullptr : workDir.c_str();

    bool ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr,
                             TRUE, CREATE_NO_WINDOW, nullptr, wd, &si, &pi) != 0;
    CloseHandle(hWrite);

    if (ok) {
        char buf[8192];
        DWORD bytes = 0;
        std::string partial;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytes, nullptr) && bytes > 0) {
            buf[bytes] = 0;
            partial += buf;
            size_t pos;
            while ((pos = partial.find('\n')) != std::string::npos) {
                lineCallback(partial.substr(0, pos));
                partial = partial.substr(pos + 1);
            }
        }
        if (!partial.empty()) lineCallback(partial);
        WaitForSingleObject(pi.hProcess, INFINITE);
        if (exitCode) GetExitCodeProcess(pi.hProcess, exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hRead);
}

static std::string TrimRight(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
        s.pop_back();
    return s;
}

// Normalize line endings for Win32 edit controls (\n -> \r\n)
static std::wstring NormalizeNewlines(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + s.size() / 10);
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == L'\n' && (i == 0 || s[i-1] != L'\r'))
            out += L'\r';
        out += s[i];
    }
    return out;
}

// Run a mariadb query inside the container, return result as wstring
static std::wstring RunQuery(const std::wstring& sql) {
    std::wstring cmd = std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -e \"" + sql + L"\" quarm";
    std::string out = TrimRight(RunCommand(cmd));
    if (out.empty()) return L"(no results)";
    return NormalizeNewlines(ToWide(out));
}

// Compute SHA1 of a UTF-8 string using Windows CryptoAPI
// Returns lowercase hex string, or empty on failure
static std::wstring ComputeSHA1Hex(const std::wstring& input) {
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1,
                                      nullptr, 0, nullptr, nullptr);
    std::string utf8(utf8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1,
                        utf8.data(), utf8len, nullptr, nullptr);
    if (!utf8.empty() && utf8.back() == '\0') utf8.pop_back();

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv, nullptr, nullptr,
                             PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return L"";
    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return L"";
    }
    CryptHashData(hHash, (BYTE*)utf8.data(), (DWORD)utf8.size(), 0);
    BYTE hash[20];
    DWORD hashLen = 20;
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    wchar_t hex[41];
    for (int i = 0; i < 20; i++)
        swprintf_s(hex + i*2, 3, L"%02x", hash[i]);
    return hex;
}

static std::wstring GetServerAddress() {
    wchar_t envPath[MAX_PATH];
    wcscpy_s(envPath, g_installDir);
    PathAppendW(envPath, L".env");
    std::ifstream f(envPath);
    if (!f) return L"127.0.0.1";
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("SERVER_ADDRESS=", 0) == 0)
            return ToWide(line.substr(15));
    }
    return L"127.0.0.1";
}

static bool SetServerAddress(const std::wstring& ip) {
    wchar_t envPath[MAX_PATH];
    wcscpy_s(envPath, g_installDir);
    PathAppendW(envPath, L".env");
    std::ofstream f(envPath);
    if (!f) return false;
    f << "SERVER_ADDRESS=" << std::string(ip.begin(), ip.end()) << "\n";
    return true;
}

static bool IsContainerRunning() {
    std::wstring cmd = L"docker inspect -f {{.State.Status}} ";
    cmd += CONTAINER;
    std::string out = TrimRight(RunCommand(cmd));
    return out == "running";
}

static std::wstring GetUptimeString() {
    std::wstring cmd = L"docker inspect -f {{.State.StartedAt}} ";
    cmd += CONTAINER;
    std::string started = TrimRight(RunCommand(cmd));
    if (started.empty() || started.find("Error") != std::string::npos)
        return L"";
    std::wstring ps = L"powershell -NoProfile -Command \""
        L"$s=[datetime]::Parse('" + ToWide(started) + L"').ToLocalTime();"
        L"$d=(Get-Date)-$s;"
        L"if($d.TotalHours -ge 1){"
        L"'{0}h {1}m' -f [int]$d.TotalHours,$d.Minutes"
        L"}elseif($d.TotalMinutes -ge 1){"
        L"'{0}m' -f [int]$d.TotalMinutes"
        L"}else{'just started'}\"";
    std::string out = TrimRight(RunCommand(ps));
    return ToWide(out);
}

static std::wstring GetProcessStatus() {
    std::string psOut = RunCommand(
        std::wstring(L"docker exec ") + CONTAINER + L" ps -eo comm");
    if (psOut.empty() || psOut.find("Error") != std::string::npos)
        return L"(container not accessible)";

    int zoneCount = 0;
    std::istringstream ss(psOut);
    std::string proc;
    std::vector<bool> found(6, false);
    while (std::getline(ss, proc)) {
        proc = TrimRight(proc);
        if (proc == "zone") { zoneCount++; continue; }
        for (int i = 0; PROC_TABLE[i].proc != nullptr; ++i) {
            if (proc == PROC_TABLE[i].proc && !found[i])
                found[i] = true;
        }
    }

    std::wstring result;
    for (int i = 0; PROC_TABLE[i].proc != nullptr; ++i) {
        result += std::wstring(L"  ") + PROC_TABLE[i].label +
                  L"  " + (found[i] ? L"RUNNING" : L"down") + L"\r\n";
    }
    result += L"  Zone Processes    " +
              (zoneCount > 0 ? std::to_wstring(zoneCount) + L" running" : L"none")
              + L"\r\n";
    return result;
}

static std::vector<std::wstring> GetBackupFiles(const wchar_t* prefix = L"backup_") {
    wchar_t backupDir[MAX_PATH];
    wcscpy_s(backupDir, g_installDir);
    PathAppendW(backupDir, L"config\\backups");
    std::vector<std::wstring> files;
    WIN32_FIND_DATAW fd;
    wchar_t pattern[MAX_PATH];
    wcscpy_s(pattern, backupDir);
    PathAppendW(pattern, (std::wstring(prefix) + L"*.sql").c_str());
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return files;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            files.push_back(fd.cFileName);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    std::sort(files.rbegin(), files.rend());
    return files;
}

static std::wstring FileSizeStr(const wchar_t* filename) {
    wchar_t path[MAX_PATH];
    wcscpy_s(path, g_installDir);
    PathAppendW(path, L"config\\backups");
    PathAppendW(path, filename);
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return L"";
    ULONGLONG sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    if (sz >= 1024 * 1024) return std::to_wstring(sz / (1024*1024)) + L" MB";
    if (sz >= 1024) return std::to_wstring(sz / 1024) + L" KB";
    return std::to_wstring(sz) + L" B";
}

static std::wstring GetDateStamp() {
    std::string ds = TrimRight(RunCommand(
        L"powershell -NoProfile -Command \"Get-Date -Format yyyy-MM-dd_HHmm\""));
    return ToWide(ds);
}

static void ApplyFont(HWND hwnd, HFONT hFont) {
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
        SendMessage(child, WM_SETFONT, (WPARAM)(HFONT)lp, TRUE);
        return TRUE;
    }, (LPARAM)hFont);
}

static HWND MakeLabel(HWND parent, const wchar_t* text,
                       int x, int y, int w, int h, DWORD extra = 0) {
    return CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT | extra,
        x, y, w, h, parent, nullptr, g_hInst, nullptr);
}

static HWND MakeButton(HWND parent, const wchar_t* text, int id,
                        int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        x, y, w, h, parent, (HMENU)(UINT_PTR)id, g_hInst, nullptr);
}

static HWND MakeEdit(HWND parent, int id, int x, int y, int w, int h,
                      DWORD extra = 0) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | extra,
        x, y, w, h, parent, (HMENU)(UINT_PTR)id, g_hInst, nullptr);
}

static HWND MakeCheck(HWND parent, const wchar_t* text, int id,
                       int x, int y, int w, int h) {
    return CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
        x, y, w, h, parent, (HMENU)(UINT_PTR)id, g_hInst, nullptr);
}

static HWND MakeListBox(HWND parent, int id, int x, int y, int w, int h) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
        x, y, w, h, parent, (HMENU)(UINT_PTR)id, g_hInst, nullptr);
}

static HWND MakeCombo(HWND parent, int id, int x, int y, int w, int h) {
    return CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP,
        x, y, w, h, parent, (HMENU)(UINT_PTR)id, g_hInst, nullptr);
}

// Make a read-only multiline edit with monospace font for query results
static HWND MakeResultBox(HWND parent, int id, int x, int y, int w, int h) {
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY |
        WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL,
        x, y, w, h, parent, (HMENU)(UINT_PTR)id, g_hInst, nullptr);
    if (hw && g_hFontMono)
        SendMessage(hw, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
    return hw;
}

struct AsyncResult {
    bool success;
    std::wstring message;
};

#define WM_ASYNC_DONE  (WM_USER + 1)
#define WM_STATUS_POLL (WM_USER + 2)

// ============================================================
// BUSY STATE
// ============================================================

static void SetBusy(bool busy) {
    g_operationBusy = busy;
    if (g_hwndPanels[TAB_STATUS]) {
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_STATUS], IDC_BTN_START), !busy);
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_STATUS], IDC_BTN_STOP),  !busy);
    }
    if (g_hwndPanels[TAB_BACKUP]) {
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_BACKUP], IDC_BTN_BACKUP_NOW),   !busy);
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_BACKUP], IDC_BTN_RESTORE),      !busy);
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_BACKUP], IDC_BTN_EXPORT_CHARS), !busy);
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_BACKUP], IDC_BTN_IMPORT_CHARS), !busy);
    }
    if (g_hwndPanels[TAB_ADVANCED]) {
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BTN_REBUILD),     !busy);
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BTN_START_FRESH), !busy);
    }
    HWND hwndLog = GetDlgItem(g_hwndPanels[TAB_LOG], IDC_BTN_LOAD_LOG);
    if (hwndLog) EnableWindow(hwndLog, !busy);
}

static void SetStatus(const wchar_t* text) {
    if (g_hwndStatus)
        SendMessage(g_hwndStatus, SB_SETTEXT, 0, (LPARAM)text);
}

// ============================================================
// TAB 1 — STATUS PANEL
// ============================================================

static HWND g_hwndStateLabel   = nullptr;
static HWND g_hwndUptimeLabel  = nullptr;
static HWND g_hwndProcList     = nullptr;

static void CreateStatusPanel(HWND parent) {
    MakeLabel(parent, L"Server State:", 20, 20, 120, 20);
    g_hwndStateLabel = MakeLabel(parent, L"Checking...", 150, 20, 300, 22, SS_SUNKEN);

    MakeLabel(parent, L"Uptime:", 20, 52, 120, 20);
    g_hwndUptimeLabel = MakeLabel(parent, L"", 150, 52, 300, 20);

    MakeLabel(parent, L"Services:", 20, 84, 120, 20);
    g_hwndProcList = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        150, 84, 420, 150, parent,
        (HMENU)(UINT_PTR)IDC_STATUS_PROCESSES, g_hInst, nullptr);
    if (g_hwndProcList && g_hFontMono)
        SendMessage(g_hwndProcList, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

    MakeButton(parent, L"Start Server", IDC_BTN_START, 20, 256, 120, 34);
    MakeButton(parent, L"Stop Server",  IDC_BTN_STOP,  160, 256, 120, 34);

    MakeLabel(parent,
        L"The server continues running in Docker whether this window is open or closed.",
        20, 306, 580, 20);
}

static void RefreshStatusTab() {
    bool running = IsContainerRunning();
    g_serverRunning = running;

    if (running) {
        SetWindowTextW(g_hwndStateLabel, L"RUNNING");
        std::wstring uptime = GetUptimeString();
        SetWindowTextW(g_hwndUptimeLabel, uptime.empty() ? L"" : uptime.c_str());
        std::wstring procs = GetProcessStatus();
        SetWindowTextW(g_hwndProcList, procs.c_str());
        SetStatus(L"Server is running");
    } else {
        SetWindowTextW(g_hwndStateLabel, L"STOPPED");
        SetWindowTextW(g_hwndUptimeLabel, L"");
        SetWindowTextW(g_hwndProcList, L"(server is not running)");
        SetStatus(L"Server is stopped");
    }
}

// ============================================================
// TAB 2 — ADMIN TOOLS PANEL
// ============================================================

static HWND g_hwndAdmAccount  = nullptr;
static HWND g_hwndAdmPassword = nullptr;
static HWND g_hwndAdmResult   = nullptr;

static void CreateAdminPanel(HWND parent) {
    int y = 12;

    MakeLabel(parent, L"Account Management:", 20, y, 160, 20);
    y += 26;

    MakeLabel(parent, L"Account:", 20, y+4, 70, 20);
    g_hwndAdmAccount = MakeEdit(parent, IDC_ADM_ACCOUNT, 96, y, 190, 24);
    MakeButton(parent, L"Make GM",        IDC_BTN_MAKE_GM,       296, y, 90, 26);
    MakeButton(parent, L"Remove GM",      IDC_BTN_REMOVE_GM,     396, y, 95, 26);
    y += 34;

    MakeButton(parent, L"List Accounts",  IDC_BTN_LIST_ACCOUNTS, 96,  y, 120, 26);
    MakeButton(parent, L"Reset Password", IDC_BTN_RESET_PASSWORD,226, y, 130, 26);
    y += 34;

    MakeLabel(parent, L"New Password:", 20, y+4, 100, 20);
    g_hwndAdmPassword = MakeEdit(parent, IDC_ADM_PASSWORD, 128, y, 200, 24, ES_PASSWORD);
    MakeLabel(parent, L"(for Reset Password above)", 338, y+4, 200, 20);
    y += 40;

    // Separator
    MakeLabel(parent, L"Player & Session Info:", 20, y, 180, 20);
    y += 26;

    MakeButton(parent, L"Who Is Online",    IDC_BTN_WHO_ONLINE,    20, y, 120, 26);
    MakeButton(parent, L"Recent Logins",    IDC_BTN_RECENT_LOGINS, 150, y, 120, 26);
    MakeButton(parent, L"Last Known IP",    IDC_BTN_IP_HISTORY,    280, y, 130, 26);
    MakeLabel(parent, L"(uses account name above)", 420, y+5, 200, 20);
    y += 40;

    g_hwndAdmResult = MakeResultBox(parent, IDC_ADM_RESULT, 20, y, 730, 220);
}

// ============================================================
// ADMIN TOOL OPERATIONS
// ============================================================

static void SetAdmResult(const std::wstring& text) {
    if (g_hwndAdmResult)
        SetWindowTextW(g_hwndAdmResult, text.c_str());
}

static bool AdmGetAccount(wchar_t* buf, int bufLen) {
    GetWindowTextW(g_hwndAdmAccount, buf, bufLen);
    if (!buf[0]) {
        MessageBoxW(g_hwndMain, L"Enter an account name first.",
            L"Account Required", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    return true;
}

static bool CheckServerRunning(const wchar_t* title) {
    if (!IsContainerRunning()) {
        MessageBoxW(g_hwndMain, L"Server must be running to use this feature.",
            title, MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
}

// Forward declarations for functions defined later
static bool GetNoBackupOnStop();

// Check if a character's account is currently active (logged in)
// Returns: 1 = online, 0 = offline, -1 = character not found or error
static int IsCharacterOnline(const wchar_t* charName) {
    std::wstring sql =
        L"SELECT a.active FROM account a "
        L"JOIN character_data cd ON cd.account_id=a.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(charName) + L"')";
    std::wstring result = RunQuery(sql);
    if (result == L"(no results)" || result.find(L"ERROR") != std::wstring::npos)
        return -1;
    if (result.find(L"1") != std::wstring::npos)
        return 1;
    return 0;
}

// Restart server (stop + start) used by Move online and Era/Zone changes
static void DoRestartServerAsync() {
    if (g_operationBusy) return;
    bool noBackup = GetNoBackupOnStop();
    SetBusy(true);
    SetStatus(L"Restarting server...");
    std::thread([noBackup]{
        if (!noBackup) {
            wchar_t bd[MAX_PATH]; wcscpy_s(bd, g_installDir);
            PathAppendW(bd, L"config\\backups");
            CreateDirectoryW(bd, nullptr);
            std::wstring ds = GetDateStamp();
            wchar_t ff[MAX_PATH]; wcscpy_s(ff, bd);
            PathAppendW(ff, (L"backup_" + ds + L".sql").c_str());
            std::wstring cmd = L"cmd /c docker exec " + std::wstring(CONTAINER) +
                L" mariadb-dump quarm > \"" + std::wstring(ff) + L"\"";
            RunCommand(cmd, g_installDir);
        }
        RunCommand(L"docker compose down", g_installDir);
        RunCommand(L"docker compose up -d", g_installDir);
        auto* res = new AsyncResult{ true, L"Server restarted." };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_STATUS, (LPARAM)res);
    }).detach();
}

static void DoMakeGM() {
    wchar_t acct[128] = {};
    if (!AdmGetAccount(acct, 128)) return;
    if (!CheckServerRunning(L"Make GM")) return;

    std::wstring sql = L"UPDATE account SET status=255 WHERE LOWER(name)=LOWER('" +
        std::wstring(acct) + L"')";
    RunQuery(sql);
    SetAdmResult(std::wstring(L"GM status set for: ") + acct +
        L"\r\n\r\nNote: status=255 means GM. Account must log out and back in for the change to take effect.\r\n"
        L"If no rows changed, the account name may not exist - use List Accounts to verify.");
}

static void DoRemoveGM() {
    wchar_t acct[128] = {};
    if (!AdmGetAccount(acct, 128)) return;
    if (!CheckServerRunning(L"Remove GM")) return;

    std::wstring sql = L"UPDATE account SET status=0 WHERE LOWER(name)=LOWER('" +
        std::wstring(acct) + L"')";
    RunQuery(sql);
    SetAdmResult(std::wstring(L"GM status removed for: ") + acct +
        L"\r\n\r\nNote: Account must log out and back in for the change to take effect.\r\n"
        L"If no rows changed, the account name may not exist - use List Accounts to verify.");
}

static void DoListAccounts() {
    if (!CheckServerRunning(L"List Accounts")) return;
    std::wstring sql =
        L"SELECT a.name, a.status, lsa.LastLoginDate, lsa.LastIPAddress "
        L"FROM account a "
        L"LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name "
        L"ORDER BY lsa.LastLoginDate DESC";
    std::wstring result = RunQuery(sql);
    SetAdmResult(L"Accounts (status 255=GM, 0=player):\r\n\r\n" + result);
}

static void DoResetPassword() {
    wchar_t acct[128] = {};
    if (!AdmGetAccount(acct, 128)) return;
    if (!CheckServerRunning(L"Reset Password")) return;

    wchar_t pass[256] = {};
    GetWindowTextW(g_hwndAdmPassword, pass, 256);
    if (!pass[0]) {
        MessageBoxW(g_hwndMain, L"Enter a new password in the 'New Password' field.",
            L"Password Required", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring hash = ComputeSHA1Hex(pass);
    if (hash.empty()) {
        SetAdmResult(L"ERROR: Failed to compute password hash.");
        return;
    }

    // Clear the password field immediately after reading
    SetWindowTextW(g_hwndAdmPassword, L"");

    std::wstring sql = L"UPDATE tblLoginServerAccounts SET AccountPassword='" + hash +
        L"' WHERE LOWER(AccountName)=LOWER('" + std::wstring(acct) + L"')";
    RunQuery(sql);
    SetAdmResult(std::wstring(L"Password reset for: ") + acct +
        L"\r\n\r\nNote: Account must log out and back in for the change to take effect.\r\n"
        L"If no rows changed, the account name may not exist - use List Accounts to verify.");
}

static void DoWhoIsOnline() {
    if (!CheckServerRunning(L"Who Is Online")) return;
    std::wstring sql =
        L"SELECT cd.name, cd.level, "
        L"CASE cd.class WHEN 1 THEN 'WAR' WHEN 2 THEN 'CLR' WHEN 3 THEN 'PAL' "
        L"WHEN 4 THEN 'RNG' WHEN 5 THEN 'SHD' WHEN 6 THEN 'DRU' "
        L"WHEN 7 THEN 'MNK' WHEN 8 THEN 'BRD' WHEN 9 THEN 'ROG' "
        L"WHEN 10 THEN 'SHM' WHEN 11 THEN 'NEC' WHEN 12 THEN 'WIZ' "
        L"WHEN 13 THEN 'MAG' WHEN 14 THEN 'ENC' WHEN 15 THEN 'BST' "
        L"ELSE CAST(cd.class AS CHAR) END as class, "
        L"z.short_name AS zone "
        L"FROM character_data cd "
        L"LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id "
        L"WHERE cd.last_login > UNIX_TIMESTAMP(NOW() - INTERVAL 1 DAY) "
        L"ORDER BY cd.last_login DESC";
    std::wstring result = RunQuery(sql);
    SetAdmResult(L"Characters active in last 24 hours:\r\n\r\n" + result);
}

static void DoRecentLogins() {
    if (!CheckServerRunning(L"Recent Logins")) return;
    std::wstring sql =
        L"SELECT a.name, lsa.LastLoginDate, lsa.LastIPAddress "
        L"FROM account a "
        L"LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name "
        L"ORDER BY lsa.LastLoginDate DESC LIMIT 20";
    std::wstring result = RunQuery(sql);
    SetAdmResult(L"Recent logins (last 20):\r\n\r\n" + result);
}

static void DoIPHistory() {
    wchar_t acct[128] = {};
    if (!AdmGetAccount(acct, 128)) return;
    if (!CheckServerRunning(L"Last Known IP")) return;

    std::wstring sql =
        L"SELECT a.name, lsa.LastIPAddress, lsa.LastLoginDate "
        L"FROM account a "
        L"LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name "
        L"WHERE LOWER(a.name)=LOWER('" + std::wstring(acct) + L"')";
    std::wstring result = RunQuery(sql);
    SetAdmResult(std::wstring(L"Last known IP for: ") + acct +
        L"\r\n(only the most recent IP is stored per account)\r\n\r\n" + result);
}

// ============================================================
// TAB 3 — PLAYER TOOLS PANEL
// ============================================================

static HWND g_hwndPlrAccount  = nullptr;
static HWND g_hwndPlrCharName = nullptr;
static HWND g_hwndPlrZone     = nullptr;
static HWND g_hwndPlrAmount   = nullptr;
static HWND g_hwndPlrResult   = nullptr;

static void CreatePlayerPanel(HWND parent) {
    int y = 12;

    MakeLabel(parent, L"Character Lookup:", 20, y, 160, 20);
    y += 26;

    MakeLabel(parent, L"Account:", 20, y+4, 70, 20);
    g_hwndPlrAccount = MakeEdit(parent, IDC_PLR_ACCOUNT, 96, y, 180, 24);
    MakeButton(parent, L"List Characters",    IDC_BTN_PLR_LIST_CHARS, 286, y, 130, 26);
    MakeButton(parent, L"Acct for Char",      IDC_BTN_SHOW_ACCT_CHAR, 426, y, 120, 26);
    y += 34;

    MakeLabel(parent, L"Character:", 20, y+4, 70, 20);
    g_hwndPlrCharName = MakeEdit(parent, IDC_PLR_CHARNAME, 96, y, 180, 24);
    MakeButton(parent, L"Char Info",          IDC_BTN_PLR_CHAR_INFO,  286, y, 90,  26);
    MakeButton(parent, L"Inventory",          IDC_BTN_SHOW_INVENTORY, 386, y, 80,  26);
    MakeButton(parent, L"Currency",           IDC_BTN_SHOW_CURRENCY,  476, y, 80,  26);
    y += 40;

    MakeLabel(parent, L"Player Actions:", 20, y, 120, 20);
    y += 26;

    MakeButton(parent, L"Move to Bind Point", IDC_BTN_MOVE_TO_BIND, 20,  y, 150, 26);
    MakeLabel(parent, L"Zone:", 180, y+4, 40, 20);
    g_hwndPlrZone = MakeEdit(parent, IDC_PLR_ZONE, 224, y, 140, 24);
    MakeButton(parent, L"Find Zone",          IDC_BTN_FIND_ZONE,    374, y, 80,  26);
    MakeButton(parent, L"Move to Zone",       IDC_BTN_MOVE_TO_ZONE, 464, y, 110, 26);
    y += 34;

    MakeLabel(parent, L"Platinum:", 20, y+4, 70, 20);
    g_hwndPlrAmount = MakeEdit(parent, IDC_PLR_AMOUNT, 96, y, 80, 24);
    MakeButton(parent, L"Give Platinum",      IDC_BTN_GIVE_PLAT,    186, y, 120, 26);
    MakeLabel(parent, L"(add to character's carried platinum)", 316, y+4, 300, 20);
    y += 40;

    MakeLabel(parent, L"Corpses:", 20, y, 80, 20);
    y += 26;

    MakeButton(parent, L"List All Corpses",     IDC_BTN_LIST_CORPSES,    20,  y, 140, 26);
    MakeButton(parent, L"Corpses by Character", IDC_BTN_CORPSES_BY_CHAR, 170, y, 160, 26);
    MakeLabel(parent, L"(uses character name above)", 340, y+5, 220, 20);
    y += 40;

    g_hwndPlrResult = MakeResultBox(parent, IDC_PLR_RESULT, 20, y, 730, 190);
}

// ============================================================
// PLAYER TOOL OPERATIONS
// ============================================================

static void SetPlrResult(const std::wstring& text) {
    if (g_hwndPlrResult)
        SetWindowTextW(g_hwndPlrResult, text.c_str());
}

static bool PlrGetChar(wchar_t* buf, int bufLen) {
    GetWindowTextW(g_hwndPlrCharName, buf, bufLen);
    if (!buf[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name first.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return false;
    }
    return true;
}

static void DoPlrListChars() {
    wchar_t acct[128] = {};
    GetWindowTextW(g_hwndPlrAccount, acct, 128);
    if (!acct[0]) {
        MessageBoxW(g_hwndMain, L"Enter an account name first.",
            L"Account Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!CheckServerRunning(L"List Characters")) return;

    std::wstring sql =
        L"SELECT cd.name, cd.level, "
        L"CASE cd.class WHEN 1 THEN 'WAR' WHEN 2 THEN 'CLR' WHEN 3 THEN 'PAL' "
        L"WHEN 4 THEN 'RNG' WHEN 5 THEN 'SHD' WHEN 6 THEN 'DRU' "
        L"WHEN 7 THEN 'MNK' WHEN 8 THEN 'BRD' WHEN 9 THEN 'ROG' "
        L"WHEN 10 THEN 'SHM' WHEN 11 THEN 'NEC' WHEN 12 THEN 'WIZ' "
        L"WHEN 13 THEN 'MAG' WHEN 14 THEN 'ENC' WHEN 15 THEN 'BST' "
        L"ELSE CAST(cd.class AS CHAR) END as class, "
        L"z.short_name AS zone "
        L"FROM character_data cd "
        L"JOIN account a ON cd.account_id=a.id "
        L"LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id "
        L"WHERE LOWER(a.name)=LOWER('" + std::wstring(acct) + L"') "
        L"ORDER BY cd.name";
    std::wstring result = RunQuery(sql);
    SetPlrResult(std::wstring(L"Characters on account '") + acct + L"':\r\n\r\n" + result);
}

static void DoPlrCharInfo() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    if (!CheckServerRunning(L"Char Info")) return;

    std::wstring sql =
        L"SELECT cd.name, cd.level, "
        L"CASE cd.class WHEN 1 THEN 'WAR' WHEN 2 THEN 'CLR' WHEN 3 THEN 'PAL' "
        L"WHEN 4 THEN 'RNG' WHEN 5 THEN 'SHD' WHEN 6 THEN 'DRU' "
        L"WHEN 7 THEN 'MNK' WHEN 8 THEN 'BRD' WHEN 9 THEN 'ROG' "
        L"WHEN 10 THEN 'SHM' WHEN 11 THEN 'NEC' WHEN 12 THEN 'WIZ' "
        L"WHEN 13 THEN 'MAG' WHEN 14 THEN 'ENC' WHEN 15 THEN 'BST' "
        L"ELSE CAST(cd.class AS CHAR) END as class, "
        L"z.short_name AS zone, cd.cur_hp, cd.mana, cd.endurance, "
        L"cd.str, cd.sta, cd.agi, cd.dex, cd.wis, cd.cha, "
        L"cd.exp, cd.aa_points, cd.aa_points_spent, "
        L"CONCAT(FLOOR(cd.time_played/3600), 'h ', FLOOR((cd.time_played MOD 3600)/60), 'm') AS time_played, "
        L"FROM_UNIXTIME(cd.last_login) AS last_login "
        L"FROM character_data cd "
        L"LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring result = RunQuery(sql);
    SetPlrResult(std::wstring(L"Character info for '") + chr + L"':\r\n\r\n" + result);
}

static void DoShowInventory() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    if (!CheckServerRunning(L"Show Inventory")) return;

    std::wstring sql =
        L"SELECT ci.slotid, i.name, ci.charges "
        L"FROM character_inventory ci "
        L"JOIN items i ON i.id=ci.itemid "
        L"JOIN character_data cd ON cd.id=ci.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"') "
        L"ORDER BY ci.slotid";
    std::wstring result = RunQuery(sql);
    SetPlrResult(std::wstring(L"Inventory for '") + chr +
        L"' (slots 0-21 are equipped):\r\n\r\n" + result);
}

static void DoShowCurrency() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    if (!CheckServerRunning(L"Show Currency")) return;

    std::wstring sql =
        L"SELECT cc.platinum, cc.gold, cc.silver, cc.copper, "
        L"cc.platinum_bank, cc.gold_bank, cc.silver_bank, cc.copper_bank, "
        L"cc.platinum_cursor, cc.gold_cursor, cc.silver_cursor, cc.copper_cursor "
        L"FROM character_currency cc "
        L"JOIN character_data cd ON cd.id=cc.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring result = RunQuery(sql);
    SetPlrResult(std::wstring(L"Currency for '") + chr + L"':\r\n\r\n" + result);
}

static void DoShowAcctForChar() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    if (!CheckServerRunning(L"Acct for Char")) return;

    std::wstring sql =
        L"SELECT a.name AS account, a.status, cd.name AS character_name, cd.level, "
        L"CASE cd.class WHEN 1 THEN 'WAR' WHEN 2 THEN 'CLR' WHEN 3 THEN 'PAL' "
        L"WHEN 4 THEN 'RNG' WHEN 5 THEN 'SHD' WHEN 6 THEN 'DRU' "
        L"WHEN 7 THEN 'MNK' WHEN 8 THEN 'BRD' WHEN 9 THEN 'ROG' "
        L"WHEN 10 THEN 'SHM' WHEN 11 THEN 'NEC' WHEN 12 THEN 'WIZ' "
        L"WHEN 13 THEN 'MAG' WHEN 14 THEN 'ENC' WHEN 15 THEN 'BST' "
        L"ELSE CAST(cd.class AS CHAR) END as class "
        L"FROM character_data cd "
        L"JOIN account a ON cd.account_id=a.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring result = RunQuery(sql);
    SetPlrResult(std::wstring(L"Account for character '") + chr + L"':\r\n\r\n" + result);
}

static void DoMoveToBind() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    if (!CheckServerRunning(L"Move to Bind")) return;

    // Show current location first
    std::wstring infoSql =
        L"SELECT cd.name, z.short_name AS current_zone, z2.short_name AS bind_zone "
        L"FROM character_data cd "
        L"LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id "
        L"LEFT JOIN character_bind cb ON cb.id=cd.id AND cb.is_home=0 "
        L"LEFT JOIN zone z2 ON z2.zoneidnumber=cb.zone_id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring info = RunQuery(infoSql);
    SetPlrResult(info);

    int online = IsCharacterOnline(chr);
    if (online == -1) {
        SetPlrResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }

    if (online == 1) {
        // Option C: character is online — offer camp-first or restart
        int r = MessageBoxW(g_hwndMain,
            (std::wstring(L"Move '") + chr + L"' to their bind point?\r\n\r\n" +
             info + L"\r\n\r\n"
             L"Character is currently ONLINE. You can either:\r\n\r\n"
             L"  YES = Move in DB + Restart server (all players disconnect briefly)\r\n"
             L"  NO  = Cancel (have them camp to char select first, then try again)\r\n").c_str(),
            L"Character Is Online", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (r != IDYES) {
            SetPlrResult(L"Cancelled. Have the character camp to character select first, then click Move to Bind again.");
            return;
        }
        // Proceed with DB update + restart
        std::wstring moveSql =
            L"UPDATE character_data cd "
            L"JOIN character_bind cb ON cb.id=cd.id "
            L"JOIN zone z ON z.zoneidnumber=cb.zone_id "
            L"SET cd.zone_id=cb.zone_id, cd.x=cb.x, cd.y=cb.y, cd.z=cb.z, cd.heading=cb.heading "
            L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"') AND cb.is_home=0";
        RunQuery(moveSql);
        SetPlrResult(std::wstring(L"'") + chr + L"' bind point set in DB. Restarting server...");
        DoRestartServerAsync();
        return;
    }

    // Offline — proceed normally
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Move '") + chr + L"' to their bind point?\r\n\r\n" +
         info + L"\r\n\r\n"
         L"Character is offline. Continue?").c_str(),
        L"Confirm Move to Bind", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;

    std::wstring moveSql =
        L"UPDATE character_data cd "
        L"JOIN character_bind cb ON cb.id=cd.id "
        L"JOIN zone z ON z.zoneidnumber=cb.zone_id "
        L"SET cd.zone_id=cb.zone_id, cd.x=cb.x, cd.y=cb.y, cd.z=cb.z, cd.heading=cb.heading "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"') AND cb.is_home=0";
    RunQuery(moveSql);
    SetPlrResult(std::wstring(L"'") + chr + L"' moved to bind point. They can now log in safely.");
}

static void DoFindZone() {
    wchar_t zone[128] = {};
    GetWindowTextW(g_hwndPlrZone, zone, 128);
    if (!zone[0]) {
        MessageBoxW(g_hwndMain, L"Enter a zone name or partial name to search.",
            L"Zone Search", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!CheckServerRunning(L"Find Zone")) return;

    std::wstring sql =
        L"SELECT short_name, long_name FROM zone "
        L"WHERE LOWER(short_name) LIKE LOWER('%" + std::wstring(zone) + L"%') "
        L"OR LOWER(long_name) LIKE LOWER('%" + std::wstring(zone) + L"%') "
        L"ORDER BY short_name LIMIT 20";
    std::wstring result = RunQuery(sql);
    SetPlrResult(L"Matching zones (enter exact short_name above to move):\r\n\r\n" + result);
}

static void DoMoveToZone() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    wchar_t zone[128] = {};
    GetWindowTextW(g_hwndPlrZone, zone, 128);
    if (!zone[0]) {
        MessageBoxW(g_hwndMain,
            L"Enter an exact zone short name in the Zone field.\r\nUse Find Zone to look up short names.",
            L"Zone Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!CheckServerRunning(L"Move to Zone")) return;

    // Verify the zone exists first
    std::wstring checkSql = L"SELECT short_name, long_name FROM zone WHERE LOWER(short_name)=LOWER('" +
        std::wstring(zone) + L"')";
    std::wstring zoneInfo = RunQuery(checkSql);
    if (zoneInfo == L"(no results)") {
        SetPlrResult(std::wstring(L"Zone '") + zone + L"' not found. Use Find Zone to look up exact short names.");
        return;
    }

    int online = IsCharacterOnline(chr);
    if (online == -1) {
        SetPlrResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }

    if (online == 1) {
        // Option C: character is online — offer camp-first or restart
        int r = MessageBoxW(g_hwndMain,
            (std::wstring(L"Move '") + chr + L"' to zone '" + zone + L"'?\r\n\r\n" +
             zoneInfo + L"\r\n\r\n"
             L"Character is currently ONLINE. You can either:\r\n\r\n"
             L"  YES = Move in DB + Restart server (all players disconnect briefly)\r\n"
             L"  NO  = Cancel (have them camp to char select first, then try again)\r\n").c_str(),
            L"Character Is Online", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (r != IDYES) {
            SetPlrResult(L"Cancelled. Have the character camp to character select first, then click Move to Zone again.");
            return;
        }
        std::wstring moveSql =
            L"UPDATE character_data cd "
            L"JOIN zone z ON LOWER(z.short_name)=LOWER('" + std::wstring(zone) + L"') "
            L"SET cd.zone_id=z.zoneidnumber, cd.x=z.safe_x, cd.y=z.safe_y, cd.z=z.safe_z, cd.heading=z.safe_heading "
            L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
        RunQuery(moveSql);
        SetPlrResult(std::wstring(L"'") + chr + L"' zone set to '" + zone + L"' in DB. Restarting server...");
        DoRestartServerAsync();
        return;
    }

    // Offline — proceed normally
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Move '") + chr + L"' to zone '" + zone + L"'?\r\n\r\n" +
         zoneInfo + L"\r\n\r\n"
         L"Character is offline. Continue?").c_str(),
        L"Confirm Move to Zone", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;

    std::wstring moveSql =
        L"UPDATE character_data cd "
        L"JOIN zone z ON LOWER(z.short_name)=LOWER('" + std::wstring(zone) + L"') "
        L"SET cd.zone_id=z.zoneidnumber, cd.x=z.safe_x, cd.y=z.safe_y, cd.z=z.safe_z, cd.heading=z.safe_heading "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    RunQuery(moveSql);
    SetPlrResult(std::wstring(L"'") + chr + L"' moved to '" + zone + L"'. They can now log in safely.\r\n\r\n"
        L"If no rows changed, check that both names are correct.");
}

static void DoGivePlatinum() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    wchar_t amtStr[32] = {};
    GetWindowTextW(g_hwndPlrAmount, amtStr, 32);
    if (!amtStr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a platinum amount first.",
            L"Amount Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Validate numeric
    for (wchar_t* p = amtStr; *p; p++) {
        if (!iswdigit(*p)) {
            MessageBoxW(g_hwndMain, L"Amount must be a positive whole number.",
                L"Invalid Amount", MB_OK | MB_ICONWARNING);
            return;
        }
    }
    if (!CheckServerRunning(L"Give Platinum")) return;

    // Show current currency
    std::wstring checkSql =
        L"SELECT cc.platinum AS carried, cc.platinum_bank AS banked "
        L"FROM character_currency cc "
        L"JOIN character_data cd ON cd.id=cc.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring current = RunQuery(checkSql);
    SetPlrResult(std::wstring(L"Current platinum for '") + chr + L"':\r\n\r\n" + current);

    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Add ") + amtStr + L" platinum to '" + chr + L"' (carried)?\r\n\r\n" +
         current + L"\r\n\r\n"
         L"WARNING: Character should be LOGGED OUT for this to work reliably.\r\n\r\n"
         L"Continue?").c_str(),
        L"Confirm Give Platinum", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;

    std::wstring giveSql =
        L"UPDATE character_currency cc "
        L"JOIN character_data cd ON cd.id=cc.id "
        L"SET cc.platinum=cc.platinum+" + std::wstring(amtStr) +
        L" WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    RunQuery(giveSql);
    SetPlrResult(std::wstring(L"Added ") + amtStr + L" platinum to '" + chr + L"' (carried).\r\n\r\n"
        L"If no rows changed, the character name may not exist - use List Characters to verify.");
}

static void DoListCorpses() {
    if (!CheckServerRunning(L"List Corpses")) return;
    std::wstring sql =
        L"SELECT cc.charname, z.short_name AS zone, cc.time_of_death, cc.is_rezzed, cc.is_buried "
        L"FROM character_corpses cc "
        L"LEFT JOIN zone z ON z.zoneidnumber=cc.zone_id "
        L"ORDER BY cc.time_of_death DESC";
    std::wstring result = RunQuery(sql);
    SetPlrResult(L"All corpses:\r\n\r\n" + result);
}

static void DoCorpsesByChar() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    if (!CheckServerRunning(L"Corpses by Character")) return;

    std::wstring sql =
        L"SELECT cc.charname, z.short_name AS zone, cc.time_of_death, cc.is_rezzed, cc.is_buried, "
        L"cc.platinum, cc.gold, cc.silver, cc.copper "
        L"FROM character_corpses cc "
        L"LEFT JOIN zone z ON z.zoneidnumber=cc.zone_id "
        L"WHERE LOWER(cc.charname)=LOWER('" + std::wstring(chr) + L"') "
        L"ORDER BY cc.time_of_death DESC";
    std::wstring result = RunQuery(sql);
    SetPlrResult(std::wstring(L"Corpses for '") + chr + L"':\r\n\r\n" + result);
}

// ============================================================
// TAB 4 — BACKUP & RESTORE PANEL
// ============================================================

static HWND g_hwndBackupList = nullptr;
static HWND g_hwndBackupInfo = nullptr;

static void CreateBackupPanel(HWND parent) {
    MakeLabel(parent, L"Backups:", 20, 16, 80, 20);
    g_hwndBackupList = MakeListBox(parent, IDC_BACKUP_LIST, 110, 16, 470, 140);

    MakeButton(parent, L"Backup Now",        IDC_BTN_BACKUP_NOW,   20, 170, 110, 30);
    MakeButton(parent, L"Restore Selected",  IDC_BTN_RESTORE,      20, 210, 130, 30);
    MakeButton(parent, L"Export Characters", IDC_BTN_EXPORT_CHARS, 170, 170, 130, 30);
    MakeButton(parent, L"Import Characters", IDC_BTN_IMPORT_CHARS, 170, 210, 130, 30);

    g_hwndBackupInfo = MakeLabel(parent, L"", 20, 255, 570, 80, SS_LEFT | WS_BORDER);

    MakeLabel(parent,
        L"Restore: select a backup from the list, then click Restore Selected.",
        20, 350, 560, 18);
    MakeLabel(parent,
        L"Export Characters: saves player data only. For transferring to another server version.",
        20, 372, 560, 18);
}

static void RefreshBackupList() {
    HWND lb = g_hwndBackupList;
    if (!lb) return;
    SendMessage(lb, LB_RESETCONTENT, 0, 0);

    auto backups = GetBackupFiles(L"backup_");
    for (auto& f : backups) {
        std::wstring item = f + L"  (" + FileSizeStr(f.c_str()) + L")";
        SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)item.c_str());
    }
    auto chars = GetBackupFiles(L"chars_");
    for (auto& f : chars) {
        std::wstring item = f + L"  (" + FileSizeStr(f.c_str()) + L")  [characters]";
        SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)item.c_str());
    }
    if (SendMessage(lb, LB_GETCOUNT, 0, 0) == 0)
        SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)L"No backups found.");
}

static void DoBackupNow() {
    if (g_operationBusy) return;
    if (!IsContainerRunning()) {
        MessageBoxW(g_hwndMain, L"Server must be running to take a backup.",
            L"Backup", MB_OK | MB_ICONWARNING);
        return;
    }
    SetBusy(true);
    SetStatus(L"Backing up...");
    SetWindowTextW(g_hwndBackupInfo, L"Backing up database, please wait...");

    std::thread([]{
        wchar_t backupDir[MAX_PATH];
        wcscpy_s(backupDir, g_installDir);
        PathAppendW(backupDir, L"config\\backups");
        CreateDirectoryW(backupDir, nullptr);

        std::wstring ds = GetDateStamp();
        std::wstring file = L"config\\backups\\backup_" + ds + L".sql";
        wchar_t fullFile[MAX_PATH];
        wcscpy_s(fullFile, g_installDir);
        PathAppendW(fullFile, file.c_str());

        std::wstring redirectCmd = L"cmd /c docker exec " +
            std::wstring(CONTAINER) + L" mariadb-dump quarm > \"" +
            std::wstring(fullFile) + L"\"";
        DWORD ec = 0;
        RunCommand(redirectCmd, g_installDir, &ec);

        WIN32_FILE_ATTRIBUTE_DATA fad{};
        ULONGLONG sz = 0;
        if (GetFileAttributesExW(fullFile, GetFileExInfoStandard, &fad))
            sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;

        auto* res = new AsyncResult();
        if (sz > 100) {
            res->success = true;
            res->message = L"Backup saved: backup_" + ds + L".sql  (" +
                FileSizeStr((L"backup_" + ds + L".sql").c_str()) + L")";
        } else {
            res->success = false;
            res->message = L"Backup failed: output file is empty.\n"
                L"Check that the server is running correctly.";
            DeleteFileW(fullFile);
        }
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_BACKUP, (LPARAM)res);
    }).detach();
}

static void DoRestore(const std::wstring& filename) {
    if (g_operationBusy) return;
    int r = MessageBoxW(g_hwndMain,
        (L"Restore from:\n\n" + filename + L"\n\n"
         L"This will overwrite all current character data.\n"
         L"The server will be stopped, restored, and restarted.\n\nContinue?").c_str(),
        L"Confirm Restore", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;

    SetBusy(true);
    SetStatus(L"Restoring...");
    SetWindowTextW(g_hwndBackupInfo, L"Stopping server...");

    std::thread([filename]{
        RunCommand(L"docker compose down", g_installDir);
        SetWindowTextW(g_hwndBackupInfo, L"Restoring database...");
        wchar_t fullFile[MAX_PATH];
        wcscpy_s(fullFile, g_installDir);
        PathAppendW(fullFile, L"config\\backups");
        PathAppendW(fullFile, filename.c_str());
        RunCommand(L"docker compose up -d", g_installDir);
        Sleep(8000);
        std::wstring cmd = L"cmd /c docker exec -i " + std::wstring(CONTAINER) +
            L" mariadb quarm < \"" + std::wstring(fullFile) + L"\"";
        DWORD ec = 0;
        RunCommand(cmd, g_installDir, &ec);
        auto* res = new AsyncResult();
        res->success = (ec == 0);
        res->message = ec == 0 ? L"Restore complete. Server is running."
            : L"Restore failed (error code: " + std::to_wstring(ec) + L").\nServer has been restarted.";
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_BACKUP, (LPARAM)res);
    }).detach();
}

static void DoExportCharacters() {
    if (g_operationBusy) return;
    if (!IsContainerRunning()) {
        MessageBoxW(g_hwndMain, L"Server must be running to export characters.",
            L"Export", MB_OK | MB_ICONWARNING);
        return;
    }
    SetBusy(true);
    SetStatus(L"Exporting characters...");
    SetWindowTextW(g_hwndBackupInfo, L"Exporting character data...");

    std::thread([]{
        wchar_t backupDir[MAX_PATH];
        wcscpy_s(backupDir, g_installDir);
        PathAppendW(backupDir, L"config\\backups");
        CreateDirectoryW(backupDir, nullptr);
        std::wstring ds = GetDateStamp();
        std::wstring file = L"chars_" + ds + L".sql";
        wchar_t fullFile[MAX_PATH];
        wcscpy_s(fullFile, backupDir);
        PathAppendW(fullFile, file.c_str());
        std::wstring cmd = L"cmd /c docker exec " + std::wstring(CONTAINER) +
            L" mariadb-dump --replace --tables quarm"
            L" account tblLoginServerAccounts"
            L" character_data character_inventory character_currency"
            L" character_bind character_skills character_spells"
            L" character_languages character_corpses"
            L" > \"" + std::wstring(fullFile) + L"\"";
        DWORD ec = 0;
        RunCommand(cmd, g_installDir, &ec);
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        ULONGLONG sz = 0;
        if (GetFileAttributesExW(fullFile, GetFileExInfoStandard, &fad))
            sz = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
        auto* res = new AsyncResult();
        if (sz > 1000) {
            res->success = true;
            res->message = L"Characters exported: " + file;
        } else {
            res->success = false;
            res->message = L"Export failed: output file is too small.";
            DeleteFileW(fullFile);
        }
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_BACKUP, (LPARAM)res);
    }).detach();
}

static void DoImportCharacters() {
    if (g_operationBusy) return;
    wchar_t szFile[MAX_PATH] = {};
    wchar_t initDir[MAX_PATH];
    wcscpy_s(initDir, g_installDir);
    PathAppendW(initDir, L"config\\backups");
    OPENFILENAMEW ofn{};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = g_hwndMain;
    ofn.lpstrFilter  = L"SQL Files (*.sql)\0*.sql\0All Files\0*.*\0";
    ofn.lpstrFile    = szFile;
    ofn.nMaxFile     = MAX_PATH;
    ofn.lpstrInitialDir = initDir;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = L"Select Character Export File";
    if (!GetOpenFileNameW(&ofn)) return;
    int r = MessageBoxW(g_hwndMain,
        L"Import character data?\n\n"
        L"WARNING: Some items may not transfer if they no longer exist in this server version.\n\nContinue?",
        L"Confirm Import", MB_YESNO | MB_ICONWARNING);
    if (r != IDYES) return;
    std::wstring filepath = szFile;
    SetBusy(true);
    SetStatus(L"Importing characters...");
    SetWindowTextW(g_hwndBackupInfo, L"Importing character data...");
    std::thread([filepath]{
        std::wstring cmd = L"cmd /c docker exec -i " + std::wstring(CONTAINER) +
            L" mariadb quarm < \"" + filepath + L"\"";
        DWORD ec = 0;
        RunCommand(cmd, g_installDir, &ec);
        auto* res = new AsyncResult();
        res->success = (ec == 0);
        res->message = ec == 0
            ? L"Characters imported successfully.\n\nNote: items that no longer exist in this server version will not appear."
            : L"Import failed. Check that the server is running and try again.";
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_BACKUP, (LPARAM)res);
    }).detach();
}

// ============================================================
// TAB 5 — LOG VIEWER PANEL
// ============================================================

static HWND g_hwndLogText  = nullptr;
static HWND g_hwndLogLines = nullptr;

static void CreateLogPanel(HWND parent) {
    MakeLabel(parent, L"Lines:", 20, 20, 50, 22);
    g_hwndLogLines = MakeCombo(parent, IDC_LOG_LINES_COMBO, 75, 17, 80, 120);
    SendMessageW(g_hwndLogLines, CB_ADDSTRING, 0, (LPARAM)L"50");
    SendMessageW(g_hwndLogLines, CB_ADDSTRING, 0, (LPARAM)L"100");
    SendMessageW(g_hwndLogLines, CB_ADDSTRING, 0, (LPARAM)L"500");
    SendMessageW(g_hwndLogLines, CB_ADDSTRING, 0, (LPARAM)L"All");
    SendMessage(g_hwndLogLines, CB_SETCURSEL, 1, 0);
    MakeButton(parent, L"Load Log",    IDC_BTN_LOAD_LOG,    170, 16, 90, 26);
    MakeButton(parent, L"Refresh",     IDC_BTN_REFRESH_LOG, 270, 16, 90, 26);
    g_hwndLogText = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        20, 54, 740, 370,
        parent, (HMENU)(UINT_PTR)IDC_LOG_TEXT, g_hInst, nullptr);
    if (g_hwndLogText && g_hFontMono)
        SendMessage(g_hwndLogText, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
}

static void DoLoadLog() {
    if (g_operationBusy) return;
    int sel = (int)SendMessage(g_hwndLogLines, CB_GETCURSEL, 0, 0);
    std::wstring tailArg;
    switch (sel) {
        case 0: tailArg = L"50";  break;
        case 1: tailArg = L"100"; break;
        case 2: tailArg = L"500"; break;
        case 3: tailArg = L"";    break;
        default: tailArg = L"100";
    }
    SetBusy(true);
    SetStatus(L"Loading log...");
    SetWindowTextW(g_hwndLogText, L"Loading...");
    std::thread([tailArg]{
        std::wstring cmd = std::wstring(L"docker logs ");
        if (!tailArg.empty()) cmd += L"--tail " + tailArg + L" ";
        cmd += CONTAINER;
        std::string out = RunCommand(cmd);
        std::wstring wout = ToWide(out);
        std::wstring norm = NormalizeNewlines(wout);
        auto* res = new AsyncResult();
        res->success = true;
        res->message = norm;
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_LOG, (LPARAM)res);
    }).detach();
}

// ============================================================
// TAB 6 — NETWORK PANEL
// ============================================================

static HWND g_hwndNetCurrent     = nullptr;
static HWND g_hwndNetStatusMsg   = nullptr;
static HWND g_hwndNetAdapterList = nullptr;
static HWND g_hwndEqhostContent  = nullptr;
static bool g_netChanging        = false;

struct AdapterInfo { std::wstring name; std::wstring ip; };
static std::vector<AdapterInfo> g_adapters;

static std::vector<AdapterInfo> EnumAdapters() {
    std::vector<AdapterInfo> result;
    std::string out = RunCommand(
        L"powershell -NoProfile -Command \""
        L"Get-NetIPAddress -AddressFamily IPv4 | "
        L"Where-Object { $_.IPAddress -ne '127.0.0.1' -and $_.IPAddress -notlike '169.254.*' -and $_.InterfaceAlias -notlike 'vEthernet*' -and $_.InterfaceAlias -notlike '*Bluetooth*' } | "
        L"ForEach-Object { $_.InterfaceAlias + '|' + $_.IPAddress }\"");
    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line = TrimRight(line);
        auto pos = line.find('|');
        if (pos == std::string::npos) continue;
        AdapterInfo ai;
        ai.name = ToWide(line.substr(0, pos));
        ai.ip   = ToWide(line.substr(pos + 1));
        result.push_back(ai);
    }
    return result;
}

static void BuildEqhostContent(const std::wstring& ip) {
    std::wstring content =
        L"[Registration Servers]\r\n{\r\n\"" + ip + L":6000\"\r\n}\r\n"
        L"[Login Servers]\r\n{\r\n\"" + ip + L":6000\"\r\n}\r\n";
    if (g_hwndEqhostContent)
        SetWindowTextW(g_hwndEqhostContent, content.c_str());
}

static void RefreshNetworkTab() {
    std::wstring addr = GetServerAddress();
    if (g_hwndNetCurrent)
        SetWindowTextW(g_hwndNetCurrent, addr.c_str());
    BuildEqhostContent(addr);
}

static void CreateNetworkPanel(HWND parent) {
    MakeLabel(parent, L"Current Server Address:", 20, 20, 170, 20);
    g_hwndNetCurrent = MakeLabel(parent, L"", 200, 20, 200, 20);
    MakeButton(parent, L"Change Network Setting", IDC_BTN_CHANGE_NETWORK, 20, 50, 180, 30);
    MakeButton(parent, L"Write eqhost.txt...",    IDC_BTN_WRITE_EQHOST,  215, 50, 150, 30);
    g_hwndNetAdapterList = MakeListBox(parent, IDC_NET_ADAPTER_LIST, 20, 100, 400, 100);
    ShowWindow(g_hwndNetAdapterList, SW_HIDE);
    MakeButton(parent, L"Confirm Selection", IDC_BTN_NET_CONFIRM, 430, 100, 140, 30);
    ShowWindow(GetDlgItem(parent, IDC_BTN_NET_CONFIRM), SW_HIDE);
    MakeLabel(parent, L"eqhost.txt content (copy to your TAKP client folder):", 20, 215, 400, 20);
    g_hwndEqhostContent = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        20, 238, 420, 120, parent,
        (HMENU)(UINT_PTR)IDC_NET_EQHOST_CONTENT, g_hInst, nullptr);
    g_hwndNetStatusMsg = MakeLabel(parent, L"", 20, 375, 560, 36);
}

static void DoChangeNetwork() {
    if (g_netChanging) {
        ShowWindow(g_hwndNetAdapterList, SW_HIDE);
        HWND btn = GetDlgItem(g_hwndPanels[TAB_NETWORK], IDC_BTN_NET_CONFIRM);
        if (btn) ShowWindow(btn, SW_HIDE);
        SetWindowTextW(g_hwndNetStatusMsg, L"");
        g_netChanging = false;
        return;
    }
    g_adapters = EnumAdapters();
    SendMessage(g_hwndNetAdapterList, LB_RESETCONTENT, 0, 0);
    SendMessageW(g_hwndNetAdapterList, LB_ADDSTRING, 0,
        (LPARAM)L"Local only - 127.0.0.1 (only this computer)");
    for (auto& a : g_adapters) {
        std::wstring item = a.name + L"  -  " + a.ip;
        SendMessageW(g_hwndNetAdapterList, LB_ADDSTRING, 0, (LPARAM)item.c_str());
    }
    ShowWindow(g_hwndNetAdapterList, SW_SHOW);
    HWND btn = GetDlgItem(g_hwndPanels[TAB_NETWORK], IDC_BTN_NET_CONFIRM);
    if (btn) ShowWindow(btn, SW_SHOW);
    SetWindowTextW(g_hwndNetStatusMsg, L"Select your network adapter and click Confirm Selection.");
    g_netChanging = true;
}

static void DoConfirmNetwork() {
    int sel = (int)SendMessage(g_hwndNetAdapterList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        MessageBoxW(g_hwndMain, L"Please select a network option.",
            L"Network", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring newIp = (sel == 0) ? L"127.0.0.1" : g_adapters[sel - 1].ip;
    SetServerAddress(newIp);
    ShowWindow(g_hwndNetAdapterList, SW_HIDE);
    HWND btn = GetDlgItem(g_hwndPanels[TAB_NETWORK], IDC_BTN_NET_CONFIRM);
    if (btn) ShowWindow(btn, SW_HIDE);
    g_netChanging = false;
    RefreshNetworkTab();
    SetWindowTextW(g_hwndNetStatusMsg,
        L"Network setting updated. Restart the server for the change to take effect.");
}

static void DoWriteEqhost() {
    const wchar_t* commonPaths[] = {
        L"C:\\TAKP", L"C:\\EverQuest", L"C:\\Games\\TAKP", L"C:\\Games\\EverQuest", nullptr
    };
    std::wstring addr = GetServerAddress();
    std::wstring content =
        L"[Registration Servers]\n{\n\"" + addr + L":6000\"\n}\n"
        L"[Login Servers]\n{\n\"" + addr + L":6000\"\n}\n";
    for (int i = 0; commonPaths[i]; ++i) {
        wchar_t path[MAX_PATH];
        wcscpy_s(path, commonPaths[i]);
        PathAppendW(path, L"eqhost.txt");
        if (PathFileExistsW(path)) {
            int r = MessageBoxW(g_hwndMain,
                (std::wstring(L"Found eqhost.txt at:\n") + path + L"\n\nWrite server address automatically?").c_str(),
                L"Write eqhost.txt", MB_YESNO | MB_ICONQUESTION);
            if (r == IDYES) {
                std::ofstream f(path);
                if (f) {
                    f << std::string(content.begin(), content.end());
                    MessageBoxW(g_hwndMain, L"eqhost.txt written successfully.", L"Done", MB_OK | MB_ICONINFORMATION);
                    return;
                }
            }
            return;
        }
    }
    wchar_t szFile[MAX_PATH] = L"eqhost.txt";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g_hwndMain;
    ofn.lpstrFilter = L"eqhost.txt\0eqhost.txt\0All Files\0*.*\0";
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle  = L"Save eqhost.txt - Navigate to your TAKP client folder";
    ofn.lpstrDefExt = L"txt";
    if (GetSaveFileNameW(&ofn)) {
        std::ofstream f(szFile);
        if (f) {
            f << std::string(content.begin(), content.end());
            MessageBoxW(g_hwndMain, L"eqhost.txt written successfully.", L"Done", MB_OK | MB_ICONINFORMATION);
        }
    }
}

// ============================================================
// TAB 7 — ADVANCED PANEL
// ============================================================

static HWND g_hwndAdvResult = nullptr;

static bool GetAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    DWORD type, size = 0;
    bool exists = (RegQueryValueExW(hKey, AUTOSTART_NAME, nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}

static void SetAutoStart(bool enable) {
    HKEY hKey;
    RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_WRITE, &hKey);
    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        RegSetValueExW(hKey, AUTOSTART_NAME, 0, REG_SZ,
            (const BYTE*)exePath, (DWORD)((wcslen(exePath) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, AUTOSTART_NAME);
    }
    RegCloseKey(hKey);
}

static int GetBackupRetention() {
    wchar_t cfgPath[MAX_PATH];
    wcscpy_s(cfgPath, g_installDir);
    PathAppendW(cfgPath, L".qsm_settings");
    std::ifstream f(cfgPath);
    if (!f) return 10;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("backup_retention=", 0) == 0) {
            try { return std::stoi(line.substr(17)); } catch (...) {}
        }
    }
    return 10;
}

static bool GetNoBackupOnStop() {
    wchar_t cfgPath[MAX_PATH];
    wcscpy_s(cfgPath, g_installDir);
    PathAppendW(cfgPath, L".qsm_settings");
    std::ifstream f(cfgPath);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line == "no_backup_on_stop=1") return true;
    }
    return false;
}

static void SaveSettings(bool noBackup, int retention) {
    wchar_t cfgPath[MAX_PATH];
    wcscpy_s(cfgPath, g_installDir);
    PathAppendW(cfgPath, L".qsm_settings");
    std::ofstream f(cfgPath);
    if (f) {
        f << "no_backup_on_stop=" << (noBackup ? 1 : 0) << "\n";
        f << "backup_retention=" << retention << "\n";
    }
}

static void PruneOldBackups(int keepCount) {
    if (keepCount <= 0) return;
    auto files = GetBackupFiles(L"backup_");
    for (int i = keepCount; i < (int)files.size(); ++i) {
        wchar_t path[MAX_PATH];
        wcscpy_s(path, g_installDir);
        PathAppendW(path, L"config\\backups");
        PathAppendW(path, files[i].c_str());
        DeleteFileW(path);
    }
}

static void CreateAdvancedPanel(HWND parent) {
    int y = 16;

    MakeLabel(parent, L"Server Operations:", 20, y, 150, 20);
    y += 26;

    MakeButton(parent, L"Rebuild Server", IDC_BTN_REBUILD,     20,  y, 130, 30);
    MakeButton(parent, L"Start Fresh...", IDC_BTN_START_FRESH, 165, y, 130, 30);
    y += 46;

    g_hwndAdvResult = MakeResultBox(parent, IDC_ADV_RESULT, 20, y, 570, 80);
    y += 96;

    MakeLabel(parent, L"Utilities:", 20, y, 80, 20);
    y += 26;
    MakeButton(parent, L"Copy eqhost.txt",    IDC_BTN_COPY_EQHOST,  20,  y, 140, 26);
    MakeButton(parent, L"Open Install Folder", IDC_BTN_OPEN_FOLDER, 170, y, 150, 26);
    MakeButton(parent, L"Open Docker Desktop", IDC_BTN_OPEN_DOCKER, 330, y, 150, 26);
    y += 44;

    MakeLabel(parent, L"Settings:", 20, y, 80, 20);
    y += 26;
    HWND chkAutoStart = MakeCheck(parent, L"Start with Windows",
                                   IDC_CHK_AUTOSTART, 20, y, 160, 22);
    if (GetAutoStartEnabled())
        SendMessage(chkAutoStart, BM_SETCHECK, BST_CHECKED, 0);
    y += 28;
    HWND chkNoBackup = MakeCheck(parent,
        L"Disable automatic backup on stop  (not recommended)",
        IDC_CHK_NO_BACKUP, 20, y, 380, 22);
    if (GetNoBackupOnStop())
        SendMessage(chkNoBackup, BM_SETCHECK, BST_CHECKED, 0);
    y += 30;
    MakeLabel(parent, L"Keep last backups:", 20, y + 4, 130, 20);
    HWND cboRetention = MakeCombo(parent, IDC_BACKUP_RETENTION, 158, y, 80, 100);
    SendMessageW(cboRetention, CB_ADDSTRING, 0, (LPARAM)L"5");
    SendMessageW(cboRetention, CB_ADDSTRING, 0, (LPARAM)L"10");
    SendMessageW(cboRetention, CB_ADDSTRING, 0, (LPARAM)L"20");
    SendMessageW(cboRetention, CB_ADDSTRING, 0, (LPARAM)L"Unlimited");
    int ret = GetBackupRetention();
    int retIdx = (ret == 5 ? 0 : ret == 10 ? 1 : ret == 20 ? 2 : 3);
    SendMessage(cboRetention, CB_SETCURSEL, retIdx, 0);
}

static void DoCopyEqhost() {
    std::wstring addr = GetServerAddress();
    std::wstring content =
        L"[Registration Servers]\n{\n\"" + addr + L":6000\"\n}\n"
        L"[Login Servers]\n{\n\"" + addr + L":6000\"\n}\n";
    if (OpenClipboard(g_hwndMain)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (content.size() + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t* p = (wchar_t*)GlobalLock(hMem);
            wcscpy_s(p, content.size() + 1, content.c_str());
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
        SetWindowTextW(g_hwndAdvResult, L"eqhost.txt content copied to clipboard.");
    }
}

static void DoRebuild() {
    if (g_operationBusy) return;
    int r = MessageBoxW(g_hwndMain,
        L"Rebuild the server image from scratch?\n\n"
        L"Your character data will be PRESERVED.\n"
        L"The server image will be deleted and recompiled.\n"
        L"This takes 40-55 minutes.\n\n"
        L"A backup will be taken automatically before the rebuild.\n\nContinue?",
        L"Confirm Rebuild", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;

    SetBusy(true);
    SetStatus(L"Rebuilding... (40-55 minutes)");
    SetWindowTextW(g_hwndAdvResult,
        L"Rebuild started. This takes 40-55 minutes.\r\n"
        L"Do not close this window.");

    std::thread([]{
        if (IsContainerRunning()) {
            wchar_t backupDir[MAX_PATH];
            wcscpy_s(backupDir, g_installDir);
            PathAppendW(backupDir, L"config\\backups");
            CreateDirectoryW(backupDir, nullptr);
            std::wstring ds = GetDateStamp();
            wchar_t fullFile[MAX_PATH];
            wcscpy_s(fullFile, backupDir);
            PathAppendW(fullFile, (L"backup_" + ds + L".sql").c_str());
            std::wstring cmd = L"cmd /c docker exec " + std::wstring(CONTAINER) +
                L" mariadb-dump quarm > \"" + std::wstring(fullFile) + L"\"";
            RunCommand(cmd, g_installDir);
        }
        RunCommand(L"docker compose down", g_installDir);
        DWORD ec = 0;
        RunCommand(L"docker compose build", g_installDir, &ec);
        if (ec != 0) {
            auto* res = new AsyncResult{ false,
                L"Rebuild failed. Check that Docker Desktop is running.\r\n"
                L"Your character data is safe. Try again." };
            PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
            return;
        }
        RunCommand(L"docker compose up -d", g_installDir);
        auto* res = new AsyncResult{ true, L"Rebuild complete. Server is running." };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
    }).detach();
}

static void DoStartFresh() {
    if (g_operationBusy) return;
    int r = MessageBoxW(g_hwndMain,
        L"WARNING — THIS WILL PERMANENTLY DELETE ALL CHARACTER DATA.\n\n"
        L"This destroys the quarm-data volume.\n"
        L"ALL characters, accounts, and progress will be lost.\n\n"
        L"This cannot be undone.\n\nAre you absolutely sure?",
        L"START FRESH — DATA WILL BE DELETED",
        MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2);
    if (r != IDYES) return;
    r = MessageBoxW(g_hwndMain,
        L"FINAL WARNING\n\nClicking Yes will permanently delete all character data.\n"
        L"There is no backup. There is no undo.\n\nDelete everything and start fresh?",
        L"CONFIRM DELETE ALL DATA",
        MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2);
    if (r != IDYES) return;

    SetBusy(true);
    SetStatus(L"Starting fresh... (40-55 minutes)");
    SetWindowTextW(g_hwndAdvResult,
        L"Deleting all data and rebuilding...\r\n"
        L"This takes 40-55 minutes. Do not close this window.");

    std::thread([]{
        RunCommand(L"docker compose down -v", g_installDir);
        DWORD ec = 0;
        RunCommand(L"docker compose build", g_installDir, &ec);
        if (ec != 0) {
            auto* res = new AsyncResult{ false, L"Build failed. Click Rebuild Server to try again." };
            PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
            return;
        }
        RunCommand(L"docker compose up -d", g_installDir);
        auto* res = new AsyncResult{ true, L"Fresh start complete. Server is running with a clean database." };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
    }).detach();
}

// ============================================================
// TAB 8 — GAME TOOLS PANEL
// ============================================================

static HWND g_hwndGameItemSearch = nullptr;
static HWND g_hwndGameItemId     = nullptr;
static HWND g_hwndGameCharName   = nullptr;
static HWND g_hwndGameResult     = nullptr;
static HWND g_hwndGameEraCbo     = nullptr;
static HWND g_hwndGameZoneCbo    = nullptr;
static HWND g_hwndGameEraCur     = nullptr;
static HWND g_hwndGameZoneCur    = nullptr;

static void SetGameResult(const std::wstring& text) {
    if (g_hwndGameResult)
        SetWindowTextW(g_hwndGameResult, text.c_str());
}

static void CreateGameToolsPanel(HWND parent) {
    int y = 10;

    MakeLabel(parent, L"Item Lookup:", 20, y, 120, 20);
    y += 22;
    MakeLabel(parent, L"Search:", 20, y+4, 50, 20);
    g_hwndGameItemSearch = MakeEdit(parent, IDC_GAME_ITEM_SEARCH, 76, y, 240, 24);
    MakeButton(parent, L"Search Items", IDC_BTN_ITEM_SEARCH, 326, y, 110, 26);
    MakeLabel(parent, L"(name or item ID)", 446, y+4, 150, 20);
    y += 34;

    MakeLabel(parent, L"Give Item:", 20, y, 80, 20);
    y += 22;
    MakeLabel(parent, L"Character:", 20, y+4, 65, 20);
    g_hwndGameCharName = MakeEdit(parent, IDC_GAME_CHAR_NAME, 90, y, 150, 24);
    MakeLabel(parent, L"Item ID:", 250, y+4, 52, 20);
    g_hwndGameItemId = MakeEdit(parent, IDC_GAME_ITEM_ID, 306, y, 80, 24);
    MakeButton(parent, L"Give Item", IDC_BTN_GIVE_ITEM, 396, y, 100, 26);
    y += 40;

    MakeLabel(parent, L"Server Settings (require restart):", 20, y, 250, 20);
    y += 24;

    MakeLabel(parent, L"Era / Expansion:", 20, y+4, 110, 20);
    MakeLabel(parent, L"Current:", 136, y+4, 52, 20);
    g_hwndGameEraCur = MakeLabel(parent, L"(unknown)", 190, y+4, 120, 20);
    g_hwndGameEraCbo = MakeCombo(parent, IDC_GAME_ERA_COMBO, 320, y, 160, 200);
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Classic");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Kunark");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Velious");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Luclin");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Planes of Power");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"All Expansions");
    MakeButton(parent, L"Set Era", IDC_BTN_SET_ERA, 490, y, 80, 26);
    y += 32;

    MakeLabel(parent, L"Dynamic Zones:", 20, y+4, 100, 20);
    MakeLabel(parent, L"Current:", 136, y+4, 52, 20);
    g_hwndGameZoneCur = MakeLabel(parent, L"(unknown)", 190, y+4, 60, 20);
    g_hwndGameZoneCbo = MakeCombo(parent, IDC_GAME_ZONE_COMBO, 320, y, 80, 200);
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"5");
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"10");
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"15");
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"20");
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"25");
    MakeButton(parent, L"Set Zones", IDC_BTN_SET_ZONE_COUNT, 410, y, 90, 26);
    MakeLabel(parent, L"(more zones = more RAM)", 510, y+4, 180, 20);
    y += 42;

    g_hwndGameResult = MakeResultBox(parent, IDC_GAME_RESULT, 20, y, 730, 200);
}

static void RefreshGameToolsTab() {
    if (!IsContainerRunning()) {
        if (g_hwndGameEraCur) SetWindowTextW(g_hwndGameEraCur, L"(server off)");
        if (g_hwndGameZoneCur) SetWindowTextW(g_hwndGameZoneCur, L"(server off)");
        return;
    }
    std::wstring eraSql = L"SELECT rule_value FROM rule_values WHERE rule_name='World:CurrentExpansion'";
    std::wstring eraResult = RunQuery(eraSql);
    if (eraResult != L"(no results)") {
        std::wstring display = L"(unknown)";
        if (eraResult.find(L"-1") != std::wstring::npos) display = L"All";
        else if (eraResult.find(L"4") != std::wstring::npos) display = L"PoP";
        else if (eraResult.find(L"3") != std::wstring::npos) display = L"Luclin";
        else if (eraResult.find(L"2") != std::wstring::npos) display = L"Velious";
        else if (eraResult.find(L"1") != std::wstring::npos) display = L"Kunark";
        else if (eraResult.find(L"0") != std::wstring::npos) display = L"Classic";
        if (g_hwndGameEraCur) SetWindowTextW(g_hwndGameEraCur, display.c_str());
    }
    std::wstring zoneSql = L"SELECT dynamics FROM launcher LIMIT 1";
    std::wstring zoneResult = RunQuery(zoneSql);
    if (zoneResult != L"(no results)") {
        std::wstring num;
        bool foundNewline = false;
        for (auto c : zoneResult) {
            if (c == L'\n' || c == L'\r') { foundNewline = true; continue; }
            if (foundNewline && iswdigit(c)) num += c;
            else if (foundNewline && !num.empty()) break;
        }
        if (!num.empty() && g_hwndGameZoneCur)
            SetWindowTextW(g_hwndGameZoneCur, num.c_str());
    }
}

static void DoItemSearch() {
    wchar_t search[256] = {};
    GetWindowTextW(g_hwndGameItemSearch, search, 256);
    if (!search[0]) {
        MessageBoxW(g_hwndMain, L"Enter an item name or item ID to search.",
            L"Search Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!CheckServerRunning(L"Item Lookup")) return;

    bool isNumeric = true;
    for (wchar_t* p = search; *p; p++) {
        if (!iswdigit(*p)) { isNumeric = false; break; }
    }

    std::wstring sql;
    if (isNumeric) {
        sql = L"SELECT id, Name, ItemType, Classes, Races, Slots, Price, StackSize "
              L"FROM items WHERE id=" + std::wstring(search);
    } else {
        sql = L"SELECT id, Name, ItemType, Classes, Races, Slots, Price, StackSize "
              L"FROM items WHERE LOWER(Name) LIKE LOWER('%" + std::wstring(search) + L"%') "
              L"ORDER BY Name LIMIT 50";
    }
    std::wstring result = RunQuery(sql);
    SetGameResult(std::wstring(L"Item search for '") + search + L"':\r\n\r\n" + result);
}

static void DoGiveItem() {
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndGameCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t itemStr[32] = {};
    GetWindowTextW(g_hwndGameItemId, itemStr, 32);
    if (!itemStr[0]) {
        MessageBoxW(g_hwndMain, L"Enter an item ID. Use Item Lookup to find IDs.",
            L"Item ID Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    for (wchar_t* p = itemStr; *p; p++) {
        if (!iswdigit(*p)) {
            MessageBoxW(g_hwndMain, L"Item ID must be a number.",
                L"Invalid Item ID", MB_OK | MB_ICONWARNING);
            return;
        }
    }
    if (!CheckServerRunning(L"Give Item")) return;

    // Validate item exists
    std::wstring itemSql = L"SELECT id, Name, StackSize FROM items WHERE id=" + std::wstring(itemStr);
    std::wstring itemInfo = RunQuery(itemSql);
    if (itemInfo == L"(no results)") {
        SetGameResult(std::wstring(L"Item ID ") + itemStr + L" not found in items table.");
        return;
    }

    // Get character internal ID
    std::wstring charSql = L"SELECT cd.id, cd.name FROM character_data cd "
                           L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring charResult = RunQuery(charSql);
    if (charResult == L"(no results)") {
        SetGameResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }

    // Parse character ID from result
    std::wstring charIdStr;
    {
        bool foundNewline = false;
        for (auto c : charResult) {
            if (c == L'\n' || c == L'\r') { foundNewline = true; continue; }
            if (foundNewline && iswdigit(c)) charIdStr += c;
            else if (foundNewline && (c == L'\t' || c == L' ') && !charIdStr.empty()) break;
        }
    }
    if (charIdStr.empty()) {
        SetGameResult(L"Could not parse character ID from database result.");
        return;
    }

    // Find open general inventory slot (23-30), searching from highest
    std::wstring slotSql = L"SELECT slotid FROM character_inventory "
                           L"WHERE id=" + charIdStr +
                           L" AND slotid BETWEEN 23 AND 30 ORDER BY slotid";
    std::wstring slotResult = RunQuery(slotSql);

    bool slotUsed[8] = {};
    if (slotResult != L"(no results)") {
        std::wstring num;
        for (auto c : slotResult) {
            if (iswdigit(c)) { num += c; }
            else if (!num.empty()) {
                int slot = _wtoi(num.c_str());
                if (slot >= 23 && slot <= 30) slotUsed[slot - 23] = true;
                num.clear();
            }
        }
        if (!num.empty()) {
            int slot = _wtoi(num.c_str());
            if (slot >= 23 && slot <= 30) slotUsed[slot - 23] = true;
        }
    }

    int openSlot = -1;
    for (int i = 7; i >= 0; --i) {
        if (!slotUsed[i]) { openSlot = 23 + i; break; }
    }
    if (openSlot == -1) {
        SetGameResult(std::wstring(L"Character '") + chr +
            L"' has no open general inventory slots (23-30 all full).\r\n"
            L"Free up an inventory slot first.");
        return;
    }

    int online = IsCharacterOnline(chr);

    // Confirm
    std::wstring msg = std::wstring(L"Give item to '") + chr + L"'?\r\n\r\n" +
        L"Item: " + std::wstring(itemStr) + L"\r\n" + itemInfo + L"\r\n" +
        L"Slot: " + std::to_wstring(openSlot) + L" (general inventory)\r\n\r\n";
    if (online == 1)
        msg += L"Character is ONLINE. Item will appear after they camp to\r\ncharacter select and re-enter world.\r\n\r\n";
    else
        msg += L"Character is offline. Item will appear on next login.\r\n\r\n";
    msg += L"Continue?";

    int r = MessageBoxW(g_hwndMain, msg.c_str(),
        L"Confirm Give Item", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;

    // Determine charges (use StackSize if stackable, else 1)
    int charges = 1;
    {
        std::wstring lastNum;
        bool inDataLine = false;
        for (auto c : itemInfo) {
            if (c == L'\n' || c == L'\r') { inDataLine = true; lastNum.clear(); continue; }
            if (inDataLine && c == L'\t') lastNum.clear();
            if (inDataLine && iswdigit(c)) lastNum += c;
        }
        if (!lastNum.empty()) {
            int ss = _wtoi(lastNum.c_str());
            if (ss > 1) charges = ss;
        }
    }

    // INSERT
    std::wstring insertSql = L"INSERT INTO character_inventory (id, slotid, itemid, charges) VALUES (" +
        charIdStr + L", " + std::to_wstring(openSlot) + L", " +
        std::wstring(itemStr) + L", " + std::to_wstring(charges) + L")";
    RunQuery(insertSql);

    if (online == 1)
        SetGameResult(std::wstring(L"Item ") + itemStr + L" added to '" + chr +
            L"' in slot " + std::to_wstring(openSlot) + L".\r\n\r\n"
            L"Character is online. They must camp to character select\r\n"
            L"and re-enter world to receive the item.");
    else
        SetGameResult(std::wstring(L"Item ") + itemStr + L" added to '" + chr +
            L"' in slot " + std::to_wstring(openSlot) + L".\r\n\r\n"
            L"Item will appear in inventory on next login.");
}

static void DoSetEra() {
    if (!CheckServerRunning(L"Set Era")) return;
    int sel = (int)SendMessage(g_hwndGameEraCbo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) {
        MessageBoxW(g_hwndMain, L"Select an era from the dropdown.",
            L"Era Required", MB_OK | MB_ICONINFORMATION);
        return;
    }

    struct EraPreset { const wchar_t* name; const wchar_t* eraNum; int bitmask; int maxExpansion; };
    EraPreset presets[] = {
        { L"Classic",          L"0",  0,  0 },
        { L"Kunark",           L"1",  1,  1 },
        { L"Velious",          L"2",  3,  2 },
        { L"Luclin",           L"3",  7,  3 },
        { L"Planes of Power",  L"4",  15, 4 },
        { L"All Expansions",   L"-1", 15, 99 },
    };
    auto& p = presets[sel];

    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Set server era to '") + p.name + L"'?\r\n\r\n"
         L"This changes:\r\n"
         L"  World:CurrentExpansion = " + p.eraNum + L"\r\n"
         L"  Character:DefaultExpansions = " + std::to_wstring(p.bitmask) + L"\r\n"
         L"  All existing account expansion flags\r\n"
         L"  Zone access restrictions\r\n\r\n"
         L"Server will restart for this to take effect.\r\nRestart now?").c_str(),
        L"Confirm Era Change", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;

    SetGameResult(L"Applying era change...");

    RunQuery(L"UPDATE rule_values SET rule_value='" + std::wstring(p.eraNum) +
             L"' WHERE rule_name='World:CurrentExpansion'");
    RunQuery(L"UPDATE rule_values SET rule_value='" + std::to_wstring(p.bitmask) +
             L"' WHERE rule_name='Character:DefaultExpansions'");
    RunQuery(L"UPDATE account SET expansion=" + std::to_wstring(p.bitmask));

    if (p.maxExpansion < 99) {
        RunQuery(L"UPDATE zone SET min_status=0 WHERE expansion<=" + std::to_wstring(p.maxExpansion));
        RunQuery(L"UPDATE zone SET min_status=100 WHERE expansion>" + std::to_wstring(p.maxExpansion));
    } else {
        RunQuery(L"UPDATE zone SET min_status=0");
    }

    SetGameResult(std::wstring(L"Era set to '") + p.name + L"'. Restarting server...");
    DoRestartServerAsync();
}

static void DoSetZoneCount() {
    if (!CheckServerRunning(L"Set Zone Count")) return;
    int sel = (int)SendMessage(g_hwndGameZoneCbo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) {
        MessageBoxW(g_hwndMain, L"Select a zone count from the dropdown.",
            L"Zone Count Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int counts[] = { 5, 10, 15, 20, 25 };
    int newCount = counts[sel];

    std::wstring nameSql = L"SELECT name FROM launcher LIMIT 1";
    std::wstring nameResult = RunQuery(nameSql);
    if (nameResult == L"(no results)") {
        SetGameResult(L"No launcher found in database. Cannot change zone count.");
        return;
    }
    std::wstring launcherName;
    bool foundNewline = false;
    for (auto c : nameResult) {
        if (c == L'\n' || c == L'\r') { foundNewline = true; continue; }
        if (foundNewline && c != L'\t' && c != L' ') launcherName += c;
        else if (foundNewline && !launcherName.empty()) break;
    }
    if (launcherName.empty()) {
        SetGameResult(L"Could not parse launcher name from database.");
        return;
    }

    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Set dynamic zones to ") + std::to_wstring(newCount) +
         L"?\r\n\r\nLauncher: " + launcherName +
         L"\r\n\r\nMore zones = more RAM (~50-100 MB each).\r\n"
         L"Server will restart for this to take effect.\r\nRestart now?").c_str(),
        L"Confirm Zone Count Change", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;

    RunQuery(L"UPDATE launcher SET dynamics=" + std::to_wstring(newCount) +
             L" WHERE name='" + launcherName + L"'");
    SetGameResult(std::wstring(L"Dynamic zones set to ") + std::to_wstring(newCount) +
        L" for launcher '" + launcherName + L"'. Restarting server...");
    DoRestartServerAsync();
}

// ============================================================
// TAB PANEL SHOW/HIDE
// ============================================================

static void ShowTab(int idx) {
    for (int i = 0; i < NUM_TABS; ++i)
        ShowWindow(g_hwndPanels[i], i == idx ? SW_SHOW : SW_HIDE);
    switch (idx) {
        case TAB_STATUS:  RefreshStatusTab();    break;
        case TAB_BACKUP:  RefreshBackupList();   break;
        case TAB_NETWORK: RefreshNetworkTab();   break;
        case TAB_GAME:    RefreshGameToolsTab(); break;
        default: break;
    }
}

static void LayoutPanels() {
    if (!g_hwndMain || !g_hwndTab) return;
    RECT rcClient;
    GetClientRect(g_hwndMain, &rcClient);
    RECT rcSb{};
    if (g_hwndStatus) GetWindowRect(g_hwndStatus, &rcSb);
    int sbH = rcSb.bottom - rcSb.top;
    SetWindowPos(g_hwndTab, nullptr, 0, 0, rcClient.right, 28, SWP_NOZORDER | SWP_NOACTIVATE);
    int panelTop = 32;
    int panelH   = rcClient.bottom - panelTop - sbH;
    int panelW   = rcClient.right;
    for (int i = 0; i < NUM_TABS; ++i) {
        if (g_hwndPanels[i])
            SetWindowPos(g_hwndPanels[i], nullptr, 0, panelTop, panelW, panelH,
                SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (g_hwndStatus) SendMessage(g_hwndStatus, WM_SIZE, 0, 0);
}

// ============================================================
// MAIN WINDOW PROCEDURE
// ============================================================

static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND)
        return SendMessageW(GetParent(hwnd), msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_TAB_CLASSES | ICC_BAR_CLASSES };
        InitCommonControlsEx(&icc);

        NONCLIENTMETRICSW ncm{ sizeof(ncm) };
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        g_hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        ncm.lfMessageFont.lfWeight = FW_BOLD;
        g_hFontBold = CreateFontIndirectW(&ncm.lfMessageFont);

        // Monospace font for result boxes and log
        g_hFontMono = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        g_hwndStatus = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_STATUSBAR, g_hInst, nullptr);

        g_hwndTab = CreateWindowExW(0, WC_TABCONTROLW, nullptr,
            WS_CHILD | WS_VISIBLE | TCS_HOTTRACK,
            0, 0, 100, 28, hwnd, (HMENU)(UINT_PTR)IDC_TAB, g_hInst, nullptr);
        SendMessage(g_hwndTab, WM_SETFONT, (WPARAM)g_hFont, TRUE);

        TCITEMW ti{ TCIF_TEXT };
        for (int i = 0; i < NUM_TABS; ++i) {
            ti.pszText = (LPWSTR)TAB_LABELS[i];
            TabCtrl_InsertItem(g_hwndTab, i, &ti);
        }

        WNDCLASSEXW pc{};
        pc.cbSize        = sizeof(pc);
        pc.lpfnWndProc   = PanelProc;
        pc.hInstance     = g_hInst;
        pc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        pc.lpszClassName = PANEL_CLASS;
        RegisterClassExW(&pc);

        for (int i = 0; i < NUM_TABS; ++i) {
            g_hwndPanels[i] = CreateWindowExW(0, PANEL_CLASS, nullptr,
                WS_CHILD | (i == 0 ? WS_VISIBLE : 0),
                0, 32, 800, 500, hwnd, nullptr, g_hInst, nullptr);
        }

        CreateStatusPanel(g_hwndPanels[TAB_STATUS]);
        CreateAdminPanel(g_hwndPanels[TAB_ADMIN]);
        CreatePlayerPanel(g_hwndPanels[TAB_PLAYER]);
        CreateBackupPanel(g_hwndPanels[TAB_BACKUP]);
        CreateLogPanel(g_hwndPanels[TAB_LOG]);
        CreateNetworkPanel(g_hwndPanels[TAB_NETWORK]);
        CreateAdvancedPanel(g_hwndPanels[TAB_ADVANCED]);
        CreateGameToolsPanel(g_hwndPanels[TAB_GAME]);

        for (int i = 0; i < NUM_TABS; ++i)
            ApplyFont(g_hwndPanels[i], g_hFont);

        // Re-apply mono font to result boxes (ApplyFont would overwrite them)
        if (g_hFontMono) {
            if (g_hwndAdmResult)   SendMessage(g_hwndAdmResult,  WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndPlrResult)   SendMessage(g_hwndPlrResult,  WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndAdvResult)   SendMessage(g_hwndAdvResult,  WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndLogText)     SendMessage(g_hwndLogText,    WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndProcList)    SendMessage(g_hwndProcList,   WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndGameResult)  SendMessage(g_hwndGameResult, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
        }

        SetTimer(hwnd, TIMER_POLL, POLL_MS, nullptr);
        PostMessage(hwnd, WM_STATUS_POLL, 0, 0);
        return 0;
    }

    case WM_SIZE:
        LayoutPanels();
        return 0;

    case WM_TIMER:
        if (wp == TIMER_POLL && !g_operationBusy) {
            std::thread([hwnd]{
                bool running = IsContainerRunning();
                PostMessageW(hwnd, WM_STATUS_POLL, running ? 1 : 0, 0);
            }).detach();
        }
        return 0;

    case WM_STATUS_POLL: {
        g_serverRunning = (wp != 0);
        int curTab = TabCtrl_GetCurSel(g_hwndTab);
        if (curTab == TAB_STATUS) RefreshStatusTab();
        else {
            SetStatus(g_serverRunning ? L"Server is running" : L"Server is stopped");
        }
        return 0;
    }

    case WM_ASYNC_DONE: {
        auto* res = reinterpret_cast<AsyncResult*>(lp);
        SetBusy(false);
        int sourceTab = (int)wp;

        if (sourceTab == TAB_LOG) {
            if (res->success)
                SetWindowTextW(g_hwndLogText, res->message.c_str());
            else
                SetWindowTextW(g_hwndLogText, L"Failed to load log.");
        } else if (sourceTab == TAB_ADVANCED) {
            SetWindowTextW(g_hwndAdvResult, res->message.c_str());
            if (!res->success)
                MessageBoxW(hwnd, res->message.c_str(), L"Operation Failed", MB_OK | MB_ICONERROR);
        } else {
            // Backup/restore/export (TAB_BACKUP or Status tab start/stop)
            if (g_hwndBackupInfo)
                SetWindowTextW(g_hwndBackupInfo, res->message.c_str());
            if (!res->success)
                MessageBoxW(hwnd, res->message.c_str(), L"Operation Failed", MB_OK | MB_ICONERROR);
            RefreshBackupList();
        }

        {
            HWND cbo = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BACKUP_RETENTION);
            if (cbo) {
                int sel = (int)SendMessage(cbo, CB_GETCURSEL, 0, 0);
                int keep = (sel == 0 ? 5 : sel == 1 ? 10 : sel == 2 ? 20 : 0);
                PruneOldBackups(keep);
            }
        }

        PostMessage(hwnd, WM_STATUS_POLL, 0, 0);
        delete res;
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lp;
        if (pnm->hwndFrom == g_hwndTab && pnm->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(g_hwndTab);
            ShowTab(sel);
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {

        // --- STATUS TAB ---
        case IDC_BTN_START:
            if (g_operationBusy) break;
            if (g_serverRunning) {
                MessageBoxW(hwnd, L"Server is already running.", L"Start", MB_OK | MB_ICONINFORMATION);
                break;
            }
            SetBusy(true);
            SetStatus(L"Starting server...");
            std::thread([]{
                RunCommand(L"docker compose up -d", g_installDir);
                auto* res = new AsyncResult{ true, L"Server started." };
                PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_STATUS, (LPARAM)res);
            }).detach();
            break;

        case IDC_BTN_STOP: {
            if (g_operationBusy) break;
            if (!g_serverRunning) {
                MessageBoxW(hwnd, L"Server is not running.", L"Stop", MB_OK | MB_ICONINFORMATION);
                break;
            }
            int r = MessageBoxW(hwnd,
                L"Stop the server?\n\nA backup will be taken automatically first.",
                L"Confirm Stop", MB_YESNO | MB_ICONQUESTION);
            if (r != IDYES) break;
            bool noBackup = GetNoBackupOnStop();
            SetBusy(true);
            SetStatus(L"Stopping server...");
            std::thread([noBackup]{
                if (!noBackup) {
                    wchar_t bd[MAX_PATH]; wcscpy_s(bd, g_installDir);
                    PathAppendW(bd, L"config\\backups");
                    CreateDirectoryW(bd, nullptr);
                    std::wstring ds = GetDateStamp();
                    wchar_t ff[MAX_PATH]; wcscpy_s(ff, bd);
                    PathAppendW(ff, (L"backup_" + ds + L".sql").c_str());
                    std::wstring cmd = L"cmd /c docker exec " + std::wstring(CONTAINER) +
                        L" mariadb-dump quarm > \"" + std::wstring(ff) + L"\"";
                    RunCommand(cmd, g_installDir);
                    HWND cbo = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BACKUP_RETENTION);
                    if (cbo) {
                        int sel = (int)SendMessage(cbo, CB_GETCURSEL, 0, 0);
                        int keep = (sel==0?5 : sel==1?10 : sel==2?20 : 0);
                        PruneOldBackups(keep);
                    }
                }
                RunCommand(L"docker compose down", g_installDir);
                auto* res = new AsyncResult{ true, L"Server stopped." };
                PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_STATUS, (LPARAM)res);
            }).detach();
            break;
        }

        // --- ADMIN TAB ---
        case IDC_BTN_MAKE_GM:          DoMakeGM();          break;
        case IDC_BTN_REMOVE_GM:        DoRemoveGM();        break;
        case IDC_BTN_LIST_ACCOUNTS:    DoListAccounts();    break;
        case IDC_BTN_RESET_PASSWORD:   DoResetPassword();   break;
        case IDC_BTN_WHO_ONLINE:       DoWhoIsOnline();     break;
        case IDC_BTN_RECENT_LOGINS:    DoRecentLogins();    break;
        case IDC_BTN_IP_HISTORY:       DoIPHistory();       break;

        // --- PLAYER TAB ---
        case IDC_BTN_PLR_LIST_CHARS:   DoPlrListChars();    break;
        case IDC_BTN_PLR_CHAR_INFO:    DoPlrCharInfo();     break;
        case IDC_BTN_SHOW_INVENTORY:   DoShowInventory();   break;
        case IDC_BTN_SHOW_CURRENCY:    DoShowCurrency();    break;
        case IDC_BTN_SHOW_ACCT_CHAR:   DoShowAcctForChar(); break;
        case IDC_BTN_MOVE_TO_BIND:     DoMoveToBind();      break;
        case IDC_BTN_FIND_ZONE:        DoFindZone();        break;
        case IDC_BTN_MOVE_TO_ZONE:     DoMoveToZone();      break;
        case IDC_BTN_GIVE_PLAT:        DoGivePlatinum();    break;
        case IDC_BTN_LIST_CORPSES:     DoListCorpses();     break;
        case IDC_BTN_CORPSES_BY_CHAR:  DoCorpsesByChar();   break;

        // --- BACKUP TAB ---
        case IDC_BTN_BACKUP_NOW:
            DoBackupNow();
            break;

        case IDC_BTN_RESTORE: {
            int sel = (int)SendMessage(g_hwndBackupList, LB_GETCURSEL, 0, 0);
            if (sel == LB_ERR) {
                MessageBoxW(hwnd, L"Select a backup from the list first.",
                    L"Restore", MB_OK | MB_ICONINFORMATION);
                break;
            }
            wchar_t item[512] = {};
            SendMessageW(g_hwndBackupList, LB_GETTEXT, sel, (LPARAM)item);
            std::wstring filename = item;
            auto sp = filename.find(L' ');
            if (sp != std::wstring::npos) filename = filename.substr(0, sp);
            DoRestore(filename);
            break;
        }

        case IDC_BTN_EXPORT_CHARS:  DoExportCharacters();  break;
        case IDC_BTN_IMPORT_CHARS:  DoImportCharacters();  break;

        // --- LOG TAB ---
        case IDC_BTN_LOAD_LOG:
        case IDC_BTN_REFRESH_LOG:
            DoLoadLog();
            break;

        // --- NETWORK TAB ---
        case IDC_BTN_CHANGE_NETWORK:  DoChangeNetwork();   break;
        case IDC_BTN_NET_CONFIRM:     DoConfirmNetwork();  break;
        case IDC_BTN_WRITE_EQHOST:    DoWriteEqhost();     break;

        // --- ADVANCED TAB ---
        case IDC_BTN_REBUILD:     DoRebuild();      break;
        case IDC_BTN_START_FRESH: DoStartFresh();   break;
        case IDC_BTN_COPY_EQHOST: DoCopyEqhost();   break;

        case IDC_BTN_OPEN_FOLDER:
            ShellExecuteW(nullptr, L"open", g_installDir,
                nullptr, nullptr, SW_SHOWNORMAL);
            break;

        case IDC_BTN_OPEN_DOCKER:
            ShellExecuteW(nullptr, L"open",
                L"C:\\Program Files\\Docker\\Docker\\Docker Desktop.exe",
                nullptr, nullptr, SW_SHOWNORMAL);
            break;

        case IDC_CHK_AUTOSTART: {
            HWND chk = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_AUTOSTART);
            bool on = (SendMessage(chk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetAutoStart(on);
            break;
        }

        case IDC_CHK_NO_BACKUP: {
            HWND chkNB = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_NO_BACKUP);
            HWND cboR  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BACKUP_RETENTION);
            bool noBackup = (SendMessage(chkNB, BM_GETCHECK, 0, 0) == BST_CHECKED);
            int sel = cboR ? (int)SendMessage(cboR, CB_GETCURSEL, 0, 0) : 1;
            int keep = (sel==0?5 : sel==1?10 : sel==2?20 : 0);
            SaveSettings(noBackup, keep);
            if (noBackup)
                MessageBoxW(hwnd,
                    L"Warning: backups will NOT be taken when stopping the server.\n"
                    L"Your character data will not be protected.",
                    L"Backup Warning", MB_OK | MB_ICONWARNING);
            break;
        }

        case IDC_BACKUP_RETENTION: {
            if (HIWORD(wp) == CBN_SELCHANGE) {
                HWND chkNB = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_NO_BACKUP);
                HWND cboR  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BACKUP_RETENTION);
                bool noBackup = chkNB ?
                    (SendMessage(chkNB, BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
                int sel = (int)SendMessage(cboR, CB_GETCURSEL, 0, 0);
                int keep = (sel==0?5 : sel==1?10 : sel==2?20 : 0);
                SaveSettings(noBackup, keep);
                PruneOldBackups(keep);
            }
            break;
        }

        // --- GAME TOOLS TAB ---
        case IDC_BTN_ITEM_SEARCH:    DoItemSearch();     break;
        case IDC_BTN_GIVE_ITEM:      DoGiveItem();       break;
        case IDC_BTN_SET_ERA:        DoSetEra();         break;
        case IDC_BTN_SET_ZONE_COUNT: DoSetZoneCount();   break;

        } // end switch id
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize = { 900, 600 };
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_POLL);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================
// STARTUP CHECKS
// ============================================================

static bool DoStartupChecks() {
    GetModuleFileNameW(nullptr, g_installDir, MAX_PATH);
    PathRemoveFileSpecW(g_installDir);

    const wchar_t* dockerPaths[] = {
        L"C:\\Program Files\\Docker\\Docker\\Docker Desktop.exe",
        L"C:\\Program Files\\Docker\\Docker\\resources\\bin\\docker.exe",
        nullptr
    };
    bool dockerInstalled = false;
    for (int i = 0; dockerPaths[i]; ++i) {
        if (PathFileExistsW(dockerPaths[i])) { dockerInstalled = true; break; }
    }
    if (!dockerInstalled) {
        std::string out = RunCommand(L"docker --version");
        if (!out.empty() && out.find("Docker version") != std::string::npos)
            dockerInstalled = true;
    }
    if (!dockerInstalled) {
        MessageBoxW(nullptr,
            L"Docker Desktop is not installed.\n\n"
            L"Please download and install Docker Desktop from:\n"
            L"https://www.docker.com/products/docker-desktop\n\n"
            L"Then run Quarm Docker Server again.",
            L"Docker Desktop Required", MB_OK | MB_ICONERROR);
        return false;
    }

    RunCommand(L"docker context use desktop-linux");

    while (true) {
        std::string info = RunCommand(L"docker info");
        if (!info.empty() && info.find("ERROR") == std::string::npos &&
            info.find("error") == std::string::npos)
            break;
        int r = MessageBoxW(nullptr,
            L"Docker Desktop is not running.\n\n"
            L"Please open Docker Desktop and wait for it to start,\nthen click Retry.",
            L"Docker Not Running", MB_RETRYCANCEL | MB_ICONWARNING);
        if (r != IDRETRY) return false;
        RunCommand(L"docker context use desktop-linux");
    }

    wchar_t sentinel[MAX_PATH];
    wcscpy_s(sentinel, g_installDir);
    PathAppendW(sentinel, L".setup_complete");
    if (!PathFileExistsW(sentinel)) {
        MessageBoxW(nullptr,
            L"Quarm Server setup is not complete.\n\n"
            L"Please run the QuarmDocker installer again.\n\n"
            L"If you installed manually, create a file named '.setup_complete'\n"
            L"in your installation directory.",
            L"Setup Not Complete", MB_OK | MB_ICONWARNING);
        return false;
    }

    DWORD inspectEc = 0;
    RunCommand(std::wstring(L"docker inspect ") + CONTAINER, L"", &inspectEc);
    if (inspectEc != 0) {
        MessageBoxW(nullptr,
            L"Server container not found.\n\nPlease run the QuarmDocker installer again.",
            L"Container Not Found", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

// ============================================================
// WINMAIN
// ============================================================

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInst = hInst;
    if (!DoStartupChecks()) return 1;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = APP_CLASS;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    g_hwndMain = CreateWindowExW(0, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 640,
        nullptr, nullptr, hInst, nullptr);
    if (!g_hwndMain) return 1;

    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(g_hwndMain, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}
