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
#include <map>
#include <set>

#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' " \
    "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' " \
    "processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

#define TIMER_POLL               1

#define IDC_TAB                  1000

// Status tab
#define IDC_BTN_START            1004
#define IDC_BTN_STOP             1005
#define IDC_STATUS_PROCESSES     1003

// Admin tab
#define IDC_ADM_ACCOUNT          1100
#define IDC_ADM_PASSWORD         1101
#define IDC_ADM_RESULT           1102
#define IDC_BTN_MAKE_GM          1041
#define IDC_BTN_REMOVE_GM        1103
#define IDC_BTN_LIST_ACCOUNTS    1104
#define IDC_BTN_RESET_PASSWORD   1105
#define IDC_BTN_WHO_ONLINE       1106
#define IDC_BTN_RECENT_LOGINS    1107
#define IDC_BTN_IP_HISTORY       1108

// Player Tools tab
#define IDC_PLR_ACCOUNT          1110
#define IDC_PLR_CHARNAME         1111
#define IDC_PLR_ZONE             1112
#define IDC_PLR_AMOUNT           1113
#define IDC_PLR_RESULT           1114
#define IDC_BTN_PLR_LIST_CHARS   1115
#define IDC_BTN_PLR_CHAR_INFO    1116
#define IDC_BTN_SHOW_INVENTORY   1117
#define IDC_BTN_SHOW_CURRENCY    1118
#define IDC_BTN_SHOW_ACCT_CHAR   1119
#define IDC_BTN_MOVE_TO_BIND     1120
#define IDC_BTN_FIND_ZONE        1121
#define IDC_BTN_MOVE_TO_ZONE     1122
#define IDC_BTN_GIVE_PLAT        1123
#define IDC_BTN_LIST_CORPSES     1124
#define IDC_BTN_CORPSES_BY_CHAR  1125

// Backup & Restore tab
#define IDC_BACKUP_LIST          1010
#define IDC_BTN_BACKUP_NOW       1011
#define IDC_BTN_RESTORE          1012
#define IDC_BTN_EXPORT_CHARS     1013
#define IDC_BTN_IMPORT_CHARS     1014

// Log Viewer tab
#define IDC_LOG_TEXT             1020
#define IDC_BTN_LOAD_LOG         1021
#define IDC_BTN_REFRESH_LOG      1022
#define IDC_LOG_LINES_COMBO      1023

// Network tab
#define IDC_BTN_CHANGE_NETWORK   1031
#define IDC_BTN_WRITE_EQHOST     1032
#define IDC_NET_EQHOST_CONTENT   1033
#define IDC_NET_ADAPTER_LIST     1034
#define IDC_BTN_NET_CONFIRM      1035

// Advanced tab
#define IDC_ADV_RESULT           1053
#define IDC_BTN_REBUILD          1045
#define IDC_BTN_START_FRESH      1046
#define IDC_BTN_COPY_EQHOST      1047
#define IDC_BTN_OPEN_FOLDER      1048
#define IDC_BTN_OPEN_DOCKER      1049
#define IDC_CHK_AUTOSTART        1050
#define IDC_CHK_NO_BACKUP        1051
#define IDC_BACKUP_RETENTION     1052

// Status bar
#define IDC_STATUSBAR            1060

namespace fs = std::filesystem;

// ============================================================
// CONSTANTS
// ============================================================

static const wchar_t* APP_CLASS   = L"QuarmServerManager";
static const wchar_t* APP_TITLE   = L"Quarm Docker Server";
static const wchar_t* APP_VERSION = L"v0.997";
static const wchar_t* PANEL_CLASS = L"QSMPanel";
static const wchar_t* CONTAINER   = L"quarm-server";
static const int      NUM_TABS    = 10;
static const int      POLL_MS     = 5000;

// Tab labels
static const wchar_t* TAB_LABELS[NUM_TABS] = {
    L"Status", L"Player Tools", L"Pro Tools",
    L"Admin Tools", L"Zones", L"Server", L"Backup & Restore",
    L"Log Viewer", L"Network", L"Advanced"
};

// Panel index constants for readability
#define TAB_STATUS   0
#define TAB_PLAYER   1
#define TAB_GAME     2
#define TAB_ADMIN    3
#define TAB_ZONES    4
#define TAB_SERVER   5
#define TAB_BACKUP   6
#define TAB_LOG      7
#define TAB_NETWORK  8
#define TAB_ADVANCED 9

// --- Game Tools tab control IDs (item search/give/era/zone) ---
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

// --- Pro Tools: character management ---
#define IDC_PRO_CHAR_NAME        5200   // character name for char-management ops
#define IDC_PRO_LEVEL_COMBO      5201   // level dropdown 1-65
#define IDC_BTN_PRO_SET_LEVEL    5202   // Set Character Level button
#define IDC_PRO_AA_EDIT          5203   // AA points numeric input
#define IDC_BTN_PRO_SET_AA       5204   // Set AA Points button
#define IDC_PRO_CLASS_COMBO      5205   // class dropdown
#define IDC_BTN_PRO_SET_CLASS    5206   // Change Class button
#define IDC_PRO_RACE_COMBO       5207   // race dropdown
#define IDC_BTN_PRO_SET_RACE     5208   // Change Race button

// --- Admin Tools: new account/character operations ---
#define IDC_ADM_DEL_CHAR         5400
#define IDC_BTN_SERVER_STATS     5401
#define IDC_BTN_SUSPEND_ACCT     5402
#define IDC_BTN_UNSUSPEND_ACCT   5403
#define IDC_BTN_DELETE_CHAR      5404
#define IDC_BTN_BAN_ACCT         5405
#define IDC_BTN_UNBAN_ACCT       5406
#define IDC_BTN_VIEW_BANS        5407

// --- Advanced tab ---
#define IDC_CHK_ALWAYS_ON_TOP    5300
#define IDC_CHK_DARK_MODE        5301
#define IDC_BTN_DOCKER_LOGS      5302
#define IDC_BTN_DISK_USAGE       5303
#define IDC_BTN_CONTAINER_STATS  5304
#define IDC_ADV_SYS_RESULT       5305

// --- Status tab additions ---
#define IDC_STATUS_MOTD_EDIT     5500
#define IDC_BTN_SET_MOTD         5501
#define IDC_BTN_RESTART_SERVER   5502

// --- Player Tools: search ---
#define IDC_PLR_SEARCH_MIN       5600
#define IDC_PLR_SEARCH_MAX       5601
#define IDC_PLR_SEARCH_CLASS     5602
#define IDC_BTN_PLR_SEARCH       5603

// --- Pro Tools: new features ---
#define IDC_PRO_PLAT_AMOUNT      5700
#define IDC_BTN_PRO_GIVE_PLAT    5701
#define IDC_PRO_NEWNAME          5702
#define IDC_BTN_PRO_RENAME       5703
#define IDC_PRO_SURNAME          5704
#define IDC_BTN_PRO_SET_SURNAME  5705
#define IDC_PRO_TITLE_COMBO      5706
#define IDC_BTN_PRO_SET_TITLE    5707
#define IDC_PRO_GENDER_COMBO     5708
#define IDC_BTN_PRO_SET_GENDER   5709
// --- Pro Tools: Currency + Inventory (own IDs, separate from Player Tools) ---
#define IDC_BTN_PRO_CURRENCY     5710
#define IDC_BTN_PRO_INVENTORY    5711

// --- Log viewer: new controls ---
#define IDC_LOG_FILE_COMBO       5900
#define IDC_LOG_FILTER_EDIT      5901
#define IDC_BTN_APPLY_FILTER     5902
#define IDC_CHK_AUTO_REFRESH     5903

// --- Network: new controls ---
#define IDC_BTN_COPY_IP          6001
#define IDC_NET_MODE_LABEL       6002

// --- Status tab: repop + announce ---
#define IDC_BTN_REPOP_ZONES      6100
#define IDC_ANNOUNCE_EDIT        6101
#define IDC_BTN_SEND_ANNOUNCE    6102

// --- Server tab: Rule Editor ---
#define IDC_RULE_SEARCH          6200
#define IDC_RULE_LIST            6201
#define IDC_RULE_VALUE           6202
#define IDC_BTN_LOAD_RULES       6203
#define IDC_BTN_SAVE_RULE        6204
#define IDC_BTN_RESET_RULE       6205
#define IDC_RULE_SELECTED        6206

// --- Server tab: XP Slider ---
#define IDC_XP_SLIDER            6210
#define IDC_XP_LABEL             6211
#define IDC_BTN_APPLY_XP         6212
#define IDC_AA_SLIDER            6213
#define IDC_AA_LABEL             6214
#define IDC_BTN_APPLY_AA         6215

// --- Server tab: Guild Manager ---
#define IDC_GUILD_LIST           6220
#define IDC_GUILD_NAME           6221
#define IDC_GUILD_LEADER         6222
#define IDC_BTN_LIST_GUILDS      6223
#define IDC_BTN_CREATE_GUILD     6224
#define IDC_BTN_SET_GUILD_LEADER 6225
#define IDC_BTN_DISBAND_GUILD    6226
#define IDC_BTN_VIEW_ROSTER      6227
#define IDC_SERVER_RESULT        6228

// --- Pro Tools: Spawn Boss ---
#define IDC_SPAWN_BOSS_COMBO     6300
#define IDC_SPAWN_ZONE_EDIT      6301
#define IDC_BTN_SPAWN_BOSS       6302

// --- Pro Tools: Spell Scriber ---
#define IDC_SPELL_SEARCH         6310
#define IDC_BTN_SPELL_SEARCH     6311
#define IDC_SPELL_LIST           6312
#define IDC_BTN_SCRIBE_SPELL     6313
#define IDC_BTN_SCRIBE_ALL       6314

// --- Pro Tools: Faction Editor ---
#define IDC_BTN_LOAD_FACTIONS    6320
#define IDC_FACTION_LIST         6321
#define IDC_FACTION_VALUE        6322
#define IDC_BTN_SET_FACTION      6323
#define IDC_BTN_FACTION_ALLY     6324
#define IDC_BTN_FACTION_WARMLY   6325
#define IDC_BTN_FACTION_INDIFF   6326
#define IDC_BTN_FACTION_KOS      6327

// --- Pro Tools: Skill Maxer ---
#define IDC_BTN_MAX_SKILLS       6330

// --- Player Tools: Loot Viewer ---
#define IDC_LOOT_SEARCH          6340
#define IDC_BTN_LOOT_BY_NPC      6341
#define IDC_BTN_LOOT_BY_ITEM     6342

// --- Pro Tools: Skill Editor ---
#define IDC_SKILL_SEARCH         6350
#define IDC_BTN_SKILL_SEARCH     6351
#define IDC_SKILL_LIST           6352
#define IDC_SKILL_VALUE          6353
#define IDC_BTN_SET_SKILL        6354
#define IDC_BTN_LOAD_SKILLS      6355

// --- Admin Tools: GM/GodMode toggles ---
#define IDC_ADM_GM_CHAR          6400
#define IDC_BTN_TOGGLE_GM        6401
#define IDC_BTN_TOGGLE_GODMODE   6402

// --- Server Tab: Weather ---
#define IDC_BTN_WEATHER_ON       6410
#define IDC_BTN_WEATHER_OFF      6411

// --- Backup: Clone Character ---
#define IDC_CLONE_SOURCE         6420
#define IDC_CLONE_NEWNAME        6421
#define IDC_BTN_CLONE_CHAR       6422
#define IDC_BTN_DB_SIZE          6423

// --- Status: Zone Management ---
#define IDC_ZONE_LIST            6500
#define IDC_BTN_REFRESH_ZONES    6501
#define IDC_BTN_STOP_ZONE        6502
#define IDC_BTN_RESTART_ZONE     6503
#define IDC_ZONE_START_EDIT      6504
#define IDC_BTN_START_ZONE       6505

// --- Status: Boss Management ---
#define IDC_BTN_CHECK_BOSS       6510
#define IDC_BTN_DESPAWN_BOSS     6511
#define IDC_BTN_LIST_ACTIVE_BOSS 6512
#define IDC_STATUS_RESULT        6513
#define IDC_ZONE_FIND_EDIT       6514
#define IDC_BTN_FIND_ZONE_STATUS 6515
#define IDC_ZEM_VALUE            6516
#define IDC_BTN_ZEM_SAVE         6517
#define IDC_BTN_ZEM_DEFAULT      6518

// --- Server: Zone Environment (weather/fog/clip) ---
#define IDC_ENV_ZONE_EDIT        6520
#define IDC_BTN_ENV_LOAD         6521
#define IDC_ENV_WEATHER_COMBO    6522
#define IDC_ENV_FOG_MIN          6523
#define IDC_ENV_FOG_MAX          6524
#define IDC_ENV_CLIP_MIN         6525
#define IDC_ENV_CLIP_MAX         6526
#define IDC_BTN_ENV_SAVE         6527
#define IDC_ENV_FOG_DENSITY      6528
#define IDC_ENV_FOG_R            6529
#define IDC_ENV_FOG_G            6530
#define IDC_ENV_FOG_B            6531
#define IDC_BTN_ENV_DEFAULT      6532
#define IDC_ENV_FIND_EDIT        6533
#define IDC_BTN_ENV_FIND_ZONE    6534

// --- Status tab: server name + login marquee ---
#define IDC_STATUS_SERVERNAME_EDIT  6540
#define IDC_BTN_SET_SERVERNAME      6541
#define IDC_STATUS_MARQUEE_EDIT     6542
#define IDC_BTN_SET_MARQUEE         6543

// --- Timers ---
#define TIMER_LOG_REFRESH        2

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

// Playable race lookup (non-contiguous IDs)
struct RaceEntry { int id; const wchar_t* name; };
static const RaceEntry RACE_TABLE[] = {
    { 1,   L"Human" },     { 2,  L"Barbarian" }, { 3,  L"Erudite" },
    { 4,   L"Wood Elf" },  { 5,  L"High Elf" },  { 6,  L"Dark Elf" },
    { 7,   L"Half Elf" },  { 8,  L"Dwarf" },      { 9,  L"Troll" },
    { 10,  L"Ogre" },      { 11, L"Halfling" },   { 12, L"Gnome" },
    { 128, L"Iksar" },     { 130,L"Vah Shir" },
};
static const int RACE_TABLE_COUNT = 14;

// Notable raid boss / named NPC lookup for spawn-on-demand
struct BossEntry { int npcId; const wchar_t* name; const wchar_t* zone; };
static const BossEntry BOSS_TABLE[] = {
    { 32040, L"Lord Nagafen",            L"nagafen"     },
    { 32018, L"Lady Vox",                L"permafrost"  },
    { 64001, L"Phinigel Autropos",       L"kedge"       },
    { 72003, L"Innoruuk",                L"fearplane"   },
    { 73057, L"Cazic-Thule",             L"cazicthule"  },
    { 87007, L"Gorenaire",               L"dreadlands"  },
    { 76020, L"Venril Sathir",           L"karnor"      },
    { 70005, L"Trakanon",                L"trakanon"    },
    { 91009, L"King Tormax",             L"kael"        },
    { 85208, L"Talendor",                L"westwastes"  },
    { 97002, L"Vulak'Aerr",              L"veeshan"     },
    { 117058,L"The Keeper of Souls",     L"potorment"   },
};
static const int BOSS_TABLE_COUNT = 12;

// Playable class names (IDs 1-15, index = id-1)
static const wchar_t* CLASS_NAMES[15] = {
    L"Warrior", L"Cleric", L"Paladin", L"Ranger", L"Shadow Knight",
    L"Druid", L"Monk", L"Bard", L"Rogue", L"Shaman",
    L"Necromancer", L"Wizard", L"Magician", L"Enchanter", L"Beastlord"
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

// Dark mode state (hoisted — used by PanelProc and paint code compiled before panels)
static bool      g_darkMode     = false;
static HBRUSH    g_hbrDark      = nullptr;   // panel/window background brush
static HBRUSH    g_hbrDarkCtl   = nullptr;   // edit/listbox background brush
static const COLORREF CLR_DARK_BG  = RGB(30, 30, 30);
static const COLORREF CLR_DARK_CTL = RGB(45, 45, 48);
static const COLORREF CLR_DARK_TXT = RGB(220, 220, 220);
static const COLORREF CLR_DARK_ACCENT  = RGB(60, 60, 65);   // button/tab face
static const COLORREF CLR_DARK_BORDER  = RGB(80, 80, 85);   // subtle borders
static HBRUSH    g_hbrDarkAccent = nullptr;  // button/tab face brush

// Status tab handles (hoisted)
static HWND g_hwndStateLabel   = nullptr;
static HWND g_hwndUptimeLabel  = nullptr;
static HWND g_hwndVersionLabel   = nullptr;
static HWND g_hwndServerNameEdit = nullptr;
static HWND g_hwndMarqueeEdit    = nullptr;
static HWND g_hwndProcList     = nullptr;
static HWND g_hwndMotdEdit     = nullptr;
static HWND g_hwndPlayerCount  = nullptr;

// Player Tools panel handles (declared here so monitoring functions can reference them)
static HWND g_hwndPlrAccount   = nullptr;
static HWND g_hwndPlrCharName  = nullptr;
static HWND g_hwndPlrZone      = nullptr;
static HWND g_hwndPlrAmount    = nullptr;   // kept for IDC compat, unused in panel
static HWND g_hwndPlrResult    = nullptr;
static HWND g_hwndPlrSearchMin = nullptr;
static HWND g_hwndPlrSearchMax = nullptr;
static HWND g_hwndPlrSearchClass = nullptr;

// Pro Tools panel handles (hoisted for cross-tab access)
static HWND g_hwndProCharName    = nullptr;
static HWND g_hwndProLevelCbo    = nullptr;
static HWND g_hwndProAaEdit      = nullptr;
static HWND g_hwndProClassCbo    = nullptr;
static HWND g_hwndProRaceCbo     = nullptr;
static HWND g_hwndProGenderCbo   = nullptr;
static HWND g_hwndProPlatAmount  = nullptr;
static HWND g_hwndProNewName     = nullptr;
static HWND g_hwndProSurname     = nullptr;
static HWND g_hwndProTitleCbo    = nullptr;

// Era/zone HWNDs used by Status tab and Pro Tools operations
static HWND g_hwndGameEraCbo   = nullptr;
static HWND g_hwndGameZoneCbo  = nullptr;
static HWND g_hwndGameEraCur   = nullptr;
static HWND g_hwndGameZoneCur  = nullptr;
static bool g_eraUserDirty     = false;  // true when user changed era dropdown but hasn't applied yet

// Admin panel handles
static HWND g_hwndAdmResult    = nullptr;
static HWND g_hwndAdmDelChar   = nullptr;

// Log viewer handles
static HWND g_hwndLogText      = nullptr;
static HWND g_hwndLogLines     = nullptr;
static HWND g_hwndLogFilter    = nullptr;
static HWND g_hwndLogFileCombo = nullptr;
static std::wstring g_logFullText;           // unfiltered log content for filtering

// Network handles
static HWND g_hwndNetModeLabel = nullptr;

// Advanced handles
static HWND g_hwndAdvResult    = nullptr;
static HWND g_hwndAdvSysResult = nullptr;

// Server tab handles (Rule Editor + Guild Manager + XP)
static HWND g_hwndRuleSearch   = nullptr;
static HWND g_hwndRuleList     = nullptr;
static HWND g_hwndRuleValue    = nullptr;
static HWND g_hwndRuleSelected = nullptr;
static HWND g_hwndGuildList    = nullptr;
static HWND g_hwndGuildName    = nullptr;
static HWND g_hwndGuildLeader  = nullptr;
static HWND g_hwndXpSlider     = nullptr;
static HWND g_hwndXpLabel      = nullptr;
static HWND g_hwndAaSlider     = nullptr;
static HWND g_hwndAaLabel      = nullptr;
static HWND g_hwndServerResult = nullptr;

// Rule editor cached data
struct RuleInfo { std::wstring name; std::wstring value; std::wstring origValue; };
static std::vector<RuleInfo> g_rules;
static std::vector<RuleInfo> g_rulesFiltered;

// Pro Tools new handles
static HWND g_hwndSpellSearch  = nullptr;
static HWND g_hwndSpellList    = nullptr;
static HWND g_hwndFactionList  = nullptr;
static HWND g_hwndFactionValue = nullptr;
static HWND g_hwndLootSearch   = nullptr;
static HWND g_hwndAnnounceEdit = nullptr;
static HWND g_hwndSkillSearch  = nullptr;
static HWND g_hwndSkillList    = nullptr;
static HWND g_hwndSkillValue   = nullptr;
static HWND g_hwndAdmGMChar    = nullptr;
static HWND g_hwndCloneSource  = nullptr;
static HWND g_hwndCloneNewName = nullptr;

// Zone management handles
static HWND g_hwndZoneList     = nullptr;
static HWND g_hwndZoneStartEdit = nullptr;
static HWND g_hwndZoneFindEdit = nullptr;
static HWND g_hwndStatusResult = nullptr;
static HWND g_hwndZoneResult   = nullptr;
static HWND g_hwndZemValue    = nullptr;
static HWND g_hwndZemLabel    = nullptr;

// Zone environment handles (Server tab)
static HWND g_hwndEnvZoneEdit  = nullptr;
static HWND g_hwndEnvWeatherCbo = nullptr;
static HWND g_hwndEnvFogMin    = nullptr;
static HWND g_hwndEnvFogMax    = nullptr;
static HWND g_hwndEnvClipMin   = nullptr;
static HWND g_hwndEnvClipMax   = nullptr;
static HWND g_hwndEnvFogDensity = nullptr;
static HWND g_hwndEnvFogR      = nullptr;
static HWND g_hwndEnvFogG      = nullptr;
static HWND g_hwndEnvFogB      = nullptr;
static HWND g_hwndEnvFindEdit  = nullptr;

// Large font for player count banner
static HFONT g_hFontLarge      = nullptr;

static std::atomic<bool> g_stopPolling{ false };

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

// Locate Docker Desktop.exe using three strategies:
//   1. Registry: Docker Inc. install key (covers custom drive letters)
//   2. Registry: Windows Uninstall key InstallLocation
//   3. Common paths enumerated across all logical drive letters
//   4. Caller falls back to ShellExecute by name if empty string returned
static std::wstring FindDockerDesktopExe() {
    const struct { HKEY root; const wchar_t* subkey; const wchar_t* value; } regLocations[] = {
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Docker Inc.\\Docker Desktop",                                           L"InstallPath"     },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Docker Inc.\\Docker Desktop",                              L"InstallPath"     },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Docker Desktop",         L"InstallLocation" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Docker Desktop", L"InstallLocation" },
        { HKEY_CURRENT_USER,  L"SOFTWARE\\Docker Inc.\\Docker Desktop",                                           L"InstallPath"     },
    };
    for (auto& loc : regLocations) {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(loc.root, loc.subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t buf[MAX_PATH] = {};
            DWORD size = sizeof(buf);
            DWORD type = 0;
            if (RegQueryValueExW(hKey, loc.value, nullptr, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS
                && (type == REG_SZ || type == REG_EXPAND_SZ) && buf[0]) {
                RegCloseKey(hKey);
                std::wstring path = buf;
                if (!path.empty() && (path.back() == L'\\' || path.back() == L'/'))
                    path.pop_back();
                std::wstring exe = path;
                if (_wcsicmp(exe.size() >= 4 ? exe.c_str() + exe.size() - 4 : L"", L".exe") != 0)
                    exe += L"\\Docker Desktop.exe";
                if (PathFileExistsW(exe.c_str()))
                    return exe;
            } else {
                RegCloseKey(hKey);
            }
        }
    }
    // Enumerate all drive letters and check common paths
    wchar_t drives[256] = {};
    GetLogicalDriveStringsW(255, drives);
    const wchar_t* relPaths[] = {
        L"Program Files\\Docker\\Docker\\Docker Desktop.exe",
        L"Program Files (x86)\\Docker\\Docker\\Docker Desktop.exe",
        nullptr
    };
    for (wchar_t* d = drives; *d; d += wcslen(d) + 1) {
        for (int i = 0; relPaths[i]; ++i) {
            std::wstring full = std::wstring(d) + relPaths[i];
            if (PathFileExistsW(full.c_str()))
                return full;
        }
    }
    return L"";  // caller should ShellExecute by name as last resort
}

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

static std::wstring TrimRight(std::wstring s) {
    while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' ' || s.back() == L'\t'))
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

// RunQueryTable: same as RunQuery but passes --table for aligned ASCII box output.
// Use this for any result shown directly to the user; use RunQuery only when
// parsing the raw output programmatically.
static std::wstring RunQueryTable(const std::wstring& sql) {
    std::wstring cmd = std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb --table -e \"" + sql + L"\" quarm";
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

static std::wstring ReadTextFileWide(const wchar_t* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return L"";
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return ToWide(content);
}

static bool WriteTextFileWide(const wchar_t* path, const std::wstring& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    std::string bytes(text.begin(), text.end());
    f.write(bytes.data(), (std::streamsize)bytes.size());
    return true;
}

static std::wstring GetBuildSignaturePath() {
    wchar_t path[MAX_PATH];
    wcscpy_s(path, g_installDir);
    PathAppendW(path, L".quarmdocker_build_signature");
    return path;
}

static std::wstring GetRebuildInputSignature() {
    const wchar_t* relPaths[] = {
        L"Dockerfile",
        L"docker-compose.yml",
        L"init.sh",
        L"entrypoint.sh",
        nullptr
    };

    std::wstring manifest;
    for (int i = 0; relPaths[i]; ++i) {
        wchar_t fullPath[MAX_PATH];
        wcscpy_s(fullPath, g_installDir);
        PathAppendW(fullPath, relPaths[i]);
        manifest += relPaths[i];
        manifest += L":";
        std::wstring content = ReadTextFileWide(fullPath);
        manifest += content.empty() ? L"(missing)" : ComputeSHA1Hex(content);
        manifest += L"\n";
    }
    return ComputeSHA1Hex(manifest);
}

static std::wstring LoadSavedBuildSignature() {
    return TrimRight(ReadTextFileWide(GetBuildSignaturePath().c_str()));
}

static void SaveBuildSignature(const std::wstring& signature) {
    if (!signature.empty())
        WriteTextFileWide(GetBuildSignaturePath().c_str(), signature);
}

static bool HasComposeBuildImage() {
    std::string imageId = TrimRight(RunCommand(L"docker compose images -q quarm", g_installDir));
    return !imageId.empty() && imageId.find("Error") == std::string::npos;
}

static bool IsContainerRunning();

static std::string BaseNameOfPath(std::string path) {
    auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

static std::vector<std::string> SplitProcessArgs(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ls(line);
    std::string token;
    while (ls >> token)
        tokens.push_back(token);
    return tokens;
}

static bool IsDynamicLauncherName(const std::string& token) {
    return token.rfind("dynamic_", 0) == 0;
}

static std::vector<std::string> SplitTabLine(const std::string& line) {
    std::vector<std::string> cols;
    std::istringstream ls(line);
    std::string col;
    while (std::getline(ls, col, '\t'))
        cols.push_back(col);
    return cols;
}

static bool TableExists(const wchar_t* tableName) {
    std::string out = TrimRight(RunCommand(
        std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"SHOW TABLES LIKE '" + std::wstring(tableName) + L"'\" quarm"));
    return out == std::string(tableName, tableName + wcslen(tableName));
}

static std::vector<std::string> GetTableColumns(const wchar_t* tableName) {
    std::vector<std::string> cols;
    std::string out = TrimRight(RunCommand(
        std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"SHOW COLUMNS FROM " + std::wstring(tableName) + L"\" quarm"));
    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line = TrimRight(line);
        if (line.empty()) continue;
        std::vector<std::string> row = SplitTabLine(line);
        if (!row.empty() && !row[0].empty())
            cols.push_back(row[0]);
    }
    return cols;
}

static bool HasColumn(const std::vector<std::string>& cols, const char* name) {
    return std::find(cols.begin(), cols.end(), std::string(name)) != cols.end();
}

static std::string FindFirstColumn(const std::vector<std::string>& cols,
                                   std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (HasColumn(cols, name))
            return name;
    }
    return "";
}

static std::map<std::wstring, int> GetRunningZoneCountsFromWorldStateTable() {
    std::map<std::wstring, int> zoneCounts;
    if (TableExists(L"webdata_servers")) {
        std::wstring staticSql =
            L"SELECT name, COUNT(*) "
            L"FROM webdata_servers "
            L"WHERE connected = 1 "
            L"AND name IS NOT NULL "
            L"AND name <> '' "
            L"AND name <> 'LoginServer' "
            L"AND name NOT LIKE 'dynamic\\\\_%' "
            L"AND name NOT LIKE 'dynzone%' "
            L"GROUP BY name "
            L"ORDER BY name";

        std::string out = TrimRight(RunCommand(
            std::wstring(L"docker exec ") + CONTAINER +
            L" mariadb -N -e \"" + staticSql + L"\" quarm"));

        std::istringstream ss(out);
        std::string line;
        while (std::getline(ss, line)) {
            line = TrimRight(line);
            if (line.empty()) continue;
            std::vector<std::string> row = SplitTabLine(line);
            if (row.size() < 2) continue;
            if (row[0].empty()) continue;
            zoneCounts[ToWide(row[0])] += atoi(row[1].c_str());
        }
    }

    if (TableExists(L"webdata_character")) {
        std::wstring playerSql =
            L"SELECT z.short_name, COUNT(*) "
            L"FROM webdata_character wc "
            L"JOIN character_data cd ON cd.id = wc.id "
            L"JOIN zone z ON z.zoneidnumber = cd.zone_id "
            L"WHERE wc.last_seen = 0 "
            L"GROUP BY z.short_name "
            L"ORDER BY z.short_name";

        std::string playerOut = TrimRight(RunCommand(
            std::wstring(L"docker exec ") + CONTAINER +
            L" mariadb -N -e \"" + playerSql + L"\" quarm"));

        std::istringstream pss(playerOut);
        std::string line;
        while (std::getline(pss, line)) {
            line = TrimRight(line);
            if (line.empty()) continue;
            std::vector<std::string> row = SplitTabLine(line);
            if (row.size() < 2) continue;
            if (row[0].empty()) continue;
            int players = atoi(row[1].c_str());
            if (players > 0) {
                int& count = zoneCounts[ToWide(row[0])];
                if (players > count)
                    count = players;
            }
        }
    }

    return zoneCounts;
}

struct LiveZoneProcessInfo {
    std::wstring zoneName;
    int port = 0;
    int pid = 0;
};

static bool ParseZoneLogFileName(const std::string& path, LiveZoneProcessInfo& info) {
    std::string base = path;
    size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos)
        base = base.substr(slash + 1);
    if (base.size() <= 4 || base.substr(base.size() - 4) != ".log")
        return false;
    base.resize(base.size() - 4);

    size_t pidSep = base.rfind('_');
    if (pidSep == std::string::npos || pidSep == 0 || pidSep + 1 >= base.size())
        return false;

    size_t portSep = base.rfind("_port_");
    if (portSep == std::string::npos || portSep == 0 || portSep + 6 >= pidSep)
        return false;

    std::string zoneName = base.substr(0, portSep);
    std::string portStr = base.substr(portSep + 6, pidSep - (portSep + 6));
    std::string pidStr = base.substr(pidSep + 1);
    if (zoneName.empty() || portStr.empty() || pidStr.empty())
        return false;

    for (char c : portStr) if (!isdigit(static_cast<unsigned char>(c))) return false;
    for (char c : pidStr) if (!isdigit(static_cast<unsigned char>(c))) return false;

    info.zoneName = ToWide(zoneName);
    info.port = atoi(portStr.c_str());
    info.pid = atoi(pidStr.c_str());
    return info.port > 0 && info.pid > 0;
}

static std::vector<LiveZoneProcessInfo> GetLiveZoneProcessesFromLogs() {
    std::vector<LiveZoneProcessInfo> processes;
    if (!IsContainerRunning())
        return processes;

    std::string psOut = TrimRight(RunCommand(
        std::wstring(L"docker exec ") + CONTAINER +
        L" sh -c \"ps -eo pid=,args=\""));

    std::set<int> liveZonePids;
    std::istringstream pss(psOut);
    std::string line;
    while (std::getline(pss, line)) {
        line = TrimRight(line);
        auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            continue;
        line.erase(0, first);

        size_t split = line.find_first_of(" \t");
        if (split == std::string::npos)
            continue;

        std::string pidStr = line.substr(0, split);
        std::string args = line.substr(split + 1);
        if (pidStr.empty() || args.empty())
            continue;

        bool digitsOnly = true;
        for (char c : pidStr) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                digitsOnly = false;
                break;
            }
        }
        if (!digitsOnly)
            continue;

        std::vector<std::string> tokens = SplitProcessArgs(args);
        if (tokens.empty())
            continue;
        if (BaseNameOfPath(tokens[0]) != "zone")
            continue;

        liveZonePids.insert(atoi(pidStr.c_str()));
    }

    if (liveZonePids.empty())
        return processes;

    std::string filesOut = TrimRight(RunCommand(
        std::wstring(L"docker exec ") + CONTAINER +
        L" sh -c \"find /src/build/bin/logs/zone /quarm/logs/zone /logs/zone "
        L"-maxdepth 1 -type f -name '*_port_*_*.log' 2>/dev/null\""));

    std::set<std::pair<std::wstring, int>> seen;
    std::istringstream fss(filesOut);
    while (std::getline(fss, line)) {
        line = TrimRight(line);
        if (line.empty())
            continue;

        LiveZoneProcessInfo info;
        if (!ParseZoneLogFileName(line, info))
            continue;
        if (liveZonePids.find(info.pid) == liveZonePids.end())
            continue;
        if (!seen.insert(std::make_pair(info.zoneName, info.pid)).second)
            continue;

        processes.push_back(info);
    }

    return processes;
}

static std::map<std::wstring, std::vector<int>> GetLiveZonePidsByName() {
    std::map<std::wstring, std::vector<int>> zonePids;
    for (const auto& info : GetLiveZoneProcessesFromLogs())
        zonePids[info.zoneName].push_back(info.pid);
    return zonePids;
}

static std::string ExtractZoneShortNameFromArgs(const std::string& line) {
    std::vector<std::string> tokens = SplitProcessArgs(line);
    if (tokens.size() < 2) return "";

    size_t exeIndex = std::string::npos;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (BaseNameOfPath(tokens[i]) == "zone") {
            exeIndex = i;
            break;
        }
    }
    if (exeIndex == std::string::npos || exeIndex + 1 >= tokens.size())
        return "";

    for (size_t i = tokens.size(); i > exeIndex + 1; --i) {
        const std::string& candidate = tokens[i - 1];
        if (candidate.empty()) continue;
        if (candidate[0] == '-') continue;
        if (IsDynamicLauncherName(candidate)) continue;
        return candidate;
    }

    return "";
}

static std::map<std::wstring, int> GetRunningZoneProcessCounts() {
    std::map<std::wstring, int> zoneCounts;
    for (const auto& info : GetLiveZoneProcessesFromLogs())
        zoneCounts[info.zoneName]++;
    if (!zoneCounts.empty())
        return zoneCounts;

    zoneCounts = GetRunningZoneCountsFromWorldStateTable();
    if (!zoneCounts.empty())
        return zoneCounts;

    zoneCounts.clear();
    std::string psOut = RunCommand(std::wstring(L"docker exec ") + CONTAINER + L" ps -eo args");
    std::istringstream ss(psOut);
    std::string line;

    while (std::getline(ss, line)) {
        line = TrimRight(line);
        auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        line.erase(0, first);

        std::string zone = ExtractZoneShortNameFromArgs(line);
        if (zone.empty()) continue;
        zoneCounts[ToWide(zone)]++;
    }

    return zoneCounts;
}

static std::map<std::wstring, std::pair<std::wstring, std::wstring>>
GetZoneMetadata(const std::map<std::wstring, int>& zoneCounts) {
    std::map<std::wstring, std::pair<std::wstring, std::wstring>> meta;
    if (zoneCounts.empty() || !IsContainerRunning())
        return meta;

    std::wstring sql =
        L"SELECT short_name, long_name, zone_exp_multiplier "
        L"FROM zone WHERE short_name IN (";
    bool first = true;
    for (const auto& entry : zoneCounts) {
        if (!first) sql += L",";
        sql += L"'";
        for (wchar_t c : entry.first) {
            if (c == L'\'') sql += L"''";
            else sql += c;
        }
        sql += L"'";
        first = false;
    }
    sql += L") ORDER BY long_name";

    std::string out = TrimRight(RunCommand(
        std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + sql + L"\" quarm"));

    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line = TrimRight(line);
        if (line.empty()) continue;

        std::vector<std::string> cols;
        std::istringstream ls(line);
        std::string col;
        while (std::getline(ls, col, '\t'))
            cols.push_back(col);
        if (cols.size() < 3) continue;

        meta[ToWide(cols[0])] = { ToWide(cols[1]), ToWide(cols[2]) };
    }

    return meta;
}

static void SetServerPanelResult(const std::wstring& text) {
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult, text.c_str());
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
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
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
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_STATUS], IDC_BTN_START),          !busy);
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_STATUS], IDC_BTN_STOP),           !busy);
        EnableWindow(GetDlgItem(g_hwndPanels[TAB_STATUS], IDC_BTN_RESTART_SERVER), !busy);
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

// ============================================================
// FORWARD DECLARATIONS (functions defined later, called earlier)
// ============================================================

static bool CheckServerRunning(const wchar_t* title);
static int  IsCharacterOnline(const wchar_t* charName);
static bool GetNoBackupOnStop();
static void SetPlrResult(const std::wstring& text);
static void SetGameResult(const std::wstring& text);
static void DoRestartServerAsync();
static void DoRefreshZones();
static std::wstring ExtractZoneFromList();

// ============================================================
// TAB 1 — STATUS PANEL
// ============================================================

static void CreateStatusPanel(HWND parent) {
    int y = 6;
    MakeLabel(parent, L"Server:", 20, y+2, 50, 22);
    g_hwndStateLabel = MakeLabel(parent, L"Checking...", 76, y, 140, 24, SS_SUNKEN);
    g_hwndVersionLabel = MakeLabel(parent, APP_VERSION, 780, y+2, 60, 20);
    y += 26;
    MakeLabel(parent, L"Uptime:", 20, y+2, 50, 20);
    g_hwndUptimeLabel = MakeLabel(parent, L"", 76, y+2, 200, 20);
    y += 24;
    MakeLabel(parent, L"Players Online:", 20, y+2, 110, 22);
    g_hwndPlayerCount = MakeLabel(parent, L"0", 136, y, 50, 24);
    y += 28;
    MakeButton(parent, L"Start Server",   IDC_BTN_START,          20,  y, 106, 28);
    MakeButton(parent, L"Stop Server",    IDC_BTN_STOP,           134, y, 106, 28);
    MakeButton(parent, L"Restart Server", IDC_BTN_RESTART_SERVER, 248, y, 120, 28);
    y += 34;
    // Services
    MakeLabel(parent, L"Services:", 20, y, 60, 18);
    y += 18;
    g_hwndProcList = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        20, y, 500, 150, parent,
        (HMENU)(UINT_PTR)IDC_STATUS_PROCESSES, g_hInst, nullptr);
    if (g_hwndProcList && g_hFontMono)
        SendMessage(g_hwndProcList, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
    y += 156;
    // Boss Section
    MakeLabel(parent, L"Boss:", 20, y+4, 36, 20);
    HWND cboBoss = MakeCombo(parent, IDC_SPAWN_BOSS_COMBO, 60, y, 210, 400);
    for (int i = 0; i < BOSS_TABLE_COUNT; ++i)
        SendMessageW(cboBoss, CB_ADDSTRING, 0, (LPARAM)BOSS_TABLE[i].name);
    MakeButton(parent, L"Check",   IDC_BTN_CHECK_BOSS,       280, y, 56, 26);
    MakeButton(parent, L"Spawn",   IDC_BTN_SPAWN_BOSS,       342, y, 56, 26);
    MakeButton(parent, L"Despawn", IDC_BTN_DESPAWN_BOSS,     404, y, 66, 26);
    MakeButton(parent, L"List Active Bosses", IDC_BTN_LIST_ACTIVE_BOSS, 476, y, 140, 26);
    y += 30;
    g_hwndStatusResult = MakeResultBox(parent, IDC_STATUS_RESULT, 20, y, 940, 130);
}

// Forward declarations for server name / marquee loaders used in RefreshStatusTab
static void DoLoadServerName();
static void DoLoadMarquee();

static void RefreshEraZone() {
    // Refresh era/zone "Current:" labels on Status tab — only when server is up
    if (!IsContainerRunning()) {
        if (g_hwndGameEraCur)  SetWindowTextW(g_hwndGameEraCur,  L"(server off)");
        if (g_hwndGameZoneCur) SetWindowTextW(g_hwndGameZoneCur, L"(server off)");
        return;
    }
    std::wstring eraResult = RunQuery(
        L"SELECT rule_value FROM rule_values WHERE rule_name='World:CurrentExpansion'");
    if (eraResult != L"(no results)") {
        std::wstring d = L"(unknown)";
        if      (eraResult.find(L"-1") != std::wstring::npos) d = L"All";
        else if (eraResult.find(L"4")  != std::wstring::npos) d = L"PoP";
        else if (eraResult.find(L"3")  != std::wstring::npos) d = L"Luclin";
        else if (eraResult.find(L"2")  != std::wstring::npos) d = L"Velious";
        else if (eraResult.find(L"1")  != std::wstring::npos) d = L"Kunark";
        else if (eraResult.find(L"0")  != std::wstring::npos) d = L"Classic";
        if (g_hwndGameEraCur) SetWindowTextW(g_hwndGameEraCur, d.c_str());
        // Select matching item in era dropdown
        int eraIdx = -1;
        if (d == L"Classic") eraIdx = 0;
        else if (d == L"Kunark") eraIdx = 1;
        else if (d == L"Velious") eraIdx = 2;
        else if (d == L"Luclin") eraIdx = 3;
        else if (d == L"PoP") eraIdx = 4;
        else if (d == L"All") eraIdx = 5;
        if (eraIdx >= 0 && g_hwndGameEraCbo && !g_eraUserDirty)
            SendMessage(g_hwndGameEraCbo, CB_SETCURSEL, eraIdx, 0);
    }
    std::wstring zoneResult = RunQuery(L"SELECT dynamics FROM launcher LIMIT 1");
    if (zoneResult != L"(no results)") {
        std::wstring num; bool nl = false;
        for (auto c : zoneResult) {
            if (c == L'\n' || c == L'\r') { nl = true; continue; }
            if (nl && iswdigit(c)) num += c;
            else if (nl && !num.empty()) break;
        }
        if (!num.empty() && g_hwndGameZoneCur)
            SetWindowTextW(g_hwndGameZoneCur, num.c_str());
    }
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
        // Active player count (characters logged in within last 5 minutes)
        std::wstring cntResult = RunQuery(
            L"SELECT COUNT(*) FROM character_data "
            L"WHERE last_login > UNIX_TIMESTAMP(NOW() - INTERVAL 5 MINUTE)");
        std::wstring cnt;
        for (auto c : cntResult) { if (iswdigit(c)) cnt += c; }
        if (g_hwndPlayerCount)
            SetWindowTextW(g_hwndPlayerCount, cnt.empty() ? L"0" : cnt.c_str());
        // Load current MOTD into the edit field (only if user hasn't typed in it)
        if (g_hwndMotdEdit && GetWindowTextLengthW(g_hwndMotdEdit) == 0) {
            std::wstring motdResult = RunQuery(
                L"SELECT rule_value FROM rule_values WHERE rule_name='World:MOTD'");
            // Strip header rows from raw query result
            std::wstring motdVal;
            bool pastHeader = false;
            for (auto c : motdResult) {
                if (c == L'\n') { pastHeader = true; continue; }
                if (c == L'\r') continue;
                if (pastHeader) motdVal += c;
            }
            while (!motdVal.empty() && (motdVal.back() == L' ' || motdVal.back() == L'\n'))
                motdVal.pop_back();
            if (!motdVal.empty() && motdVal != L"(no results)")
                SetWindowTextW(g_hwndMotdEdit, motdVal.c_str());
        }
        // Load server name and marquee (only once when fields are empty)
        if (g_hwndServerNameEdit && GetWindowTextLengthW(g_hwndServerNameEdit) == 0)
            DoLoadServerName();
        if (g_hwndMarqueeEdit && GetWindowTextLengthW(g_hwndMarqueeEdit) == 0)
            DoLoadMarquee();
        // Zone list refreshes only on explicit Refresh click, not every poll
    } else {
        SetWindowTextW(g_hwndStateLabel, L"STOPPED");
        SetWindowTextW(g_hwndUptimeLabel, L"");
        SetWindowTextW(g_hwndProcList, L"(server is not running)");
        if (g_hwndPlayerCount) SetWindowTextW(g_hwndPlayerCount, L"\x2014");
        if (g_hwndZoneList) SendMessage(g_hwndZoneList, LB_RESETCONTENT, 0, 0);
        SetStatus(L"Server is stopped");
    }
    RefreshEraZone();
}

static void DoSetMOTD() {
    if (!CheckServerRunning(L"Set MOTD")) return;
    wchar_t motd[512] = {};
    if (g_hwndMotdEdit) GetWindowTextW(g_hwndMotdEdit, motd, 512);
    // Escape single quotes for SQL safety
    std::wstring safe;
    for (wchar_t c : std::wstring(motd)) {
        if (c == L'\'') safe += L"''";
        else safe += c;
    }
    RunQuery(L"UPDATE rule_values SET rule_value='" + safe +
             L"' WHERE rule_name='World:MOTD'");
    if (motd[0])
        SetStatus(L"MOTD set \x2014 players will see it on next zone-in.");
    else
        SetStatus(L"MOTD cleared.");
}

// ============================================================
// TAB 4 — ADMIN TOOLS PANEL
// ============================================================

static HWND g_hwndAdmAccount  = nullptr;
static HWND g_hwndAdmPassword = nullptr;

static void CreateAdminPanel(HWND parent) {
    int y = 12;

    MakeLabel(parent, L"Account Management:", 20, y, 160, 20);
    y += 26;

    MakeLabel(parent, L"Account:", 20, y+4, 70, 20);
    g_hwndAdmAccount = MakeEdit(parent, IDC_ADM_ACCOUNT, 96, y, 190, 24);
    MakeButton(parent, L"Make GM",    IDC_BTN_MAKE_GM,    296, y, 85, 26);
    MakeButton(parent, L"Remove GM",  IDC_BTN_REMOVE_GM,  390, y, 90, 26);
    y += 34;

    MakeButton(parent, L"List Accounts",  IDC_BTN_LIST_ACCOUNTS,   96, y, 120, 26);
    MakeButton(parent, L"Reset Password", IDC_BTN_RESET_PASSWORD,  226, y, 125, 26);
    MakeButton(parent, L"Suspend",        IDC_BTN_SUSPEND_ACCT,    360, y,  80, 26);
    MakeButton(parent, L"Unsuspend",      IDC_BTN_UNSUSPEND_ACCT,  450, y,  85, 26);
    MakeButton(parent, L"Ban",            IDC_BTN_BAN_ACCT,        544, y,  50, 26);
    MakeButton(parent, L"Unban",          IDC_BTN_UNBAN_ACCT,      604, y,  55, 26);
    y += 34;

    MakeLabel(parent, L"New Password:", 20, y+4, 100, 20);
    g_hwndAdmPassword = MakeEdit(parent, IDC_ADM_PASSWORD, 128, y, 200, 24, ES_PASSWORD);
    MakeLabel(parent, L"(for Reset Password above)", 338, y+4, 200, 20);
    y += 44;

    // --- Server Overview ---
    MakeLabel(parent, L"Server Overview:", 20, y, 140, 20);
    y += 26;

    MakeButton(parent, L"Server Stats", IDC_BTN_SERVER_STATS, 20, y, 120, 26);
    MakeButton(parent, L"View Bans",    IDC_BTN_VIEW_BANS,   150, y, 100, 26);
    MakeLabel(parent, L"(accounts, characters, online, corpses; bans use Account field)", 260, y+4, 430, 20);
    y += 44;

    // --- Danger Zone: Delete Character ---
    MakeLabel(parent, L"Delete Character (permanent \x2014 two confirmations required):", 20, y, 430, 20);
    y += 26;

    MakeLabel(parent, L"Character:", 20, y+4, 70, 20);
    g_hwndAdmDelChar = MakeEdit(parent, IDC_ADM_DEL_CHAR, 96, y, 190, 24);
    MakeButton(parent, L"Delete Character...", IDC_BTN_DELETE_CHAR, 296, y, 150, 26);
    y += 36;

    // --- In-Game GM & GodMode Toggles ---
    MakeLabel(parent, L"In-Game Toggles (account must be GM status 255):", 20, y, 380, 20);
    y += 22;
    MakeLabel(parent, L"Character:", 20, y+4, 70, 20);
    g_hwndAdmGMChar = MakeEdit(parent, IDC_ADM_GM_CHAR, 96, y, 160, 24);
    MakeButton(parent, L"Toggle GM Flag",   IDC_BTN_TOGGLE_GM,      268, y, 120, 26);
    MakeButton(parent, L"Toggle God Mode",  IDC_BTN_TOGGLE_GODMODE, 398, y, 130, 26);
    MakeLabel(parent, L"(character must be offline)", 540, y+4, 200, 20);
    y += 34;

    MakeLabel(parent, L"Character Attributes:", 20, y, 130, 20);
    y += 22;
    MakeLabel(parent, L"Race:", 20, y+4, 36, 20);
    g_hwndProRaceCbo = MakeCombo(parent, IDC_PRO_RACE_COMBO, 60, y, 120, 400);
    for (int i = 0; i < RACE_TABLE_COUNT; ++i)
        SendMessageW(g_hwndProRaceCbo, CB_ADDSTRING, 0, (LPARAM)RACE_TABLE[i].name);
    MakeButton(parent, L"Set", IDC_BTN_PRO_SET_RACE, 186, y, 40, 26);
    MakeLabel(parent, L"Class:", 236, y+4, 40, 20);
    g_hwndProClassCbo = MakeCombo(parent, IDC_PRO_CLASS_COMBO, 280, y, 120, 400);
    for (int i = 0; i < 15; ++i)
        SendMessageW(g_hwndProClassCbo, CB_ADDSTRING, 0, (LPARAM)CLASS_NAMES[i]);
    MakeButton(parent, L"Set", IDC_BTN_PRO_SET_CLASS, 406, y, 40, 26);
    MakeLabel(parent, L"Gender:", 456, y+4, 48, 20);
    g_hwndProGenderCbo = MakeCombo(parent, IDC_PRO_GENDER_COMBO, 508, y, 80, 100);
    SendMessageW(g_hwndProGenderCbo, CB_ADDSTRING, 0, (LPARAM)L"Male");
    SendMessageW(g_hwndProGenderCbo, CB_ADDSTRING, 0, (LPARAM)L"Female");
    SendMessage(g_hwndProGenderCbo, CB_SETCURSEL, 0, 0);
    MakeButton(parent, L"Set", IDC_BTN_PRO_SET_GENDER, 594, y, 40, 26);
    MakeLabel(parent, L"(restart req'd)", 644, y+4, 100, 16);
    y += 34;

    g_hwndAdmResult = MakeResultBox(parent, IDC_ADM_RESULT, 20, y, 880, 200);
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

// Check if a character's account is currently active (logged in)
// Returns: 1 = online, 0 = offline, -1 = character not found or error
static int IsCharacterOnline(const wchar_t* charName) {
    std::wstring sql =
        L"SELECT a.active FROM account a "
        L"JOIN character_data cd ON cd.account_id=a.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(charName) + L"')";
    std::wstring result = RunQueryTable(sql);
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
    std::wstring result = RunQueryTable(sql);
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
        L"SELECT cd.name, "
        L"CASE "
        L"WHEN wc.id IS NOT NULL AND wc.last_seen = 0 THEN 'Online' "
        L"ELSE 'Offline' "
        L"END AS status, "
        L"cd.level, "
        L"CASE cd.class WHEN 1 THEN 'WAR' WHEN 2 THEN 'CLR' WHEN 3 THEN 'PAL' "
        L"WHEN 4 THEN 'RNG' WHEN 5 THEN 'SHD' WHEN 6 THEN 'DRU' "
        L"WHEN 7 THEN 'MNK' WHEN 8 THEN 'BRD' WHEN 9 THEN 'ROG' "
        L"WHEN 10 THEN 'SHM' WHEN 11 THEN 'NEC' WHEN 12 THEN 'WIZ' "
        L"WHEN 13 THEN 'MAG' WHEN 14 THEN 'ENC' WHEN 15 THEN 'BST' "
        L"ELSE CAST(cd.class AS CHAR) END as class, "
        L"z.short_name AS zone "
        L"FROM character_data cd "
        L"LEFT JOIN webdata_character wc ON wc.id = cd.id "
        L"LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id "
        L"WHERE cd.last_login > UNIX_TIMESTAMP(NOW() - INTERVAL 1 DAY) "
        L"ORDER BY "
        L"CASE WHEN wc.id IS NOT NULL AND wc.last_seen = 0 THEN 0 ELSE 1 END, "
        L"cd.last_login DESC";
    std::wstring result = RunQueryTable(sql);
    SetPlrResult(L"Characters active in last 24 hours:\r\n\r\n" + result);
}

static void DoRecentLogins() {
    if (!CheckServerRunning(L"Recent Logins")) return;
    std::wstring sql =
        L"SELECT a.name, lsa.LastLoginDate, lsa.LastIPAddress "
        L"FROM account a "
        L"LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name "
        L"ORDER BY lsa.LastLoginDate DESC LIMIT 20";
    std::wstring result = RunQueryTable(sql);
    SetPlrResult(L"Recent logins (last 20):\r\n\r\n" + result);
}

static void DoIPHistory() {
    wchar_t acct[128] = {};
    GetWindowTextW(g_hwndPlrAccount, acct, 128);
    if (!acct[0]) {
        MessageBoxW(g_hwndMain, L"Enter an account name in the Player Tools Account field first.",
            L"Account Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!CheckServerRunning(L"Last Known IP")) return;

    std::wstring sql =
        L"SELECT a.name, lsa.LastIPAddress, lsa.LastLoginDate "
        L"FROM account a "
        L"LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name "
        L"WHERE LOWER(a.name)=LOWER('" + std::wstring(acct) + L"')";
    std::wstring result = RunQueryTable(sql);
    SetPlrResult(std::wstring(L"Last known IP for: ") + acct +
        L"\r\n(only the most recent IP is stored per account)\r\n\r\n" + result);
}

static void DoServerStats() {
    if (!CheckServerRunning(L"Server Stats")) return;
    std::wstring sql =
        L"SELECT "
        L"(SELECT COUNT(*) FROM account) AS total_accounts, "
        L"(SELECT COUNT(*) FROM character_data) AS total_characters, "
        L"(SELECT COUNT(*) FROM character_data WHERE last_login > UNIX_TIMESTAMP(NOW() - INTERVAL 1 DAY)) AS active_24h, "
        L"(SELECT COUNT(*) FROM character_corpses WHERE is_rezzed=0 AND is_buried=0) AS live_corpses";
    std::wstring result = RunQueryTable(sql);
    SetAdmResult(L"Server Statistics:\r\n\r\n" + result +
        L"\r\n\r\nactive_24h = characters logged in within the last 24 hours\r\n"
        L"live_corpses = unrezzed, unburied corpses currently in the world");
}

static void DoSuspendAccount() {
    wchar_t acct[128] = {};
    if (!AdmGetAccount(acct, 128)) return;
    if (!CheckServerRunning(L"Suspend Account")) return;
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Suspend account '") + acct + L"'?\r\n\r\n"
         L"This sets the account status to -1, blocking all logins.\r\n"
         L"Use Unsuspend to restore access.\r\n\r\nContinue?").c_str(),
        L"Confirm Suspend", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE account SET status=-1 WHERE LOWER(name)=LOWER('" + std::wstring(acct) + L"')");
    SetAdmResult(std::wstring(L"Account '") + acct + L"' suspended (status=-1).\r\n\r\n"
        L"The player will be unable to log in.\r\n"
        L"Use Unsuspend to restore access.\r\n"
        L"If no rows changed, the account name may not exist.");
}

static void DoUnsuspendAccount() {
    wchar_t acct[128] = {};
    if (!AdmGetAccount(acct, 128)) return;
    if (!CheckServerRunning(L"Unsuspend Account")) return;
    RunQuery(L"UPDATE account SET status=0 WHERE LOWER(name)=LOWER('" + std::wstring(acct) +
             L"') AND status=-1");
    SetAdmResult(std::wstring(L"Account '") + acct + L"' unsuspended (status restored to 0).\r\n\r\n"
        L"The player can now log in again.\r\n"
        L"If no rows changed, the account was not suspended or the name is incorrect.");
}

static void DoDeleteCharacter() {
    if (!CheckServerRunning(L"Delete Character")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndAdmDelChar, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Delete Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring checkSql =
        L"SELECT cd.name, cd.level, a.name AS account "
        L"FROM character_data cd JOIN account a ON cd.account_id=a.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring info = RunQueryTable(checkSql);
    if (info == L"(no results)") {
        SetAdmResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }

    int online = IsCharacterOnline(chr);
    if (online == 1) {
        MessageBoxW(g_hwndMain,
            L"That character is currently online.\r\n\r\nLog them out before deleting.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }

    // First confirmation — show what will be destroyed
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"PERMANENTLY DELETE character '") + chr + L"'?\r\n\r\n" +
         info + L"\r\n\r\n"
         L"The following will be erased with no backup and no recovery:\r\n"
         L"  \x2022 All inventory and equipped items\r\n"
         L"  \x2022 All experience, levels, and AA points\r\n"
         L"  \x2022 All skills, spells, and faction values\r\n"
         L"  \x2022 All corpses and their loot\r\n"
         L"  \x2022 All buffs and pet data\r\n\r\n"
         L"THIS ACTION CANNOT BE REVERSED.\r\n\r\nContinue to second confirmation?").c_str(),
        L"Delete Character \x2014 Step 1 of 2", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2);
    if (r != IDYES) return;

    // Second confirmation — final permanent warning
    r = MessageBoxW(g_hwndMain,
        (std::wstring(L"PERMANENT DATA LOSS \x2014 FINAL CONFIRMATION\r\n\r\n") +
         L"You are about to permanently destroy character '" + chr + L"'.\r\n\r\n"
         L"Every item, every level, every AA, every skill \x2014\r\n"
         L"all of it will be gone forever.\r\n\r\n"
         L"There is no undo. There is no backup. There is no recovery.\r\n\r\n"
         L"Click YES only if you are absolutely certain.").c_str(),
        L"Delete Character \x2014 Step 2 of 2 (PERMANENT)", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2);
    if (r != IDYES) return;

    std::wstring idSql = L"SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
        std::wstring(chr) + L"')";
    std::wstring idResult = RunQuery(idSql);
    std::wstring charId;
    bool foundNl = false;
    for (auto c : idResult) {
        if (c == L'\n' || c == L'\r') { foundNl = true; continue; }
        if (foundNl && iswdigit(c)) charId += c;
        else if (foundNl && !charId.empty()) break;
    }
    if (charId.empty()) { SetAdmResult(L"Could not resolve character ID."); return; }

    const wchar_t* related[] = {
        L"character_inventory", L"character_currency", L"character_skills",
        L"character_spells", L"character_bind", L"character_corpses",
        L"character_buffs", L"character_faction_values", L"character_memmed_spells",
        L"character_pet_buffs", L"character_pet_inventory", L"character_alternate_abilities",
        nullptr
    };
    for (int i = 0; related[i]; ++i)
        RunQuery(std::wstring(L"DELETE FROM ") + related[i] + L" WHERE id=" + charId);
    RunQuery(L"DELETE FROM character_corpses WHERE charname='" + std::wstring(chr) + L"'");
    RunQuery(L"DELETE FROM character_data WHERE id=" + charId);

    SetAdmResult(std::wstring(L"Character '") + chr + L"' permanently deleted.\r\n\r\n"
        L"Inventory, currency, skills, spells, AAs, corpses, and all associated data removed.");
}

static void DoBanAccount() {
    wchar_t acct[128] = {};
    if (!AdmGetAccount(acct, 128)) return;
    if (!CheckServerRunning(L"Ban Account")) return;
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Ban account '") + acct + L"'?\r\n\r\n"
         L"Status will be set to -2, blocking all logins permanently.\r\n"
         L"Use Unban to restore access.\r\n\r\nContinue?").c_str(),
        L"Confirm Ban", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE account SET status=-2 WHERE LOWER(name)=LOWER('" + std::wstring(acct) + L"')");
    SetAdmResult(std::wstring(L"Account '") + acct + L"' banned (status=-2).\r\n\r\n"
        L"The player cannot log in. Use Unban to restore.\r\n"
        L"If no rows changed, the account name does not exist.");
}

static void DoUnbanAccount() {
    wchar_t acct[128] = {};
    if (!AdmGetAccount(acct, 128)) return;
    if (!CheckServerRunning(L"Unban Account")) return;
    RunQuery(L"UPDATE account SET status=0 WHERE LOWER(name)=LOWER('" + std::wstring(acct) +
             L"') AND status=-2");
    SetAdmResult(std::wstring(L"Account '") + acct + L"' unbanned (status restored to 0).\r\n\r\n"
        L"The player can log in again.\r\n"
        L"If no rows changed, the account was not banned or the name is incorrect.");
}

static void DoViewBans() {
    if (!CheckServerRunning(L"View Bans")) return;
    std::wstring sql =
        L"SELECT a.name AS account, a.status, lsa.LastLoginDate, lsa.LastIPAddress "
        L"FROM account a "
        L"LEFT JOIN tblLoginServerAccounts lsa ON lsa.AccountName=a.name "
        L"WHERE a.status=-2 ORDER BY a.name";
    std::wstring result = RunQueryTable(sql);
    SetAdmResult(L"Banned accounts (status=-2):\r\n\r\n" + result +
        L"\r\n\r\nTo unban, enter the account name above and click Unban.");
}

// ============================================================
// TAB 2 — PLAYER TOOLS PANEL
// ============================================================

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
    y += 40;

    MakeLabel(parent, L"Corpses:", 20, y, 80, 20);
    y += 26;
    MakeButton(parent, L"List All Corpses",     IDC_BTN_LIST_CORPSES,    20,  y, 140, 26);
    MakeButton(parent, L"Corpses by Character", IDC_BTN_CORPSES_BY_CHAR, 170, y, 160, 26);
    MakeLabel(parent, L"(uses character name above)", 340, y+5, 220, 20);
    y += 40;

    MakeLabel(parent, L"Search Characters:", 20, y, 140, 20);
    y += 26;
    MakeLabel(parent, L"Level:", 20, y+4, 42, 20);
    g_hwndPlrSearchMin = MakeEdit(parent, IDC_PLR_SEARCH_MIN, 66, y, 44, 24);
    MakeLabel(parent, L"to", 116, y+4, 20, 20);
    g_hwndPlrSearchMax = MakeEdit(parent, IDC_PLR_SEARCH_MAX, 140, y, 44, 24);
    MakeLabel(parent, L"Class:", 196, y+4, 42, 20);
    g_hwndPlrSearchClass = MakeCombo(parent, IDC_PLR_SEARCH_CLASS, 242, y, 140, 400);
    SendMessageW(g_hwndPlrSearchClass, CB_ADDSTRING, 0, (LPARAM)L"All Classes");
    for (int i = 0; i < 15; ++i)
        SendMessageW(g_hwndPlrSearchClass, CB_ADDSTRING, 0, (LPARAM)CLASS_NAMES[i]);
    SendMessage(g_hwndPlrSearchClass, CB_SETCURSEL, 0, 0);
    MakeButton(parent, L"Search", IDC_BTN_PLR_SEARCH, 392, y, 80, 26);
    MakeLabel(parent, L"(leave level blank for any)", 484, y+4, 200, 20);
    y += 40;

    MakeLabel(parent, L"Player Activity:", 20, y, 140, 20);
    y += 26;
    MakeButton(parent, L"Who Is Online",   IDC_BTN_WHO_ONLINE,    20,  y, 120, 26);
    MakeButton(parent, L"Recent Logins",   IDC_BTN_RECENT_LOGINS, 150, y, 120, 26);
    MakeButton(parent, L"Last Known IP",   IDC_BTN_IP_HISTORY,    280, y, 120, 26);
    MakeLabel(parent, L"(uses Account above)", 410, y+5, 160, 20);
    y += 34;

    // --- Faction Editor ---
    MakeLabel(parent, L"Factions:", 20, y, 60, 20);
    MakeLabel(parent, L"(uses Character above)", 86, y, 160, 20);
    MakeButton(parent, L"Load Factions", IDC_BTN_LOAD_FACTIONS, 260, y, 110, 24);
    MakeLabel(parent, L"Value:", 384, y+2, 40, 20);
    g_hwndFactionValue = MakeEdit(parent, IDC_FACTION_VALUE, 428, y, 56, 24);
    MakeButton(parent, L"Set", IDC_BTN_SET_FACTION, 494, y, 40, 24);
    MakeButton(parent, L"Ally", IDC_BTN_FACTION_ALLY, 544, y, 52, 24);
    MakeButton(parent, L"Warmly", IDC_BTN_FACTION_WARMLY, 602, y, 60, 24);
    MakeButton(parent, L"Indiff", IDC_BTN_FACTION_INDIFF, 668, y, 52, 24);
    MakeButton(parent, L"KOS", IDC_BTN_FACTION_KOS, 726, y, 42, 24);
    y += 26;
    g_hwndFactionList = MakeListBox(parent, IDC_FACTION_LIST, 20, y, 750, 56);
    y += 62;

    g_hwndPlrResult = MakeResultBox(parent, IDC_PLR_RESULT, 20, y, 940, 180);
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
    std::wstring result = RunQueryTable(sql);
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
    std::wstring result = RunQueryTable(sql);
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
    std::wstring result = RunQueryTable(sql);
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
    std::wstring result = RunQueryTable(sql);
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
    std::wstring result = RunQueryTable(sql);
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
    std::wstring result = RunQueryTable(sql);
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
        int r = MessageBoxW(g_hwndMain,
            (std::wstring(L"WARNING: '") + chr + L"' is currently ONLINE.\r\n\r\n"
             L"This feature is designed to be used when the character is OFFLINE.\r\n"
             L"Moving an online character may crash the server.\r\n\r\n"
             L"If you continue:\r\n"
             L"  - The zone will be updated in the database\r\n"
             L"  - *** SERVER RESTART REQUIRED ***\r\n"
             L"  - All connected players will be disconnected\r\n\r\n"
             L"Recommended: have the player camp to character select first,\r\n"
             L"then use Move to Zone while they are offline.\r\n\r\n"
             L"Continue anyway and restart the server?").c_str(),
            L"Character Is Online \x2014 Risk of Server Crash", MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2);
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
        L"SELECT cc.charname, z.short_name AS zone, "
        L"FROM_UNIXTIME(cc.time_of_death) AS died, cc.is_rezzed, cc.is_buried "
        L"FROM character_corpses cc "
        L"LEFT JOIN zone z ON z.zoneidnumber=cc.zone_id "
        L"ORDER BY cc.time_of_death DESC LIMIT 100";
    SetPlrResult(L"All corpses (newest first):\r\n\r\n" + RunQueryTable(sql));
}

static void DoCorpsesByChar() {
    wchar_t chr[128] = {};
    if (!PlrGetChar(chr, 128)) return;
    if (!CheckServerRunning(L"Corpses by Character")) return;
    std::wstring sql =
        L"SELECT cc.charname, z.short_name AS zone, "
        L"FROM_UNIXTIME(cc.time_of_death) AS died, "
        L"cc.is_rezzed, cc.is_buried, "
        L"cc.platinum, cc.gold, cc.silver, cc.copper "
        L"FROM character_corpses cc "
        L"LEFT JOIN zone z ON z.zoneidnumber=cc.zone_id "
        L"WHERE LOWER(cc.charname)=LOWER('" + std::wstring(chr) + L"') "
        L"ORDER BY cc.time_of_death DESC";
    SetPlrResult(std::wstring(L"Corpses for '") + chr + L"':\r\n\r\n" + RunQueryTable(sql));
}

static void DoSearchCharacters() {
    if (!CheckServerRunning(L"Search Characters")) return;
    wchar_t minBuf[16] = {}, maxBuf[16] = {};
    if (g_hwndPlrSearchMin) GetWindowTextW(g_hwndPlrSearchMin, minBuf, 16);
    if (g_hwndPlrSearchMax) GetWindowTextW(g_hwndPlrSearchMax, maxBuf, 16);
    int classSel = g_hwndPlrSearchClass ?
        (int)SendMessage(g_hwndPlrSearchClass, CB_GETCURSEL, 0, 0) : 0;

    std::wstring where = L"1=1";
    if (minBuf[0]) {
        for (wchar_t* p = minBuf; *p; p++) {
            if (!iswdigit(*p)) { SetPlrResult(L"Level min must be a number."); return; }
        }
        where += L" AND cd.level >= " + std::wstring(minBuf);
    }
    if (maxBuf[0]) {
        for (wchar_t* p = maxBuf; *p; p++) {
            if (!iswdigit(*p)) { SetPlrResult(L"Level max must be a number."); return; }
        }
        where += L" AND cd.level <= " + std::wstring(maxBuf);
    }
    if (classSel > 0) // 0 = "All Classes"
        where += L" AND cd.class = " + std::to_wstring(classSel);

    std::wstring sql =
        L"SELECT cd.name, cd.level, "
        L"CASE cd.class WHEN 1 THEN 'WAR' WHEN 2 THEN 'CLR' WHEN 3 THEN 'PAL' "
        L"WHEN 4 THEN 'RNG' WHEN 5 THEN 'SHD' WHEN 6 THEN 'DRU' "
        L"WHEN 7 THEN 'MNK' WHEN 8 THEN 'BRD' WHEN 9 THEN 'ROG' "
        L"WHEN 10 THEN 'SHM' WHEN 11 THEN 'NEC' WHEN 12 THEN 'WIZ' "
        L"WHEN 13 THEN 'MAG' WHEN 14 THEN 'ENC' WHEN 15 THEN 'BST' "
        L"ELSE CAST(cd.class AS CHAR) END AS class, "
        L"z.short_name AS zone, a.name AS account "
        L"FROM character_data cd "
        L"JOIN account a ON cd.account_id=a.id "
        L"LEFT JOIN zone z ON z.zoneidnumber=cd.zone_id "
        L"WHERE " + where + L" ORDER BY cd.level DESC LIMIT 100";
    SetPlrResult(L"Search results (max 100):\r\n\r\n" + RunQueryTable(sql));
}

// ============================================================
// TAB 5 — BACKUP & RESTORE PANEL
// ============================================================

static HWND g_hwndBackupList = nullptr;
static HWND g_hwndBackupInfo = nullptr;

static void CreateBackupPanel(HWND parent) {
    MakeLabel(parent, L"Backups:", 20, 16, 80, 20);
    g_hwndBackupList = MakeListBox(parent, IDC_BACKUP_LIST, 110, 16, 600, 130);

    // 6 harmonious buttons in 3x2 grid
    int bx = 20, by = 160, bw = 140, bh = 30, gap = 10;
    MakeButton(parent, L"Backup Now",        IDC_BTN_BACKUP_NOW,   bx,            by,      bw, bh);
    MakeButton(parent, L"Restore Selected",  IDC_BTN_RESTORE,      bx+bw+gap,     by,      bw, bh);
    MakeButton(parent, L"Database Size",     IDC_BTN_DB_SIZE,      bx+2*(bw+gap), by,      bw, bh);
    MakeButton(parent, L"Export Characters", IDC_BTN_EXPORT_CHARS, bx,            by+bh+6, bw, bh);
    MakeButton(parent, L"Import Characters", IDC_BTN_IMPORT_CHARS, bx+bw+gap,     by+bh+6, bw, bh);
    MakeButton(parent, L"Clone Character",   IDC_BTN_CLONE_CHAR,   bx+2*(bw+gap), by+bh+6, bw, bh);

    // Clone Character fields
    int cy = by + 2*(bh+6) + 10;
    MakeLabel(parent, L"Clone:", 20, cy+4, 42, 20);
    MakeLabel(parent, L"Source:", 66, cy+4, 48, 20);
    g_hwndCloneSource = MakeEdit(parent, IDC_CLONE_SOURCE, 118, cy, 160, 24);
    MakeLabel(parent, L"New Name:", 290, cy+4, 68, 20);
    g_hwndCloneNewName = MakeEdit(parent, IDC_CLONE_NEWNAME, 362, cy, 160, 24);
    cy += 34;

    g_hwndBackupInfo = MakeResultBox(parent, 0, 20, cy, 780, 180);

    MakeLabel(parent,
        L"Restore: select a backup from the list, then click Restore Selected.",
        20, cy+186, 560, 18);
    MakeLabel(parent,
        L"Clone: duplicates a character with all gear/AAs/spells under a new name.",
        20, cy+206, 560, 18);
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

static void CreateLogPanel(HWND parent) {
    int y = 14;

    // Row 1: log file selector + lines + load + refresh
    MakeLabel(parent, L"Log:", 20, y+4, 30, 20);
    g_hwndLogFileCombo = MakeCombo(parent, IDC_LOG_FILE_COMBO, 54, y, 180, 200);
    SendMessageW(g_hwndLogFileCombo, CB_ADDSTRING, 0, (LPARAM)L"Container (stdout)");
    SendMessageW(g_hwndLogFileCombo, CB_ADDSTRING, 0, (LPARAM)L"eqemu_debug.log");
    SendMessageW(g_hwndLogFileCombo, CB_ADDSTRING, 0, (LPARAM)L"world.log");
    SendMessageW(g_hwndLogFileCombo, CB_ADDSTRING, 0, (LPARAM)L"crash.log");
    SendMessage(g_hwndLogFileCombo, CB_SETCURSEL, 0, 0);

    MakeLabel(parent, L"Lines:", 244, y+4, 42, 20);
    g_hwndLogLines = MakeCombo(parent, IDC_LOG_LINES_COMBO, 290, y, 70, 120);
    SendMessageW(g_hwndLogLines, CB_ADDSTRING, 0, (LPARAM)L"50");
    SendMessageW(g_hwndLogLines, CB_ADDSTRING, 0, (LPARAM)L"100");
    SendMessageW(g_hwndLogLines, CB_ADDSTRING, 0, (LPARAM)L"500");
    SendMessageW(g_hwndLogLines, CB_ADDSTRING, 0, (LPARAM)L"All");
    SendMessage(g_hwndLogLines, CB_SETCURSEL, 1, 0);
    MakeButton(parent, L"Load",    IDC_BTN_LOAD_LOG,    370, y, 70, 26);
    MakeButton(parent, L"Refresh", IDC_BTN_REFRESH_LOG, 450, y, 70, 26);
    y += 36;

    // Row 2: filter + auto-refresh
    MakeLabel(parent, L"Filter:", 20, y+4, 42, 20);
    g_hwndLogFilter = MakeEdit(parent, IDC_LOG_FILTER_EDIT, 66, y, 250, 24);
    MakeButton(parent, L"Apply", IDC_BTN_APPLY_FILTER, 326, y, 70, 26);
    HWND chkAR = MakeCheck(parent, L"Auto-Refresh (30s)",
                            IDC_CHK_AUTO_REFRESH, 410, y+2, 160, 22);
    SendMessage(chkAR, BM_SETCHECK, BST_UNCHECKED, 0); // default off
    y += 36;

    g_hwndLogText = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        20, y, 880, 420,
        parent, (HMENU)(UINT_PTR)IDC_LOG_TEXT, g_hInst, nullptr);
    if (g_hwndLogText && g_hFontMono)
        SendMessage(g_hwndLogText, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
}

static void DoApplyLogFilter() {
    if (g_logFullText.empty()) return;
    wchar_t filterBuf[256] = {};
    if (g_hwndLogFilter) GetWindowTextW(g_hwndLogFilter, filterBuf, 256);
    if (!filterBuf[0]) {
        // No filter — show full text
        SetWindowTextW(g_hwndLogText, g_logFullText.c_str());
        return;
    }
    std::wstring filter = filterBuf;
    // Lower-case filter for case-insensitive match
    std::wstring filterLo = filter;
    for (auto& c : filterLo) c = towlower(c);

    std::wstring filtered;
    std::wistringstream ss(g_logFullText);
    std::wstring line;
    while (std::getline(ss, line)) {
        std::wstring lineLo = line;
        for (auto& c : lineLo) c = towlower(c);
        if (lineLo.find(filterLo) != std::wstring::npos)
            filtered += line + L"\r\n";
    }
    if (filtered.empty()) filtered = L"(no lines matching '" + filter + L"')";
    SetWindowTextW(g_hwndLogText, filtered.c_str());
}

static void DoLoadLog() {
    if (g_operationBusy) return;
    int fileSel = g_hwndLogFileCombo ?
        (int)SendMessage(g_hwndLogFileCombo, CB_GETCURSEL, 0, 0) : 0;
    int linesSel = (int)SendMessage(g_hwndLogLines, CB_GETCURSEL, 0, 0);
    std::wstring tailArg;
    switch (linesSel) {
        case 0: tailArg = L"50";  break;
        case 1: tailArg = L"100"; break;
        case 2: tailArg = L"500"; break;
        default: tailArg = L"";
    }
    SetBusy(true);
    SetStatus(L"Loading log...");
    SetWindowTextW(g_hwndLogText, L"Loading...");

    std::thread([fileSel, tailArg]{
        std::wstring cmd;
        if (fileSel == 0) {
            // Container stdout via docker logs
            cmd = L"docker logs ";
            if (!tailArg.empty()) cmd += L"--tail " + tailArg + L" ";
            cmd += CONTAINER;
        } else {
            // Log files inside the container — try common paths
            const wchar_t* logNames[] = {
                L"eqemu_debug.log",
                L"world.log",
                L"crash.log",
            };
            const wchar_t* logName = logNames[fileSel - 1];
            // Try multiple common log locations with sh -c
            cmd = std::wstring(L"docker exec ") + CONTAINER +
                L" sh -c \"for d in /quarm/logs /quarm/server/logs /home/eqemu/server/logs /opt/eqemu/logs; do "
                L"if [ -f \\\"$d/" + std::wstring(logName) + L"\\\" ]; then ";
            if (!tailArg.empty())
                cmd += L"tail -n " + tailArg + L" \\\"$d/" + std::wstring(logName) + L"\\\"; ";
            else
                cmd += L"cat \\\"$d/" + std::wstring(logName) + L"\\\"; ";
            cmd += L"exit 0; fi; done; echo '(log file not found - checked /quarm/logs, /quarm/server/logs, /home/eqemu/server/logs)'\"";
        }
        std::string out = RunCommand(cmd);
        std::wstring norm = NormalizeNewlines(ToWide(out));
        auto* res = new AsyncResult();
        res->success = true;
        if (norm.empty()) {
            const wchar_t* hints[] = {
                L"(Container stdout is empty. The server may not have produced output yet.)",
                L"(eqemu_debug.log does not exist. This file is only created when debug-level file logging is enabled in the server's log configuration. Use Container (stdout) for all server output.)",
                L"(world.log does not exist. The world server logs to Container (stdout) by default. File-based logging requires a log.ini configuration change inside the container.)",
                L"(crash.log does not exist. This is normal and expected \x2014 it means no zone or world process has crashed.)",
            };
            res->message = hints[fileSel < 4 ? fileSel : 0];
        } else {
            res->message = norm;
        }
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

    // Detect LAN vs WAN
    if (g_hwndNetModeLabel) {
        bool isLan = false;
        // Check private ranges: 10.x, 172.16-31.x, 192.168.x, 127.x
        if (addr.rfind(L"10.", 0) == 0 ||
            addr.rfind(L"192.168.", 0) == 0 ||
            addr.rfind(L"127.", 0) == 0) {
            isLan = true;
        } else if (addr.rfind(L"172.", 0) == 0 && addr.size() > 4) {
            int second = _wtoi(addr.c_str() + 4);
            if (second >= 16 && second <= 31) isLan = true;
        }
        if (isLan)
            SetWindowTextW(g_hwndNetModeLabel, L"Mode: LAN \x2014 private network (players on same network only)");
        else if (addr == L"127.0.0.1")
            SetWindowTextW(g_hwndNetModeLabel, L"Mode: Local only \x2014 only this computer can connect");
        else
            SetWindowTextW(g_hwndNetModeLabel, L"Mode: WAN \x2014 public IP (players need port forwarding: TCP/UDP 6000, TCP 9000)");
    }
}

static void CreateNetworkPanel(HWND parent) {
    MakeLabel(parent, L"Current Server Address:", 20, 20, 170, 20);
    g_hwndNetCurrent = MakeLabel(parent, L"", 200, 20, 200, 20);
    g_hwndNetModeLabel = MakeLabel(parent, L"", 20, 44, 660, 18);

    MakeButton(parent, L"Change Network Setting", IDC_BTN_CHANGE_NETWORK, 20, 68, 180, 30);
    MakeButton(parent, L"Write eqhost.txt...",    IDC_BTN_WRITE_EQHOST,  210, 68, 150, 30);
    MakeButton(parent, L"Copy IP",                IDC_BTN_COPY_IP,       370, 68,  80, 30);

    g_hwndNetAdapterList = MakeListBox(parent, IDC_NET_ADAPTER_LIST, 20, 116, 400, 100);
    ShowWindow(g_hwndNetAdapterList, SW_HIDE);
    MakeButton(parent, L"Confirm Selection", IDC_BTN_NET_CONFIRM, 430, 116, 140, 30);
    ShowWindow(GetDlgItem(parent, IDC_BTN_NET_CONFIRM), SW_HIDE);

    MakeLabel(parent, L"eqhost.txt content (copy to your TAKP client folder):", 20, 232, 400, 20);
    g_hwndEqhostContent = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        20, 255, 520, 140, parent,
        (HMENU)(UINT_PTR)IDC_NET_EQHOST_CONTENT, g_hInst, nullptr);
    g_hwndNetStatusMsg = MakeLabel(parent, L"", 20, 410, 800, 40);
}

static void DoCopyIP() {
    std::wstring addr = GetServerAddress();
    if (OpenClipboard(g_hwndMain)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (addr.size() + 1) * sizeof(wchar_t));
        if (hMem) {
            wchar_t* p = (wchar_t*)GlobalLock(hMem);
            wcscpy_s(p, addr.size() + 1, addr.c_str());
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
        if (g_hwndNetStatusMsg)
            SetWindowTextW(g_hwndNetStatusMsg,
                (std::wstring(L"Copied to clipboard: ") + addr).c_str());
    }
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

static bool GetAlwaysOnTop() {
    wchar_t cfgPath[MAX_PATH];
    wcscpy_s(cfgPath, g_installDir);
    PathAppendW(cfgPath, L".qsm_settings");
    std::ifstream f(cfgPath);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line == "always_on_top=1") return true;
    }
    return false;
}

static bool GetDarkMode() {
    wchar_t cfgPath[MAX_PATH];
    wcscpy_s(cfgPath, g_installDir);
    PathAppendW(cfgPath, L".qsm_settings");
    std::ifstream f(cfgPath);
    if (!f) return true;  // default: dark mode ON
    std::string line;
    while (std::getline(f, line)) {
        if (line == "dark_mode=0") return false;
        if (line == "dark_mode=1") return true;
    }
    return true;  // default: dark mode ON
}

static void ApplyAlwaysOnTop(bool onTop) {
    if (!g_hwndMain) return;
    SetWindowPos(g_hwndMain,
        onTop ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

static void ApplyDarkMode(bool dark) {
    g_darkMode = dark;
    if (g_hbrDark)       { DeleteObject(g_hbrDark);       g_hbrDark       = nullptr; }
    if (g_hbrDarkCtl)    { DeleteObject(g_hbrDarkCtl);    g_hbrDarkCtl    = nullptr; }
    if (g_hbrDarkAccent) { DeleteObject(g_hbrDarkAccent); g_hbrDarkAccent = nullptr; }
    if (dark) {
        g_hbrDark       = CreateSolidBrush(CLR_DARK_BG);
        g_hbrDarkCtl    = CreateSolidBrush(CLR_DARK_CTL);
        g_hbrDarkAccent = CreateSolidBrush(CLR_DARK_ACCENT);
    }

    // Dark title bar via DWM (Windows 10 1809+ / Windows 11)
    if (g_hwndMain) {
        BOOL useDark = dark ? TRUE : FALSE;
        DwmSetWindowAttribute(g_hwndMain, 20, &useDark, sizeof(useDark));
        DwmSetWindowAttribute(g_hwndMain, 19, &useDark, sizeof(useDark));
    }

    // Tabs stay normal in both modes - no dark theming on tab control

    // Toggle owner-draw on buttons + theme on controls
    if (g_hwndMain) {
        for (int i = 0; i < NUM_TABS; ++i) {
            if (g_hwndPanels[i]) {
                EnumChildWindows(g_hwndPanels[i], [](HWND child, LPARAM dark) -> BOOL {
                    wchar_t cls[64] = {};
                    GetClassNameW(child, cls, 64);
                    if (_wcsicmp(cls, L"Button") == 0) {
                        LONG st = GetWindowLong(child, GWL_STYLE);
                        LONG btnType = st & BS_TYPEMASK;
                        if (dark) {
                            // Only convert push buttons to owner-draw, not checkboxes
                            if (btnType == BS_PUSHBUTTON || btnType == BS_DEFPUSHBUTTON) {
                                st = (st & ~BS_TYPEMASK) | BS_OWNERDRAW;
                                SetWindowLong(child, GWL_STYLE, st);
                            }
                            // Don't apply DarkMode_Explorer to checkboxes —
                            // let WM_CTLCOLORSTATIC handle their text color
                            if (btnType != BS_AUTOCHECKBOX && btnType != BS_CHECKBOX &&
                                btnType != BS_3STATE && btnType != BS_AUTO3STATE &&
                                btnType != BS_RADIOBUTTON && btnType != BS_AUTORADIOBUTTON) {
                                SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
                            } else {
                                // Clear any theme on checkboxes so WM_CTLCOLORSTATIC works
                                SetWindowTheme(child, L"", L"");
                            }
                        } else {
                            // Restore owner-draw buttons back to normal push buttons
                            if (btnType == BS_OWNERDRAW) {
                                st = (st & ~BS_TYPEMASK) | BS_PUSHBUTTON;
                                SetWindowLong(child, GWL_STYLE, st);
                            }
                            SetWindowTheme(child, L"", L"");
                        }
                    } else {
                        // Non-button controls: apply/clear explorer theme
                        if (dark)
                            SetWindowTheme(child, L"DarkMode_Explorer", nullptr);
                        else
                            SetWindowTheme(child, L"", L"");
                    }
                    RedrawWindow(child, nullptr, nullptr,
                        RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
                    return TRUE;
                }, (LPARAM)dark);
                InvalidateRect(g_hwndPanels[i], nullptr, TRUE);
            }
        }
        if (g_hwndStatus) {
            if (dark) SetWindowTheme(g_hwndStatus, L"DarkMode_Explorer", nullptr);
            else      SetWindowTheme(g_hwndStatus, L"", L"");
            InvalidateRect(g_hwndStatus, nullptr, TRUE);
        }
        RedrawWindow(g_hwndMain, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
    }
}

static void SaveSettings(bool noBackup, int retention, bool alwaysOnTop, bool darkMode) {
    wchar_t cfgPath[MAX_PATH];
    wcscpy_s(cfgPath, g_installDir);
    PathAppendW(cfgPath, L".qsm_settings");
    std::ofstream f(cfgPath);
    if (f) {
        f << "no_backup_on_stop=" << (noBackup ? 1 : 0) << "\n";
        f << "backup_retention=" << retention << "\n";
        f << "always_on_top=" << (alwaysOnTop ? 1 : 0) << "\n";
        f << "dark_mode=" << (darkMode ? 1 : 0) << "\n";
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

// ============================================================
// TAB 5 — SERVER PANEL (Rule Editor + XP Slider + Guild Mgr)
// ============================================================


// ============================================================
// TAB 5 — ZONES PANEL
// ============================================================

static void CreateZonePanel(HWND parent) {
    int y = 8;

    // --- ZEM (Zone Experience Modifier) ---
    MakeLabel(parent, L"Zone Exp Modifier (ZEM):", 20, y+4, 166, 20);
    g_hwndZemLabel = MakeLabel(parent, L"(select a zone below, then click Refresh)", 190, y+4, 340, 20);
    MakeLabel(parent, L"ZEM:", 560, y+4, 32, 20);
    g_hwndZemValue = MakeEdit(parent, IDC_ZEM_VALUE, 596, y, 60, 24);
    MakeButton(parent, L"Save ZEM", IDC_BTN_ZEM_SAVE, 664, y, 76, 26);
    MakeButton(parent, L"Default", IDC_BTN_ZEM_DEFAULT, 746, y, 62, 26);
    y += 30;

    // Running Zones
    MakeLabel(parent, L"Running Zones:", 20, y, 110, 18);
    y += 18;
    g_hwndZoneList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP | LBS_USETABSTOPS,
        20, y, 760, 140, parent, (HMENU)(UINT_PTR)IDC_ZONE_LIST, g_hInst, nullptr);
    // Set tab stops: Long Name | Short Name | Spawns | ZEM
    // Units are 1/4 average char width of the listbox font
    int tabs[] = { 130, 200, 230 };
    SendMessage(g_hwndZoneList, LB_SETTABSTOPS, 3, (LPARAM)tabs);
    if (g_hwndZoneList && g_hFontMono)
        SendMessage(g_hwndZoneList, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
    y += 144;

    // Zone Controls
    MakeButton(parent, L"Refresh Zones",   IDC_BTN_REFRESH_ZONES, 20,  y, 100, 26);
    MakeButton(parent, L"Stop Zone",       IDC_BTN_STOP_ZONE,     128, y, 80,  26);
    MakeButton(parent, L"Restart Zone",    IDC_BTN_RESTART_ZONE,  216, y, 92,  26);
    MakeButton(parent, L"Repop All Zones", IDC_BTN_REPOP_ZONES,   316, y, 120, 26);
    y += 30;

    // Dynamic Zones + Start Zone + Find Zone
    MakeLabel(parent, L"Dynamic Zones:", 20, y+4, 96, 20);
    g_hwndGameZoneCur = MakeLabel(parent, L"(?)", 120, y+4, 30, 20);
    g_hwndGameZoneCbo = MakeCombo(parent, IDC_GAME_ZONE_COMBO, 154, y, 56, 200);
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"5");
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"10");
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"15");
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"20");
    SendMessageW(g_hwndGameZoneCbo, CB_ADDSTRING, 0, (LPARAM)L"25");
    MakeButton(parent, L"Set", IDC_BTN_SET_ZONE_COUNT, 216, y, 40, 26);
    MakeLabel(parent, L"(restart)", 262, y+4, 56, 20);
    MakeLabel(parent, L"Start Zone:", 340, y+4, 74, 20);
    g_hwndZoneStartEdit = MakeEdit(parent, IDC_ZONE_START_EDIT, 418, y, 120, 24);
    MakeButton(parent, L"Start", IDC_BTN_START_ZONE, 544, y, 52, 26);
    MakeLabel(parent, L"Find Zone:", 614, y+4, 68, 20);
    g_hwndZoneFindEdit = MakeEdit(parent, IDC_ZONE_FIND_EDIT, 686, y, 120, 24);
    MakeButton(parent, L"Find Zone", IDC_BTN_FIND_ZONE_STATUS, 812, y, 80, 26);
    y += 34;

    // Zone Environment
    MakeLabel(parent, L"Zone Environment:", 20, y, 130, 20);
    y += 22;
    MakeLabel(parent, L"Zone:", 20, y+4, 36, 20);
    g_hwndEnvZoneEdit = MakeEdit(parent, IDC_ENV_ZONE_EDIT, 60, y, 130, 24);
    MakeButton(parent, L"Load Zone", IDC_BTN_ENV_LOAD, 196, y, 86, 26);
    MakeLabel(parent, L"Weather Type:", 296, y+4, 88, 20);
    g_hwndEnvWeatherCbo = MakeCombo(parent, IDC_ENV_WEATHER_COMBO, 388, y, 100, 120);
    SendMessageW(g_hwndEnvWeatherCbo, CB_ADDSTRING, 0, (LPARAM)L"0 - Off");
    SendMessageW(g_hwndEnvWeatherCbo, CB_ADDSTRING, 0, (LPARAM)L"1 - Rain");
    SendMessageW(g_hwndEnvWeatherCbo, CB_ADDSTRING, 0, (LPARAM)L"2 - Snow");
    MakeLabel(parent, L"Find:", 510, y+4, 34, 20);
    g_hwndEnvFindEdit = MakeEdit(parent, IDC_ENV_FIND_EDIT, 548, y, 120, 24);
    MakeButton(parent, L"Find Zone", IDC_BTN_ENV_FIND_ZONE, 674, y, 76, 26);
    y += 28;
    MakeLabel(parent, L"Fog Start:", 20, y+4, 62, 20);
    g_hwndEnvFogMin = MakeEdit(parent, IDC_ENV_FOG_MIN, 86, y, 46, 24);
    MakeLabel(parent, L"Fog End:", 142, y+4, 54, 20);
    g_hwndEnvFogMax = MakeEdit(parent, IDC_ENV_FOG_MAX, 200, y, 46, 24);
    MakeLabel(parent, L"Fog Density:", 258, y+4, 76, 20);
    g_hwndEnvFogDensity = MakeEdit(parent, IDC_ENV_FOG_DENSITY, 338, y, 40, 24);
    MakeLabel(parent, L"Fog Color R:", 394, y+4, 76, 20);
    g_hwndEnvFogR = MakeEdit(parent, IDC_ENV_FOG_R, 474, y, 32, 24);
    MakeLabel(parent, L"G:", 512, y+4, 16, 20);
    g_hwndEnvFogG = MakeEdit(parent, IDC_ENV_FOG_G, 532, y, 32, 24);
    MakeLabel(parent, L"B:", 570, y+4, 16, 20);
    g_hwndEnvFogB = MakeEdit(parent, IDC_ENV_FOG_B, 590, y, 32, 24);
    y += 28;
    MakeLabel(parent, L"Clip Near:", 20, y+4, 60, 20);
    g_hwndEnvClipMin = MakeEdit(parent, IDC_ENV_CLIP_MIN, 84, y, 50, 24);
    MakeLabel(parent, L"Clip Far:", 146, y+4, 54, 20);
    g_hwndEnvClipMax = MakeEdit(parent, IDC_ENV_CLIP_MAX, 204, y, 50, 24);
    MakeButton(parent, L"Save Zone Settings", IDC_BTN_ENV_SAVE, 274, y, 130, 26);
    MakeButton(parent, L"Reset to Zone Default", IDC_BTN_ENV_DEFAULT, 414, y, 150, 26);
    y += 34;

    g_hwndZoneResult = MakeResultBox(parent, IDC_STATUS_RESULT, 20, y, 940, 140);
}

static void CreateServerPanel(HWND parent) {
    int y = 10;

    MakeLabel(parent, L"Server Settings:", 20, y, 110, 20);
    y += 22;
    MakeLabel(parent, L"Era:", 20, y+4, 26, 20);
    g_hwndGameEraCur = MakeLabel(parent, L"(?)", 50, y+4, 60, 20);
    g_hwndGameEraCbo = MakeCombo(parent, IDC_GAME_ERA_COMBO, 114, y, 150, 200);
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Classic");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Kunark");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Velious");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Luclin");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"Planes of Power");
    SendMessageW(g_hwndGameEraCbo, CB_ADDSTRING, 0, (LPARAM)L"All Expansions");
    MakeButton(parent, L"Set Era", IDC_BTN_SET_ERA, 272, y, 72, 26);
    MakeLabel(parent, L"(restart required)", 352, y+4, 130, 20);
    y += 30;
    MakeLabel(parent, L"Server Name:", 20, y+4, 88, 20);
    g_hwndServerNameEdit = MakeEdit(parent, IDC_STATUS_SERVERNAME_EDIT, 114, y, 694, 24);
    MakeButton(parent, L"Set Name", IDC_BTN_SET_SERVERNAME, 816, y, 76, 26);
    y += 28;
    MakeLabel(parent, L"Login Marquee:", 20, y+4, 100, 20);
    g_hwndMarqueeEdit = MakeEdit(parent, IDC_STATUS_MARQUEE_EDIT, 124, y, 684, 24);
    MakeButton(parent, L"Set Marquee", IDC_BTN_SET_MARQUEE, 816, y, 76, 26);
    y += 28;
    MakeLabel(parent, L"MOTD:", 20, y+4, 42, 20);
    g_hwndMotdEdit = MakeEdit(parent, IDC_STATUS_MOTD_EDIT, 68, y, 740, 24);
    MakeButton(parent, L"Set MOTD", IDC_BTN_SET_MOTD, 816, y, 76, 26);
    y += 28;
    MakeLabel(parent, L"Announce:", 20, y+4, 68, 20);
    g_hwndAnnounceEdit = MakeEdit(parent, IDC_ANNOUNCE_EDIT, 94, y, 714, 24);
    MakeButton(parent, L"Send", IDC_BTN_SEND_ANNOUNCE, 816, y, 76, 26);
    y += 38;

    // --- XP Multiplier Slider (1.00x - 10.00x in 0.25 steps) ---
    MakeLabel(parent, L"XP Rate:", 20, y+4, 56, 20);
    g_hwndXpSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
        80, y, 400, 28, parent, (HMENU)(UINT_PTR)IDC_XP_SLIDER, g_hInst, nullptr);
    SendMessage(g_hwndXpSlider, TBM_SETRANGE, TRUE, MAKELPARAM(4, 40));
    SendMessage(g_hwndXpSlider, TBM_SETPOS, TRUE, 4);
    SendMessage(g_hwndXpSlider, TBM_SETTICFREQ, 4, 0);
    g_hwndXpLabel = MakeLabel(parent, L"1.00x", 488, y+4, 50, 20);
    MakeButton(parent, L"Apply", IDC_BTN_APPLY_XP, 546, y, 54, 26);
    MakeLabel(parent, L"(takes effect on next zone-in)", 614, y+4, 220, 20);
    y += 32;

    // --- AA XP Multiplier Slider (below XP) ---
    MakeLabel(parent, L"AA Rate:", 20, y+4, 56, 20);
    g_hwndAaSlider = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | WS_TABSTOP,
        80, y, 400, 28, parent, (HMENU)(UINT_PTR)IDC_AA_SLIDER, g_hInst, nullptr);
    SendMessage(g_hwndAaSlider, TBM_SETRANGE, TRUE, MAKELPARAM(4, 40));
    SendMessage(g_hwndAaSlider, TBM_SETPOS, TRUE, 4);
    SendMessage(g_hwndAaSlider, TBM_SETTICFREQ, 4, 0);
    g_hwndAaLabel = MakeLabel(parent, L"1.00x", 488, y+4, 50, 20);
    MakeButton(parent, L"Apply", IDC_BTN_APPLY_AA, 546, y, 54, 26);
    y += 34;

    // --- Rule Editor ---
    MakeLabel(parent, L"Server Rules:", 20, y, 100, 20);
    y += 22;
    MakeLabel(parent, L"Search:", 20, y+4, 48, 20);
    g_hwndRuleSearch = MakeEdit(parent, IDC_RULE_SEARCH, 72, y, 240, 24);
    MakeButton(parent, L"Load Rules", IDC_BTN_LOAD_RULES, 322, y, 100, 26);
    y += 30;
    g_hwndRuleList = MakeListBox(parent, IDC_RULE_LIST, 20, y, 660, 120);
    y += 126;
    MakeLabel(parent, L"Selected:", 20, y+4, 58, 20);
    g_hwndRuleSelected = MakeLabel(parent, L"(none)", 82, y+4, 400, 20);
    y += 22;
    MakeLabel(parent, L"New Value:", 20, y+4, 70, 20);
    g_hwndRuleValue = MakeEdit(parent, IDC_RULE_VALUE, 96, y, 200, 24);
    MakeButton(parent, L"Save Rule", IDC_BTN_SAVE_RULE, 306, y, 90, 26);
    MakeButton(parent, L"Reset to Original", IDC_BTN_RESET_RULE, 406, y, 140, 26);
    y += 44;

    // --- Guild Manager ---
    MakeLabel(parent, L"Guild Manager:", 20, y, 110, 20);
    MakeButton(parent, L"List Guilds", IDC_BTN_LIST_GUILDS, 136, y, 100, 24);
    y += 28;
    MakeLabel(parent, L"Guild Name:", 20, y+4, 80, 20);
    g_hwndGuildName = MakeEdit(parent, IDC_GUILD_NAME, 106, y, 160, 24);
    MakeLabel(parent, L"Leader:", 278, y+4, 48, 20);
    g_hwndGuildLeader = MakeEdit(parent, IDC_GUILD_LEADER, 330, y, 140, 24);
    y += 28;
    MakeButton(parent, L"Create Guild",  IDC_BTN_CREATE_GUILD,     20,  y, 110, 26);
    MakeButton(parent, L"Set Leader",    IDC_BTN_SET_GUILD_LEADER, 140, y, 100, 26);
    MakeButton(parent, L"Disband",       IDC_BTN_DISBAND_GUILD,    250, y, 80,  26);
    MakeButton(parent, L"View Roster",   IDC_BTN_VIEW_ROSTER,      340, y, 110, 26);
    MakeLabel(parent, L"(select guild from result first)", 460, y+4, 220, 20);
    y += 32;

    g_hwndServerResult = MakeResultBox(parent, IDC_SERVER_RESULT, 20, y, 940, 130);
}

// ============================================================
// NEW OPERATIONS — Status Tab
// ============================================================

static void DoRepopZones() {
    if (!CheckServerRunning(L"Repop Zones")) return;
    int r = MessageBoxW(g_hwndMain,
        L"Force all NPCs to respawn across all zones?\n\n"
        L"This clears all respawn timers and restarts zone processes.\n"
        L"All current NPCs will despawn and respawn fresh.",
        L"Confirm Repop", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;
    SetBusy(true);
    SetStatus(L"Repopulating zones...");
    std::thread([]{
        RunQuery(L"DELETE FROM respawn_times");
        RunCommand(std::wstring(L"docker exec ") + CONTAINER + L" killall zone");
        Sleep(3000);
        auto* res = new AsyncResult{ true, L"Zone repop complete. All NPCs will respawn." };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_STATUS, (LPARAM)res);
    }).detach();
}

static void DoSendAnnouncement() {
    if (!CheckServerRunning(L"Send Announcement")) return;
    wchar_t msg[512] = {};
    if (g_hwndAnnounceEdit) GetWindowTextW(g_hwndAnnounceEdit, msg, 512);
    if (!msg[0]) {
        MessageBoxW(g_hwndMain, L"Enter an announcement message first.",
            L"Announcement", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(msg)) {
        if (c == L'\'') safe += L"''";
        else safe += c;
    }
    RunQuery(L"UPDATE rule_values SET rule_value='[ANNOUNCE] " + safe +
             L"' WHERE rule_name='World:MOTD'");
    SetStatus(L"Announcement set \x2014 players will see it on next zone-in.");
}

// ============================================================
// NEW OPERATIONS — Status Tab (Zone Management)
// ============================================================

static void DoRefreshZones() {
    if (!IsContainerRunning()) {
        if (g_hwndZoneList) {
            SendMessage(g_hwndZoneList, LB_RESETCONTENT, 0, 0);
            SendMessageW(g_hwndZoneList, LB_ADDSTRING, 0, (LPARAM)L"(server is not running)");
        }
        return;
    }

    std::map<std::wstring, int> zoneCounts = GetRunningZoneProcessCounts();
    int totalProcs = 0;
    for (const auto& entry : zoneCounts)
        totalProcs += entry.second;

    // Get configured dynamic count
    std::string dynCountOut = TrimRight(RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"SELECT dynamics FROM launcher LIMIT 1\" quarm"));
    int dynCount = 0;
    for (char c : dynCountOut) { if (isdigit(c)) dynCount = dynCount * 10 + (c - '0'); }

    std::map<std::wstring, std::pair<std::wstring, std::wstring>> zoneMeta =
        GetZoneMetadata(zoneCounts);
    std::vector<std::wstring> items;
    int runningZones = 0;
    for (const auto& entry : zoneCounts) {
        const std::wstring& shortName = entry.first;
        std::wstring longName = shortName;
        std::wstring zem = L"0";
        auto metaIt = zoneMeta.find(shortName);
        if (metaIt != zoneMeta.end()) {
            if (!metaIt->second.first.empty()) longName = metaIt->second.first;
            if (!metaIt->second.second.empty()) zem = metaIt->second.second;
        }

        std::wstring display = longName + L"\t" + shortName + L"\t" +
                               std::to_wstring(entry.second);
        if (zem != L"0" && zem != L"0.00")
            display += L"\t" + zem;
        else
            display += L"\t";

        items.push_back(display);
        runningZones++;
    }

    // Only redraw if content changed
    bool changed = false;
    int oldCount = (int)SendMessage(g_hwndZoneList, LB_GETCOUNT, 0, 0);
    if (oldCount != (int)items.size()) {
        changed = true;
    } else {
        for (int i = 0; i < oldCount && !changed; ++i) {
            wchar_t existing[512] = {};
            SendMessageW(g_hwndZoneList, LB_GETTEXT, i, (LPARAM)existing);
            if (items[i] != existing) changed = true;
        }
    }
    if (changed) {
        SendMessage(g_hwndZoneList, LB_RESETCONTENT, 0, 0);
        for (auto& item : items)
            SendMessageW(g_hwndZoneList, LB_ADDSTRING, 0, (LPARAM)item.c_str());
        if (items.empty())
            SendMessageW(g_hwndZoneList, LB_ADDSTRING, 0, (LPARAM)L"(no running zone processes found)");
    }

    wchar_t buf[256];
    swprintf_s(buf, L"%d zone processes across %d running zones, %d dynamic pool configured",
        totalProcs, runningZones, dynCount);
    if (g_hwndZoneResult) SetWindowTextW(g_hwndZoneResult, buf);
    if (g_hwndStatusResult) SetWindowTextW(g_hwndStatusResult, buf);
}

static void DoStopZone() {
    if (!CheckServerRunning(L"Stop Zone")) return;
    std::wstring zoneName = ExtractZoneFromList();
    if (zoneName.empty()) {
        MessageBoxW(g_hwndMain, L"Select a zone from the Running Zones list.",
            L"Stop Zone", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (L"Stop all processes for zone '" + zoneName + L"'?").c_str(),
        L"Confirm Stop Zone", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;

    auto zonePids = GetLiveZonePidsByName();
    auto it = zonePids.find(zoneName);
    if (it == zonePids.end() || it->second.empty()) {
        if (g_hwndZoneResult)
            SetWindowTextW(g_hwndZoneResult,
                (L"Could not find a live PID for zone '" + zoneName + L"'.").c_str());
        DoRefreshZones();
        return;
    }

    std::wstring pidList;
    for (int pid : it->second) {
        if (!pidList.empty())
            pidList += L" ";
        pidList += std::to_wstring(pid);
    }

    RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" sh -c \"kill " + pidList + L"\"");
    bool stopped = false;
    for (int i = 0; i < 8; ++i) {
        auto zoneCounts = GetRunningZoneProcessCounts();
        if (zoneCounts.find(zoneName) == zoneCounts.end()) {
            stopped = true;
            break;
        }
        Sleep(250);
    }
    if (g_hwndZoneResult) {
        if (stopped)
            SetWindowTextW(g_hwndZoneResult, (L"Stopped zone '" + zoneName + L"' and refreshed live state.").c_str());
        else
            SetWindowTextW(g_hwndZoneResult, (L"Stop requested for '" + zoneName + L"', but a zone process is still present.").c_str());
    }
    DoRefreshZones();
}

static void DoRestartZone() {
    if (!CheckServerRunning(L"Restart Zone")) return;
    std::wstring zoneName = ExtractZoneFromList();
    if (zoneName.empty()) {
        MessageBoxW(g_hwndMain, L"Select a zone from the Running Zones list.",
            L"Restart Zone", MB_OK | MB_ICONINFORMATION);
        return;
    }
    auto zonePids = GetLiveZonePidsByName();
    auto it = zonePids.find(zoneName);
    if (it == zonePids.end() || it->second.empty()) {
        if (g_hwndZoneResult)
            SetWindowTextW(g_hwndZoneResult,
                (L"Could not find a live PID for zone '" + zoneName + L"'.").c_str());
        DoRefreshZones();
        return;
    }

    std::wstring pidList;
    for (int pid : it->second) {
        if (!pidList.empty())
            pidList += L" ";
        pidList += std::to_wstring(pid);
    }

    RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" sh -c \"kill " + pidList + L"\"");

    for (int i = 0; i < 12; ++i) {
        auto zoneCounts = GetRunningZoneProcessCounts();
        if (zoneCounts.find(zoneName) == zoneCounts.end())
            break;
        Sleep(250);
    }

    RunCommand(std::wstring(L"docker exec -d ") + CONTAINER +
        L" /quarm/bin/zone " + zoneName);
    if (g_hwndZoneResult) SetWindowTextW(g_hwndZoneResult,
        (L"Restart requested for zone '" + zoneName + L"' using its live PID.").c_str());
    Sleep(2500);
    DoRefreshZones();
}

static void DoStartZone() {
    if (!CheckServerRunning(L"Start Zone")) return;
    wchar_t zone[128] = {};
    GetWindowTextW(g_hwndZoneStartEdit, zone, 128);
    if (!zone[0]) {
        MessageBoxW(g_hwndMain, L"Enter a zone short name (e.g. 'commons', 'gfaydark').",
            L"Start Zone", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Use eqlaunch-compatible method: start a zone process directly
    std::wstring cmd = std::wstring(L"docker exec -d ") + CONTAINER +
        L" /quarm/bin/zone " + std::wstring(zone);
    RunCommand(cmd);
    SetStatus((L"Starting zone: " + std::wstring(zone)).c_str());
    Sleep(2000);
    DoRefreshZones();
}

static void DoFindZoneStatus() {
    if (!CheckServerRunning(L"Find Zone")) return;
    wchar_t term[128] = {};
    if (g_hwndZoneFindEdit) GetWindowTextW(g_hwndZoneFindEdit, term, 128);
    if (!term[0]) {
        MessageBoxW(g_hwndMain, L"Enter a zone short name or partial name to search.",
            L"Find Zone", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(term)) {
        if (c == L'\'') safe += L"''";
        else safe += c;
    }
    std::wstring statusCase = L"CASE";
    for (const auto& entry : GetRunningZoneProcessCounts())
        statusCase += L" WHEN z.short_name='" + entry.first + L"' THEN 'RUNNING'";
    statusCase += L" ELSE 'stopped' END";

    std::wstring sql =
        std::wstring(L"SELECT z.short_name, z.long_name, z.zoneidnumber, ")
        + L"z.zone_exp_multiplier AS zem, "
        + statusCase + L" AS status "
        + L"FROM zone z "
        + L"WHERE z.short_name LIKE '%" + safe + L"%' OR z.long_name LIKE '%" + safe +
        L"%' ORDER BY z.short_name LIMIT 30";
    std::wstring result = RunQueryTable(sql);
    if (g_hwndZoneResult)
        SetWindowTextW(g_hwndZoneResult, (L"Zone Search:\r\n" + result).c_str());
}

// ============================================================
// NEW OPERATIONS — Status Tab (Boss Management)
// ============================================================

static void DoCheckBoss() {
    if (!CheckServerRunning(L"Check Boss")) return;
    HWND cbo = GetDlgItem(g_hwndPanels[TAB_STATUS], IDC_SPAWN_BOSS_COMBO);
    int sel = cbo ? (int)SendMessage(cbo, CB_GETCURSEL, 0, 0) : -1;
    if (sel == CB_ERR || sel < 0 || sel >= BOSS_TABLE_COUNT) {
        MessageBoxW(g_hwndMain, L"Select a boss from the dropdown.",
            L"Check Boss", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int npcId = BOSS_TABLE[sel].npcId;
    const wchar_t* bossName = BOSS_TABLE[sel].name;
    std::wstring sql =
        L"SELECT se.spawngroupID, s2.zone, s2.enabled "
        L"FROM spawnentry se "
        L"JOIN spawn2 s2 ON s2.spawngroupID=se.spawngroupID "
        L"WHERE se.npcID=" + std::to_wstring(npcId) + L" LIMIT 5";
    std::wstring result = RunQueryTable(sql);
    std::wstring msg = std::wstring(bossName) + L":\r\n" +
        (result.find(L"(no results)") != std::wstring::npos
            ? L"No spawn entry found in database."
            : result);
    if (g_hwndStatusResult)
        SetWindowTextW(g_hwndStatusResult, msg.c_str());
}

static void DoDespawnBoss() {
    if (!CheckServerRunning(L"Despawn Boss")) return;
    HWND cbo = GetDlgItem(g_hwndPanels[TAB_STATUS], IDC_SPAWN_BOSS_COMBO);
    int sel = cbo ? (int)SendMessage(cbo, CB_GETCURSEL, 0, 0) : -1;
    if (sel == CB_ERR || sel < 0 || sel >= BOSS_TABLE_COUNT) {
        MessageBoxW(g_hwndMain, L"Select a boss from the dropdown.",
            L"Despawn Boss", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int npcId = BOSS_TABLE[sel].npcId;
    const wchar_t* bossName = BOSS_TABLE[sel].name;
    const wchar_t* zone = BOSS_TABLE[sel].zone;
    int r = MessageBoxW(g_hwndMain,
        (L"Despawn " + std::wstring(bossName) + L"?\n\n"
         L"This adds a respawn timer entry to prevent the boss from respawning\n"
         L"and restarts the zone to remove the current NPC.").c_str(),
        L"Confirm Despawn", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;
    // Add respawn timer to suppress the spawn, then restart zone
    RunQuery(L"INSERT IGNORE INTO respawn_times (id, start, duration) "
             L"SELECT s2.id, UNIX_TIMESTAMP(), 999999 FROM spawn2 s2 "
             L"JOIN spawnentry se ON se.spawngroupID=s2.spawngroupID "
             L"WHERE se.npcID=" + std::to_wstring(npcId));
    // Kill zones for that zone to force reload
    RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" sh -c \"ps -eo pid,args | awk '$2 ~ /(^|\\\\/)zone$/ && $NF==\\\"" + std::wstring(zone) +
        L"\\\" {print $1}' | xargs -r kill\"");
    if (g_hwndStatusResult)
        SetWindowTextW(g_hwndStatusResult,
            (std::wstring(bossName) + L" despawned. Zone '" + std::wstring(zone) + L"' reloading.").c_str());
}

static void DoListActiveBosses() {
    if (!CheckServerRunning(L"List Bosses")) return;
    // Check which bosses from our table have active spawn entries without respawn timers
    std::wstring npcIds;
    for (int i = 0; i < BOSS_TABLE_COUNT; ++i) {
        if (i > 0) npcIds += L",";
        npcIds += std::to_wstring(BOSS_TABLE[i].npcId);
    }
    std::wstring sql =
        L"SELECT nt.name, s2.zone, "
        L"CASE WHEN rt.id IS NULL THEN 'SPAWNABLE' ELSE 'ON TIMER' END AS status "
        L"FROM spawnentry se "
        L"JOIN npc_types nt ON nt.id=se.npcID "
        L"JOIN spawn2 s2 ON s2.spawngroupID=se.spawngroupID "
        L"LEFT JOIN respawn_times rt ON rt.id=s2.id "
        L"WHERE se.npcID IN (" + npcIds + L") "
        L"GROUP BY nt.name, s2.zone ORDER BY nt.name";
    std::wstring result = RunQueryTable(sql);
    if (g_hwndStatusResult)
        SetWindowTextW(g_hwndStatusResult, (L"Boss Status:\r\n" + result).c_str());
}

// ============================================================
// NEW OPERATIONS — Server Tab (Rules, XP, Guilds)
// ============================================================

static void DoLoadRules() {
    if (!CheckServerRunning(L"Load Rules")) return;
    std::wstring sql = L"SELECT rule_name, rule_value FROM rule_values ORDER BY rule_name";
    std::string out = RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + sql + L"\" quarm");
    g_rules.clear();
    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        RuleInfo ri;
        ri.name = ToWide(line.substr(0, tab));
        ri.value = ToWide(line.substr(tab + 1));
        ri.origValue = ri.value;
        g_rules.push_back(ri);
    }
    // Apply search filter
    wchar_t filterBuf[256] = {};
    if (g_hwndRuleSearch) GetWindowTextW(g_hwndRuleSearch, filterBuf, 256);
    std::wstring filter = filterBuf;
    for (auto& c : filter) c = towlower(c);
    g_rulesFiltered.clear();
    SendMessage(g_hwndRuleList, LB_RESETCONTENT, 0, 0);
    for (auto& r : g_rules) {
        if (!filter.empty()) {
            std::wstring nameLo = r.name;
            for (auto& c : nameLo) c = towlower(c);
            if (nameLo.find(filter) == std::wstring::npos) continue;
        }
        g_rulesFiltered.push_back(r);
        std::wstring display = r.name + L" = " + r.value;
        SendMessageW(g_hwndRuleList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
    }
    wchar_t countBuf[64];
    swprintf_s(countBuf, L"Loaded %d rules (%d shown)",
        (int)g_rules.size(), (int)g_rulesFiltered.size());
    if (g_hwndServerResult) SetWindowTextW(g_hwndServerResult, countBuf);
    // Set XP and AA sliders from rules
    for (auto& r : g_rules) {
        if (r.name.find(L"Character:ExpMultiplier") != std::wstring::npos ||
            r.name.find(L"Zone:ExpMultiplier") != std::wstring::npos) {
            try {
                double val = std::stod(std::string(r.value.begin(), r.value.end()));
                int tick = (int)(val * 4.0 + 0.5);
                if (tick < 4) tick = 4; if (tick > 40) tick = 40;
                SendMessage(g_hwndXpSlider, TBM_SETPOS, TRUE, tick);
                wchar_t buf[32]; swprintf_s(buf, L"%.2fx", tick / 4.0);
                SetWindowTextW(g_hwndXpLabel, buf);
            } catch (...) {}
        }
        if (r.name.find(L"AAExpMultiplier") != std::wstring::npos) {
            try {
                double val = std::stod(std::string(r.value.begin(), r.value.end()));
                int tick = (int)(val * 4.0 + 0.5);
                if (tick < 4) tick = 4; if (tick > 40) tick = 40;
                SendMessage(g_hwndAaSlider, TBM_SETPOS, TRUE, tick);
                wchar_t buf[32]; swprintf_s(buf, L"%.2fx", tick / 4.0);
                SetWindowTextW(g_hwndAaLabel, buf);
            } catch (...) {}
        }
    }
}

static void DoSaveRule() {
    if (!CheckServerRunning(L"Save Rule")) return;
    int sel = (int)SendMessage(g_hwndRuleList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR || sel >= (int)g_rulesFiltered.size()) {
        MessageBoxW(g_hwndMain, L"Select a rule from the list first.",
            L"Save Rule", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t newVal[512] = {};
    GetWindowTextW(g_hwndRuleValue, newVal, 512);
    std::wstring safe;
    for (wchar_t c : std::wstring(newVal)) {
        if (c == L'\'') safe += L"''";
        else safe += c;
    }
    std::wstring ruleName = g_rulesFiltered[sel].name;
    RunQuery(L"UPDATE rule_values SET rule_value='" + safe +
             L"' WHERE rule_name='" + ruleName + L"'");
    g_rulesFiltered[sel].value = newVal;
    // Update in master list too
    for (auto& r : g_rules) {
        if (r.name == ruleName) { r.value = newVal; break; }
    }
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"Rule '" + ruleName + L"' set to: " + std::wstring(newVal)).c_str());
}

static void DoResetRule() {
    if (!CheckServerRunning(L"Reset Rule")) return;
    int sel = (int)SendMessage(g_hwndRuleList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR || sel >= (int)g_rulesFiltered.size()) {
        MessageBoxW(g_hwndMain, L"Select a rule from the list first.",
            L"Reset Rule", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring ruleName = g_rulesFiltered[sel].name;
    std::wstring origVal  = g_rulesFiltered[sel].origValue;
    RunQuery(L"UPDATE rule_values SET rule_value='" + origVal +
             L"' WHERE rule_name='" + ruleName + L"'");
    g_rulesFiltered[sel].value = origVal;
    for (auto& r : g_rules) {
        if (r.name == ruleName) { r.value = origVal; break; }
    }
    SetWindowTextW(g_hwndRuleValue, origVal.c_str());
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"Rule '" + ruleName + L"' reset to original: " + origVal).c_str());
}

static void DoApplyXP() {
    if (!CheckServerRunning(L"Apply XP Rate")) return;
    int pos = (int)SendMessage(g_hwndXpSlider, TBM_GETPOS, 0, 0);
    wchar_t valBuf[16];
    swprintf_s(valBuf, L"%.2f", pos / 4.0);
    std::wstring val = valBuf;
    RunQuery(L"UPDATE rule_values SET rule_value='" + val +
             L"' WHERE rule_name='Character:ExpMultiplier'");
    RunQuery(L"UPDATE rule_values SET rule_value='" + val +
             L"' WHERE rule_name='Zone:ExpMultiplier'");
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"XP multiplier set to " + val + L"x. Takes effect on next zone-in.").c_str());
}

static void DoApplyAA() {
    if (!CheckServerRunning(L"Apply AA Rate")) return;
    int pos = (int)SendMessage(g_hwndAaSlider, TBM_GETPOS, 0, 0);
    wchar_t valBuf[16];
    swprintf_s(valBuf, L"%.2f", pos / 4.0);
    std::wstring val = valBuf;
    RunQuery(L"UPDATE rule_values SET rule_value='" + val +
             L"' WHERE rule_name='Zone:AAExpMultiplier'");
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"AA multiplier set to " + val + L"x. Takes effect on next zone-in.").c_str());
}

static void DoListGuilds() {
    if (!CheckServerRunning(L"List Guilds")) return;
    std::wstring sql =
        L"SELECT g.id, g.name, IFNULL(cd.name,'(none)') AS leader "
        L"FROM guilds g "
        L"LEFT JOIN character_data cd ON cd.id=g.leader "
        L"ORDER BY g.name";
    std::wstring result = RunQueryTable(sql);
    if (g_hwndServerResult) SetWindowTextW(g_hwndServerResult, result.c_str());
}

static void DoCreateGuild() {
    if (!CheckServerRunning(L"Create Guild")) return;
    wchar_t name[128] = {}, leader[128] = {};
    GetWindowTextW(g_hwndGuildName, name, 128);
    GetWindowTextW(g_hwndGuildLeader, leader, 128);
    if (!name[0]) {
        MessageBoxW(g_hwndMain, L"Enter a guild name.", L"Create Guild", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!leader[0]) {
        MessageBoxW(g_hwndMain, L"Enter a leader character name.", L"Create Guild", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring sql =
        L"INSERT INTO guilds (name, leader) VALUES ('" + std::wstring(name) +
        L"', (SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
        std::wstring(leader) + L"')))";
    RunQuery(sql);
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"Guild '" + std::wstring(name) + L"' created with leader " +
             std::wstring(leader) + L".").c_str());
}

static void DoDisbandGuild() {
    if (!CheckServerRunning(L"Disband Guild")) return;
    wchar_t name[128] = {};
    GetWindowTextW(g_hwndGuildName, name, 128);
    if (!name[0]) {
        MessageBoxW(g_hwndMain, L"Enter the guild name to disband.", L"Disband", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (L"Permanently disband guild '" + std::wstring(name) + L"'?\n\n"
         L"All members will be removed.").c_str(),
        L"Confirm Disband", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"DELETE FROM guild_members WHERE guild_id=(SELECT id FROM guilds WHERE name='" +
             std::wstring(name) + L"')");
    RunQuery(L"DELETE FROM guilds WHERE name='" + std::wstring(name) + L"'");
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"Guild '" + std::wstring(name) + L"' disbanded.").c_str());
}

static void DoViewRoster() {
    if (!CheckServerRunning(L"View Roster")) return;
    wchar_t name[128] = {};
    GetWindowTextW(g_hwndGuildName, name, 128);
    if (!name[0]) {
        MessageBoxW(g_hwndMain, L"Enter a guild name.", L"View Roster", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring sql =
        L"SELECT cd.name, gm.rank FROM guild_members gm "
        L"JOIN character_data cd ON cd.id=gm.char_id "
        L"WHERE gm.guild_id=(SELECT id FROM guilds WHERE name='" +
        std::wstring(name) + L"') ORDER BY gm.rank, cd.name";
    std::wstring result = RunQueryTable(sql);
    if (g_hwndServerResult) SetWindowTextW(g_hwndServerResult, result.c_str());
}

static void DoSetGuildLeader() {
    if (!CheckServerRunning(L"Set Guild Leader")) return;
    wchar_t name[128] = {}, leader[128] = {};
    GetWindowTextW(g_hwndGuildName, name, 128);
    GetWindowTextW(g_hwndGuildLeader, leader, 128);
    if (!name[0] || !leader[0]) {
        MessageBoxW(g_hwndMain, L"Enter both guild name and new leader name.",
            L"Set Leader", MB_OK | MB_ICONINFORMATION);
        return;
    }
    RunQuery(L"UPDATE guilds SET leader=(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
             std::wstring(leader) + L"')) WHERE name='" + std::wstring(name) + L"'");
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"Guild leader set to " + std::wstring(leader) + L".").c_str());
}

// ============================================================
// NEW OPERATIONS — Pro Tools (Spawn, Spells, Factions, Skills)
// ============================================================

static void DoSpawnBoss() {
    if (!CheckServerRunning(L"Spawn Boss")) return;
    HWND cbo = GetDlgItem(g_hwndPanels[TAB_STATUS], IDC_SPAWN_BOSS_COMBO);
    int sel = cbo ? (int)SendMessage(cbo, CB_GETCURSEL, 0, 0) : -1;
    if (sel == CB_ERR || sel < 0 || sel >= BOSS_TABLE_COUNT) {
        MessageBoxW(g_hwndMain, L"Select a boss from the dropdown.",
            L"Spawn Boss", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int npcId = BOSS_TABLE[sel].npcId;
    const wchar_t* bossName = BOSS_TABLE[sel].name;
    const wchar_t* zone = BOSS_TABLE[sel].zone;
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Spawn ") + bossName + L" in " + zone + L"?\n\n"
         L"A temporary spawn entry will be created and the zone reloaded.").c_str(),
        L"Confirm Spawn", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;
    SetBusy(true);
    SetStatus(L"Spawning boss...");
    int capturedNpcId = npcId;
    std::wstring capturedZone = zone;
    std::wstring capturedName = bossName;
    std::thread([capturedNpcId, capturedZone, capturedName]{
        // Insert temp spawngroup + spawnentry + spawn2
        RunQuery(L"INSERT IGNORE INTO spawngroup (id, name) VALUES (999990, 'QSM_TEMP_BOSS')");
        RunQuery(L"DELETE FROM spawnentry WHERE spawngroupID=999990");
        RunQuery(L"INSERT INTO spawnentry (spawngroupID, npcID, chance) VALUES (999990, " +
                 std::to_wstring(capturedNpcId) + L", 100)");
        RunQuery(L"DELETE FROM spawn2 WHERE spawngroupID=999990");
        RunQuery(L"INSERT INTO spawn2 (spawngroupID, zone, x, y, z, respawntime, variance) VALUES "
                 L"(999990, '" + capturedZone + L"', 0, 0, 5, 1200, 0)");
        // Restart zone processes to pick up the new spawn
        RunCommand(std::wstring(L"docker exec ") + CONTAINER + L" killall zone");
        Sleep(3000);
        auto* res = new AsyncResult{ true,
            capturedName + L" spawned in " + capturedZone + L".\r\n"
            L"The spawn entry (group 999990) will persist until cleaned up." };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_GAME, (LPARAM)res);
    }).detach();
}

static void DoSpellSearch() {
    if (!CheckServerRunning(L"Spell Search")) return;
    wchar_t term[256] = {};
    if (g_hwndSpellSearch) GetWindowTextW(g_hwndSpellSearch, term, 256);
    if (!term[0]) {
        MessageBoxW(g_hwndMain, L"Enter a spell name to search.",
            L"Spell Search", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(term)) {
        if (c == L'\'') safe += L"''";
        else safe += c;
    }
    std::wstring sql =
        L"SELECT id, name FROM spells_new WHERE name LIKE '%" + safe +
        L"%' ORDER BY name LIMIT 100";
    std::string out = RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + sql + L"\" quarm");
    SendMessage(g_hwndSpellList, LB_RESETCONTENT, 0, 0);
    int count = 0;
    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;
        std::wstring wide = ToWide(line);
        // Replace tab with ": "
        auto tab = wide.find(L'\t');
        if (tab != std::wstring::npos) wide.replace(tab, 1, L": ");
        SendMessageW(g_hwndSpellList, LB_ADDSTRING, 0, (LPARAM)wide.c_str());
        count++;
    }
    wchar_t buf[64]; swprintf_s(buf, L"Found %d spells.", count);
    SetGameResult(buf);
}

static void DoScribeSpell() {
    if (!CheckServerRunning(L"Scribe Spell")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Scribe Spell", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int sel = (int)SendMessage(g_hwndSpellList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        MessageBoxW(g_hwndMain, L"Select a spell from the search results.",
            L"Scribe Spell", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t item[512] = {};
    SendMessageW(g_hwndSpellList, LB_GETTEXT, sel, (LPARAM)item);
    // Extract spell ID (everything before ":")
    std::wstring spellStr = item;
    auto colon = spellStr.find(L':');
    if (colon == std::wstring::npos) return;
    std::wstring spellId = spellStr.substr(0, colon);
    // Trim whitespace
    while (!spellId.empty() && spellId.back() == L' ') spellId.pop_back();

    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain, L"Character must be offline to scribe spells.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    // Find next slot
    std::wstring slotSql =
        L"SELECT IFNULL(MAX(slot_id)+1, 0) FROM character_spells "
        L"WHERE id=(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
        std::wstring(chr) + L"'))";
    std::string slotOut = TrimRight(RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + slotSql + L"\" quarm"));
    std::wstring slot = ToWide(slotOut);
    // Trim whitespace
    while (!slot.empty() && (slot.back() == L'\n' || slot.back() == L'\r' || slot.back() == L' '))
        slot.pop_back();
    if (slot.empty()) slot = L"0";

    RunQuery(L"INSERT INTO character_spells (id, slot_id, spell_id) VALUES ("
             L"(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
             std::wstring(chr) + L"')), " + slot + L", " + spellId + L")");
    SetGameResult((L"Scribed spell " + spellStr + L" to " + std::wstring(chr) +
                   L" at slot " + slot + L".").c_str());
}

static void DoScribeAll() {
    if (!CheckServerRunning(L"Scribe All")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name.", L"Scribe All", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int count = (int)SendMessage(g_hwndSpellList, LB_GETCOUNT, 0, 0);
    if (count <= 0) {
        MessageBoxW(g_hwndMain, L"No spells in search results.", L"Scribe All", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain, L"Character must be offline.", L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (L"Scribe all " + std::to_wstring(count) + L" spells to " + std::wstring(chr) + L"?").c_str(),
        L"Confirm Scribe All", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;
    int scribed = 0;
    for (int i = 0; i < count; ++i) {
        wchar_t item[512] = {};
        SendMessageW(g_hwndSpellList, LB_GETTEXT, i, (LPARAM)item);
        std::wstring spellStr = item;
        auto colon = spellStr.find(L':');
        if (colon == std::wstring::npos) continue;
        std::wstring spellId = spellStr.substr(0, colon);
        while (!spellId.empty() && spellId.back() == L' ') spellId.pop_back();
        RunQuery(L"INSERT IGNORE INTO character_spells (id, slot_id, spell_id) VALUES ("
                 L"(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
                 std::wstring(chr) + L"')), " + std::to_wstring(200 + i) + L", " + spellId + L")");
        scribed++;
    }
    wchar_t buf[128]; swprintf_s(buf, L"Scribed %d spells to %s.", scribed, chr);
    SetGameResult(buf);
}

static void DoLoadFactions() {
    if (!CheckServerRunning(L"Load Factions")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndPlrCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name.", L"Load Factions", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring sql =
        L"SELECT cf.faction_id, fl.name, cf.current_value "
        L"FROM character_faction_values cf "
        L"LEFT JOIN faction_list fl ON fl.id=cf.faction_id "
        L"WHERE cf.id=(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
        std::wstring(chr) + L"')) ORDER BY fl.name LIMIT 200";
    std::string out = RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + sql + L"\" quarm");
    SendMessage(g_hwndFactionList, LB_RESETCONTENT, 0, 0);
    int count = 0;
    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;
        std::wstring wide = ToWide(line);
        // Replace tabs with " | "
        std::wstring display;
        for (auto c : wide) {
            if (c == L'\t') display += L" | ";
            else display += c;
        }
        SendMessageW(g_hwndFactionList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        count++;
    }
    wchar_t buf[64]; swprintf_s(buf, L"Loaded %d faction entries for %s.", count, chr);
    SetPlrResult(buf);
}

static void DoSetFaction(int presetValue = -99999) {
    if (!CheckServerRunning(L"Set Faction")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndPlrCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name.", L"Set Faction", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int sel = (int)SendMessage(g_hwndFactionList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        MessageBoxW(g_hwndMain, L"Select a faction from the list.",
            L"Set Faction", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t item[512] = {};
    SendMessageW(g_hwndFactionList, LB_GETTEXT, sel, (LPARAM)item);
    // Extract faction_id (first number before " | ")
    std::wstring str = item;
    auto pipe = str.find(L" | ");
    if (pipe == std::wstring::npos) return;
    std::wstring factionId = str.substr(0, pipe);

    int newValue = presetValue;
    if (presetValue == -99999) {
        wchar_t valBuf[64] = {};
        GetWindowTextW(g_hwndFactionValue, valBuf, 64);
        if (!valBuf[0]) {
            MessageBoxW(g_hwndMain, L"Enter a faction value or use a preset button.",
                L"Faction", MB_OK | MB_ICONINFORMATION);
            return;
        }
        newValue = _wtoi(valBuf);
    }
    RunQuery(L"UPDATE character_faction_values SET current_value=" +
             std::to_wstring(newValue) +
             L" WHERE id=(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
             std::wstring(chr) + L"')) AND faction_id=" + factionId);
    SetPlrResult((L"Faction " + factionId + L" set to " +
                   std::to_wstring(newValue) + L" for " + std::wstring(chr) + L".").c_str());
}

static void DoMaxSkills() {
    if (!CheckServerRunning(L"Max Skills")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Max Skills", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain, L"Character must be offline.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (L"Max all skills for '" + std::wstring(chr) +
         L"'?\n\nThis sets all combat, casting, trade, and language skills to maximum.").c_str(),
        L"Confirm Max Skills", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;
    std::wstring charIdSql = L"(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
        std::wstring(chr) + L"'))";
    RunQuery(L"UPDATE character_skills SET value=252 WHERE id=" + charIdSql);
    RunQuery(L"UPDATE character_languages SET value=100 WHERE id=" + charIdSql);
    SetGameResult((L"All skills and languages maxed for '" + std::wstring(chr) +
                   L"'. Log in to see changes.").c_str());
}

// ============================================================
// NEW OPERATIONS — Player Tools (Loot Viewer)
// ============================================================

static void DoLootByNPC() {
    if (!CheckServerRunning(L"Loot Lookup")) return;
    wchar_t term[256] = {};
    if (g_hwndLootSearch) GetWindowTextW(g_hwndLootSearch, term, 256);
    if (!term[0]) {
        MessageBoxW(g_hwndMain, L"Enter an NPC name to search.",
            L"Loot Lookup", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(term)) {
        if (c == L'\'') safe += L"''";
        else if (c == L'`') continue;
        else safe += c;
    }
    std::wstring safeUnderscore = safe;
    for (auto& c : safeUnderscore) if (c == L' ') c = L'_';
    std::wstring sql =
        L"SELECT nt.name AS npc, it.Name AS item, ROUND(lde.chance,1) AS pct "
        L"FROM npc_types nt "
        L"JOIN loottable_entries lte ON lte.loottable_id=nt.loottable_id "
        L"JOIN lootdrop_entries lde ON lde.lootdrop_id=lte.lootdrop_id "
        L"JOIN items it ON it.id=lde.item_id "
        L"WHERE REPLACE(LOWER(nt.name),'_',' ') LIKE LOWER('%" + safe + L"%') "
        L"OR LOWER(nt.name) LIKE LOWER('%" + safeUnderscore + L"%') "
        L"ORDER BY nt.name, lde.chance DESC LIMIT 200";
    SetGameResult(L"Loot by NPC results:\r\n\r\n" + RunQueryTable(sql));
}

static void DoLootByItem() {
    if (!CheckServerRunning(L"Loot Lookup")) return;
    wchar_t term[256] = {};
    if (g_hwndLootSearch) GetWindowTextW(g_hwndLootSearch, term, 256);
    if (!term[0]) {
        MessageBoxW(g_hwndMain, L"Enter an item name to search.",
            L"Loot Lookup", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(term)) {
        if (c == L'\'') safe += L"''";
        else if (c == L'`') continue;
        else safe += c;
    }
    std::wstring safeUnderscore = safe;
    for (auto& c : safeUnderscore) if (c == L' ') c = L'_';
    std::wstring sql =
        L"SELECT it.Name AS item, nt.name AS dropped_by, ROUND(lde.chance,1) AS pct "
        L"FROM items it "
        L"JOIN lootdrop_entries lde ON lde.item_id=it.id "
        L"JOIN loottable_entries lte ON lte.lootdrop_id=lde.lootdrop_id "
        L"JOIN npc_types nt ON nt.loottable_id=lte.loottable_id "
        L"WHERE REPLACE(LOWER(it.Name),'_',' ') LIKE LOWER('%" + safe + L"%') "
        L"OR LOWER(it.Name) LIKE LOWER('%" + safeUnderscore + L"%') "
        L"ORDER BY it.Name, lde.chance DESC LIMIT 200";
    SetGameResult(L"Loot by Item results:\r\n\r\n" + RunQueryTable(sql));
}

// ============================================================
// NEW OPERATIONS — Pro Tools (Skills)
// ============================================================

static void DoSkillSearch() {
    if (!CheckServerRunning(L"Skill Search")) return;
    wchar_t term[256] = {};
    if (g_hwndSkillSearch) GetWindowTextW(g_hwndSkillSearch, term, 256);
    if (!term[0]) {
        MessageBoxW(g_hwndMain, L"Enter a skill name to search (e.g. 'kick', '1h', 'swim').",
            L"Skill Search", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(term)) {
        if (c == L'\'') safe += L"''";
        else safe += c;
    }
    // skill_caps has skill_id; we need the name from a known table or just show IDs
    // EQEmu uses numeric skill IDs; search skill_caps for matching descriptions
    std::wstring sql =
        L"SELECT DISTINCT sc.skill_id, "
        L"CASE sc.skill_id "
        L"WHEN 0 THEN '1H Blunt' WHEN 1 THEN '1H Slashing' WHEN 2 THEN '2H Blunt' "
        L"WHEN 3 THEN '2H Slashing' WHEN 4 THEN 'Abjuration' WHEN 5 THEN 'Alteration' "
        L"WHEN 6 THEN 'Apply Poison' WHEN 7 THEN 'Archery' WHEN 8 THEN 'Backstab' "
        L"WHEN 9 THEN 'Bind Wound' WHEN 10 THEN 'Bash' WHEN 11 THEN 'Block' "
        L"WHEN 12 THEN 'Brass Instruments' WHEN 13 THEN 'Channeling' WHEN 14 THEN 'Conjuration' "
        L"WHEN 15 THEN 'Defense' WHEN 16 THEN 'Disarm' WHEN 17 THEN 'Disarm Traps' "
        L"WHEN 18 THEN 'Divination' WHEN 19 THEN 'Dodge' WHEN 20 THEN 'Double Attack' "
        L"WHEN 21 THEN 'Dragon Punch' WHEN 22 THEN 'Dual Wield' WHEN 23 THEN 'Eagle Strike' "
        L"WHEN 24 THEN 'Evocation' WHEN 25 THEN 'Feign Death' WHEN 26 THEN 'Flying Kick' "
        L"WHEN 27 THEN 'Forage' WHEN 28 THEN 'Hand to Hand' WHEN 29 THEN 'Hide' "
        L"WHEN 30 THEN 'Kick' WHEN 31 THEN 'Meditate' WHEN 32 THEN 'Mend' "
        L"WHEN 33 THEN 'Offense' WHEN 34 THEN 'Parry' WHEN 35 THEN 'Pick Lock' "
        L"WHEN 36 THEN 'Piercing' WHEN 37 THEN 'Riposte' WHEN 38 THEN 'Round Kick' "
        L"WHEN 39 THEN 'Safe Fall' WHEN 40 THEN 'Sense Heading' WHEN 41 THEN 'Singing' "
        L"WHEN 42 THEN 'Sneak' WHEN 43 THEN 'Specialize Abjure' WHEN 44 THEN 'Specialize Alteration' "
        L"WHEN 45 THEN 'Specialize Conjuration' WHEN 46 THEN 'Specialize Divination' "
        L"WHEN 47 THEN 'Specialize Evocation' WHEN 48 THEN 'Swimming' WHEN 49 THEN 'Throwing' "
        L"WHEN 50 THEN 'Tiger Claw' WHEN 51 THEN 'Tracking' WHEN 52 THEN 'Wind Instruments' "
        L"WHEN 53 THEN 'Fishing' WHEN 54 THEN 'Make Poison' WHEN 55 THEN 'Tinkering' "
        L"WHEN 56 THEN 'Research' WHEN 57 THEN 'Alchemy' WHEN 58 THEN 'Baking' "
        L"WHEN 59 THEN 'Tailoring' WHEN 60 THEN 'Sense Traps' WHEN 61 THEN 'Blacksmithing' "
        L"WHEN 62 THEN 'Fletching' WHEN 63 THEN 'Brewing' WHEN 64 THEN 'Alcohol Tolerance' "
        L"WHEN 65 THEN 'Begging' WHEN 66 THEN 'Jewelry Making' WHEN 67 THEN 'Pottery' "
        L"WHEN 68 THEN 'Percussion Instruments' WHEN 69 THEN 'Intimidation' "
        L"WHEN 70 THEN 'Berserking' WHEN 71 THEN 'Taunt' WHEN 72 THEN 'Frenzy' "
        L"WHEN 73 THEN 'Remove Traps' WHEN 74 THEN 'Triple Attack' "
        L"ELSE CONCAT('Skill ', sc.skill_id) END AS skill_name "
        L"FROM skill_caps sc "
        L"HAVING skill_name LIKE '%" + safe + L"%' "
        L"ORDER BY skill_name LIMIT 100";
    std::string out = RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + sql + L"\" quarm");
    SendMessage(g_hwndSkillList, LB_RESETCONTENT, 0, 0);
    int count = 0;
    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;
        std::wstring wide = ToWide(line);
        auto tab = wide.find(L'\t');
        if (tab != std::wstring::npos) wide.replace(tab, 1, L": ");
        SendMessageW(g_hwndSkillList, LB_ADDSTRING, 0, (LPARAM)wide.c_str());
        count++;
    }
    wchar_t buf[64]; swprintf_s(buf, L"Found %d skills.", count);
    SetGameResult(buf);
}

static void DoLoadSkills() {
    if (!CheckServerRunning(L"Load Skills")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name.",
            L"Load Skills", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring sql =
        L"SELECT cs.skill_id, cs.value FROM character_skills cs "
        L"WHERE cs.id=(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
        std::wstring(chr) + L"')) AND cs.value > 0 ORDER BY cs.skill_id";
    std::string out = RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + sql + L"\" quarm");
    SendMessage(g_hwndSkillList, LB_RESETCONTENT, 0, 0);
    int count = 0;
    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;
        std::wstring wide = ToWide(line);
        auto tab = wide.find(L'\t');
        if (tab != std::wstring::npos) wide.replace(tab, 1, L" = ");
        SendMessageW(g_hwndSkillList, LB_ADDSTRING, 0, (LPARAM)wide.c_str());
        count++;
    }
    wchar_t buf[64]; swprintf_s(buf, L"Loaded %d skills for %s.", count, chr);
    SetGameResult(buf);
}

static void DoSetSkill() {
    if (!CheckServerRunning(L"Set Skill")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name.",
            L"Set Skill", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int sel = (int)SendMessage(g_hwndSkillList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        MessageBoxW(g_hwndMain, L"Select a skill from the list.",
            L"Set Skill", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t valBuf[64] = {};
    GetWindowTextW(g_hwndSkillValue, valBuf, 64);
    if (!valBuf[0]) {
        MessageBoxW(g_hwndMain, L"Enter a skill value.",
            L"Set Skill", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t item[512] = {};
    SendMessageW(g_hwndSkillList, LB_GETTEXT, sel, (LPARAM)item);
    // Extract skill ID (number before ":" or " = ")
    std::wstring str = item;
    std::wstring skillId;
    for (auto c : str) {
        if (iswdigit(c)) skillId += c;
        else break;
    }
    if (skillId.empty()) return;
    int newVal = _wtoi(valBuf);
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain, L"Character must be offline.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    // Upsert skill
    RunQuery(L"INSERT INTO character_skills (id, skill_id, value) VALUES ("
             L"(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" +
             std::wstring(chr) + L"')), " + skillId + L", " + std::to_wstring(newVal) +
             L") ON DUPLICATE KEY UPDATE value=" + std::to_wstring(newVal));
    SetGameResult((L"Skill " + skillId + L" set to " + std::to_wstring(newVal) +
                   L" for " + std::wstring(chr) + L".").c_str());
}

// ============================================================
// NEW OPERATIONS — Admin Tools (GM/GodMode)
// ============================================================

static void DoToggleGM() {
    if (!CheckServerRunning(L"Toggle GM")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndAdmGMChar, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name.", L"Toggle GM", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Check account has GM status
    std::wstring checkSql =
        L"SELECT a.status FROM account a JOIN character_data cd ON cd.account_id=a.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::string statusOut = TrimRight(RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + checkSql + L"\" quarm"));
    if (statusOut.empty() || statusOut.find("255") == std::string::npos) {
        SetAdmResult(L"Account for '" + std::wstring(chr) + L"' does not have GM status (255). Set GM first via Make GM.");
        return;
    }
    // Toggle gm flag: if gm=0, set to 1; if gm>0, set to 0
    std::wstring sql =
        L"UPDATE character_data SET gm = IF(gm=0, 1, 0) "
        L"WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')";
    RunQuery(sql);
    std::wstring result = RunQueryTable(
        L"SELECT name, gm FROM character_data WHERE LOWER(name)=LOWER('" +
        std::wstring(chr) + L"')");
    SetAdmResult(L"GM flag toggled for '" + std::wstring(chr) + L"'.\r\n\r\n" + result +
                 L"\r\nCharacter must relog for change to take effect.");
}

static void DoToggleGodMode() {
    if (!CheckServerRunning(L"Toggle God Mode")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndAdmGMChar, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name.", L"Toggle God Mode", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Fetch account status and current god mode flags in one query
    std::wstring checkSql =
        L"SELECT a.status, a.flymode, a.gmspeed, a.gminvul, a.hideme "
        L"FROM account a JOIN character_data cd ON cd.account_id=a.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::string statusOut = TrimRight(RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + checkSql + L"\" quarm"));
    if (statusOut.empty()) {
        SetAdmResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }
    if (statusOut.find("255") == std::string::npos) {
        SetAdmResult(L"Account for '" + std::wstring(chr) + L"' does not have GM status (255). Set GM first via Make GM.");
        return;
    }
    // Parse flags from the data row — if any flag is on, toggle all off; otherwise toggle all on
    bool anyOn = false;
    {
        std::istringstream ss(statusOut);
        std::string line;
        while (std::getline(ss, line)) {
            std::istringstream ls(line);
            std::string tok;
            std::vector<std::string> cols;
            while (std::getline(ls, tok, '\t')) cols.push_back(tok);
            if (cols.size() >= 5) {
                // cols: status, flymode, gmspeed, gminvul, hideme
                for (int i = 1; i <= 4; ++i)
                    if (!cols[i].empty() && cols[i] != "0") { anyOn = true; break; }
                break;
            }
        }
    }
    std::wstring newVal = anyOn ? L"0" : L"1";
    RunQuery(
        L"UPDATE account a JOIN character_data cd ON cd.account_id=a.id "
        L"SET a.flymode=" + newVal + L", a.gmspeed=" + newVal +
        L", a.gminvul=" + newVal + L", a.hideme=" + newVal +
        L" WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')");
    std::wstring result = RunQueryTable(
        L"SELECT a.name AS account, a.flymode, a.gmspeed, a.gminvul, a.hideme "
        L"FROM account a JOIN character_data cd ON cd.account_id=a.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')");
    std::wstring state = anyOn ? L"OFF" : L"ON";
    SetAdmResult(L"God Mode toggled " + state + L" for '" + std::wstring(chr) + L"'.\r\n\r\n" +
                 result + L"\r\nCharacter must relog for change to take effect.");
}

// ============================================================
// NEW OPERATIONS — Server Tab (Weather)
// ============================================================

static void DoEnvLoad() {
    if (!CheckServerRunning(L"Load Zone")) return;
    wchar_t zone[128] = {};
    GetWindowTextW(g_hwndEnvZoneEdit, zone, 128);
    if (!zone[0]) {
        MessageBoxW(g_hwndMain, L"Enter a zone short name (e.g. 'commons').",
            L"Load Zone", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(zone)) { if (c==L'\'') safe+=L"''"; else safe+=c; }
    std::wstring sql =
        L"SELECT weather, fog_minclip, fog_maxclip, fog_density, "
        L"fog_red, fog_green, fog_blue, minclip, maxclip "
        L"FROM zone WHERE short_name='" + safe + L"' LIMIT 1";
    std::string out = TrimRight(RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + sql + L"\" quarm"));
    if (out.empty()) {
        if (g_hwndZoneResult)
            SetWindowTextW(g_hwndZoneResult, (L"Zone '" + std::wstring(zone) + L"' not found.").c_str());
        return;
    }
    // Parse tab-separated: weather, fog_minclip, fog_maxclip, fog_density, fog_r, fog_g, fog_b, minclip, maxclip
    std::istringstream ss(out);
    std::string vals[9];
    for (int i = 0; i < 9 && ss; ++i) {
        if (i < 8) std::getline(ss, vals[i], '\t');
        else std::getline(ss, vals[i]);
    }
    SendMessage(g_hwndEnvWeatherCbo, CB_SETCURSEL, std::atoi(vals[0].c_str()), 0);
    SetWindowTextW(g_hwndEnvFogMin, ToWide(vals[1]).c_str());
    SetWindowTextW(g_hwndEnvFogMax, ToWide(vals[2]).c_str());
    SetWindowTextW(g_hwndEnvFogDensity, ToWide(vals[3]).c_str());
    SetWindowTextW(g_hwndEnvFogR, ToWide(vals[4]).c_str());
    SetWindowTextW(g_hwndEnvFogG, ToWide(vals[5]).c_str());
    SetWindowTextW(g_hwndEnvFogB, ToWide(vals[6]).c_str());
    SetWindowTextW(g_hwndEnvClipMin, ToWide(vals[7]).c_str());
    SetWindowTextW(g_hwndEnvClipMax, ToWide(vals[8]).c_str());
    if (g_hwndZoneResult)
        SetWindowTextW(g_hwndZoneResult,
            (L"Loaded environment for '" + std::wstring(zone) + L"'.").c_str());
}

static void DoEnvSave() {
    if (!CheckServerRunning(L"Save Zone")) return;
    wchar_t zone[128] = {};
    GetWindowTextW(g_hwndEnvZoneEdit, zone, 128);
    if (!zone[0]) {
        MessageBoxW(g_hwndMain, L"Enter a zone short name first.",
            L"Save Zone", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Read all fields
    wchar_t fogMin[32]={}, fogMax[32]={}, fogDen[32]={}, fogR[16]={}, fogG[16]={}, fogB[16]={};
    wchar_t clipMin[32]={}, clipMax[32]={};
    GetWindowTextW(g_hwndEnvFogMin, fogMin, 32);
    GetWindowTextW(g_hwndEnvFogMax, fogMax, 32);
    GetWindowTextW(g_hwndEnvFogDensity, fogDen, 32);
    GetWindowTextW(g_hwndEnvFogR, fogR, 16);
    GetWindowTextW(g_hwndEnvFogG, fogG, 16);
    GetWindowTextW(g_hwndEnvFogB, fogB, 16);
    GetWindowTextW(g_hwndEnvClipMin, clipMin, 32);
    GetWindowTextW(g_hwndEnvClipMax, clipMax, 32);
    int weather = (int)SendMessage(g_hwndEnvWeatherCbo, CB_GETCURSEL, 0, 0);
    if (weather == CB_ERR) weather = 0;

    std::wstring safe;
    for (wchar_t c : std::wstring(zone)) { if (c==L'\'') safe+=L"''"; else safe+=c; }
    RunQuery(L"UPDATE zone SET "
             L"weather=" + std::to_wstring(weather) +
             L", fog_minclip=" + (fogMin[0]?fogMin:L"0") +
             L", fog_maxclip=" + (fogMax[0]?fogMax:L"0") +
             L", fog_density=" + (fogDen[0]?fogDen:L"0") +
             L", fog_red=" + (fogR[0]?fogR:L"0") +
             L", fog_green=" + (fogG[0]?fogG:L"0") +
             L", fog_blue=" + (fogB[0]?fogB:L"0") +
             L", minclip=" + (clipMin[0]?clipMin:L"0") +
             L", maxclip=" + (clipMax[0]?clipMax:L"0") +
             L" WHERE short_name='" + safe + L"'");
    if (g_hwndZoneResult)
        SetWindowTextW(g_hwndZoneResult,
            (L"Zone environment saved for '" + std::wstring(zone) +
             L"'. Restart the zone for changes to take effect.").c_str());
}

// Helper: extract zone short_name from list item (second column, after first tab)
static std::wstring ExtractZoneFromList() {
    int sel = (int)SendMessage(g_hwndZoneList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return L"";
    wchar_t item[512] = {};
    SendMessageW(g_hwndZoneList, LB_GETTEXT, sel, (LPARAM)item);
    std::wstring str = item;
    // Format: "Long Name\tshort_name\tspawns\tzem"
    auto tab1 = str.find(L'\t');
    if (tab1 == std::wstring::npos) return L"";
    auto tab2 = str.find(L'\t', tab1 + 1);
    std::wstring zoneName = str.substr(tab1 + 1, (tab2 != std::wstring::npos) ? tab2 - tab1 - 1 : std::wstring::npos);
    // Trim whitespace
    while (!zoneName.empty() && zoneName.back() == L' ') zoneName.pop_back();
    while (!zoneName.empty() && zoneName.front() == L' ') zoneName.erase(zoneName.begin());
    return zoneName;
}

static void DoLoadZem() {
    if (!CheckServerRunning(L"Load ZEM")) return;
    std::wstring zoneName = ExtractZoneFromList();
    if (zoneName.empty()) {
        if (g_hwndZemLabel) SetWindowTextW(g_hwndZemLabel, L"(select a zone from the list first)");
        return;
    }
    std::wstring safe;
    for (wchar_t c : zoneName) {
        if (c == L'\'') safe += L"''"; else safe += c;
    }
    std::string out = TrimRight(RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"SELECT zone_exp_multiplier FROM zone WHERE short_name='" + safe + L"'\" quarm"));
    std::wstring zem = ToWide(out);
    while (!zem.empty() && (zem.back() == L'\n' || zem.back() == L'\r' || zem.back() == L' '))
        zem.pop_back();
    if (zem.empty()) zem = L"0";
    if (g_hwndZemValue) SetWindowTextW(g_hwndZemValue, zem.c_str());
    if (g_hwndZemLabel) SetWindowTextW(g_hwndZemLabel,
        (L"Zone: " + zoneName + L"  (current ZEM: " + zem + L")").c_str());
}

static void DoSaveZem() {
    if (!CheckServerRunning(L"Save ZEM")) return;
    std::wstring zoneName = ExtractZoneFromList();
    if (zoneName.empty()) {
        MessageBoxW(g_hwndMain, L"Select a zone from the list first.", L"Save ZEM", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t val[64] = {};
    GetWindowTextW(g_hwndZemValue, val, 64);
    if (!val[0]) { MessageBoxW(g_hwndMain, L"Enter a ZEM value.", L"ZEM", MB_OK | MB_ICONINFORMATION); return; }
    std::wstring safe;
    for (wchar_t c : zoneName) { if (c == L'\'') safe += L"''"; else safe += c; }
    RunQuery(L"UPDATE zone SET zone_exp_multiplier=" + std::wstring(val) +
             L" WHERE short_name='" + safe + L"'");
    if (g_hwndZoneResult)
        SetWindowTextW(g_hwndZoneResult,
            (L"ZEM set to " + std::wstring(val) + L" for zone '" + zoneName + L"'. Restart zone to take effect.").c_str());
}

static void DoDefaultZem() {
    if (!CheckServerRunning(L"Reset ZEM")) return;
    std::wstring zoneName = ExtractZoneFromList();
    if (zoneName.empty()) {
        MessageBoxW(g_hwndMain, L"Select a zone from the list first.", L"Reset ZEM", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : zoneName) { if (c == L'\'') safe += L"''"; else safe += c; }
    // Default ZEM is 0 (which means "use global default" in EQEmu)
    RunQuery(L"UPDATE zone SET zone_exp_multiplier=0 WHERE short_name='" + safe + L"'");
    SetWindowTextW(g_hwndZemValue, L"0");
    if (g_hwndZemLabel) SetWindowTextW(g_hwndZemLabel, (L"Zone: " + zoneName + L"  (ZEM reset to default: 0)").c_str());
    if (g_hwndZoneResult)
        SetWindowTextW(g_hwndZoneResult, (L"ZEM reset to default (0) for '" + zoneName + L"'.").c_str());
}

static void DoEnvDefault() {
    if (!CheckServerRunning(L"Reset Zone")) return;
    wchar_t zone[128] = {};
    GetWindowTextW(g_hwndEnvZoneEdit, zone, 128);
    if (!zone[0]) {
        MessageBoxW(g_hwndMain, L"Enter a zone short name first, then click Load Zone.",
            L"Reset Zone", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(zone)) {
        if (c == L'\'') safe += L"''"; else safe += c;
    }
    RunQuery(L"UPDATE zone SET weather=0, fog_minclip=0, fog_maxclip=0, "
             L"fog_density=0, fog_red=0, fog_green=0, fog_blue=0, "
             L"minclip=0, maxclip=0 WHERE short_name='" + safe + L"'");
    // Reload the fields
    SetWindowTextW(g_hwndEnvFogMin, L"0");
    SetWindowTextW(g_hwndEnvFogMax, L"0");
    SetWindowTextW(g_hwndEnvFogDensity, L"0");
    SetWindowTextW(g_hwndEnvFogR, L"0");
    SetWindowTextW(g_hwndEnvFogG, L"0");
    SetWindowTextW(g_hwndEnvFogB, L"0");
    SetWindowTextW(g_hwndEnvClipMin, L"0");
    SetWindowTextW(g_hwndEnvClipMax, L"0");
    SendMessage(g_hwndEnvWeatherCbo, CB_SETCURSEL, 0, 0);
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"Zone '" + std::wstring(zone) + L"' reset to default environment settings.").c_str());
}

static void DoEnvFindZone() {
    if (!CheckServerRunning(L"Find Zone")) return;
    wchar_t term[128] = {};
    if (g_hwndEnvFindEdit) GetWindowTextW(g_hwndEnvFindEdit, term, 128);
    if (!term[0]) {
        MessageBoxW(g_hwndMain, L"Enter a zone name or partial name to search.",
            L"Find Zone", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(term)) {
        if (c == L'\'') safe += L"''"; else safe += c;
    }
    std::wstring sql =
        L"SELECT z.short_name, z.long_name, z.zoneidnumber, z.zone_exp_multiplier AS zem, "
        L"CASE WHEN EXISTS(SELECT 1 FROM spawn2 s2 WHERE s2.zone=z.short_name AND s2.enabled=1) "
        L"THEN 'ACTIVE' ELSE 'empty' END AS status "
        L"FROM zone z WHERE z.short_name LIKE '%" + safe + L"%' OR z.long_name LIKE '%" + safe +
        L"%' ORDER BY z.short_name LIMIT 30";
    if (g_hwndZoneResult)
        SetWindowTextW(g_hwndZoneResult, RunQueryTable(sql).c_str());
}

static void DoViewGuildRoster() {
    if (!CheckServerRunning(L"View Roster")) return;
    wchar_t name[128] = {};
    GetWindowTextW(g_hwndGuildName, name, 128);
    if (!name[0]) {
        MessageBoxW(g_hwndMain, L"Enter a guild name.", L"View Roster", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(name)) {
        if (c == L'\'') safe += L"''"; else safe += c;
    }
    std::wstring sql =
        L"SELECT cd.name AS character_name, cd.level, "
        L"CASE cd.class WHEN 1 THEN 'WAR' WHEN 2 THEN 'CLR' WHEN 3 THEN 'PAL' "
        L"WHEN 4 THEN 'RNG' WHEN 5 THEN 'SHD' WHEN 6 THEN 'DRU' WHEN 7 THEN 'MNK' "
        L"WHEN 8 THEN 'BRD' WHEN 9 THEN 'ROG' WHEN 10 THEN 'SHM' WHEN 11 THEN 'NEC' "
        L"WHEN 12 THEN 'WIZ' WHEN 13 THEN 'MAG' WHEN 14 THEN 'ENC' WHEN 15 THEN 'BST' "
        L"ELSE CAST(cd.class AS CHAR) END AS class, "
        L"CASE gm.rank WHEN 0 THEN 'Member' WHEN 1 THEN 'Officer' WHEN 2 THEN 'Leader' "
        L"ELSE CONCAT('Rank ',gm.rank) END AS guild_rank "
        L"FROM guild_members gm "
        L"JOIN character_data cd ON cd.id=gm.char_id "
        L"WHERE gm.guild_id=(SELECT id FROM guilds WHERE name='" + safe +
        L"') ORDER BY gm.rank DESC, cd.level DESC";
    if (g_hwndServerResult)
        SetWindowTextW(g_hwndServerResult,
            (L"Roster for '" + std::wstring(name) + L"':\r\n" + RunQueryTable(sql)).c_str());
}

// ============================================================
// NEW OPERATIONS — Backup Tab (Clone, DB Size)
// ============================================================

static void DoCloneCharacter() {
    if (!CheckServerRunning(L"Clone Character")) return;
    wchar_t src[128] = {}, dst[128] = {};
    GetWindowTextW(g_hwndCloneSource, src, 128);
    GetWindowTextW(g_hwndCloneNewName, dst, 128);
    if (!src[0] || !dst[0]) {
        MessageBoxW(g_hwndMain, L"Enter both source character name and new clone name.",
            L"Clone Character", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (IsCharacterOnline(src) == 1) {
        MessageBoxW(g_hwndMain, L"Source character must be offline.",
            L"Clone", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (L"Clone '" + std::wstring(src) + L"' as '" + std::wstring(dst) + L"'?\n\n"
         L"This copies all character data, inventory, spells, AAs, and skills\n"
         L"to a new character on the same account.").c_str(),
        L"Confirm Clone", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;
    SetBusy(true);
    SetStatus(L"Cloning character...");
    std::wstring srcW = src, dstW = dst;
    std::thread([srcW, dstW]{
        // Get source character ID and account_id
        std::string idOut = TrimRight(RunCommand(std::wstring(L"docker exec ") + CONTAINER +
            L" mariadb -N -e \"SELECT id, account_id FROM character_data WHERE LOWER(name)=LOWER('" +
            srcW + L"')\" quarm"));
        if (idOut.empty()) {
            auto* res = new AsyncResult{ false, L"Source character '" + srcW + L"' not found." };
            PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_BACKUP, (LPARAM)res);
            return;
        }
        // Insert new character_data row with new name
        RunQuery(L"INSERT INTO character_data "
                 L"(account_id, name, level, race, class, gender, deity, zone_id, x, y, z, "
                 L"last_login, time_played, pvp_status, gm, invulnerable) "
                 L"SELECT account_id, '" + dstW + L"', level, race, class, gender, deity, zone_id, x, y, z, "
                 L"last_login, time_played, pvp_status, gm, invulnerable "
                 L"FROM character_data WHERE LOWER(name)=LOWER('" + srcW + L"')");
        // Copy skills, spells, inventory for the new character
        std::wstring newIdSql = L"(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" + dstW + L"'))";
        std::wstring srcIdSql = L"(SELECT id FROM character_data WHERE LOWER(name)=LOWER('" + srcW + L"'))";
        RunQuery(L"INSERT IGNORE INTO character_skills (id, skill_id, value) "
                 L"SELECT " + newIdSql + L", skill_id, value FROM character_skills WHERE id=" + srcIdSql);
        RunQuery(L"INSERT IGNORE INTO character_spells (id, slot_id, spell_id) "
                 L"SELECT " + newIdSql + L", slot_id, spell_id FROM character_spells WHERE id=" + srcIdSql);
        RunQuery(L"INSERT IGNORE INTO character_languages (id, lang_id, value) "
                 L"SELECT " + newIdSql + L", lang_id, value FROM character_languages WHERE id=" + srcIdSql);
        auto* res = new AsyncResult{ true,
            L"Character '" + srcW + L"' cloned as '" + dstW + L"'.\r\n"
            L"Skills, spells, and languages were copied.\r\n"
            L"Note: inventory items are not duplicated (character starts with empty bags)." };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_BACKUP, (LPARAM)res);
    }).detach();
}

static void DoDbSize() {
    if (!CheckServerRunning(L"Database Size")) return;
    std::wstring sql =
        L"SELECT table_name, "
        L"ROUND(((data_length + index_length) / 1024 / 1024), 2) AS size_mb "
        L"FROM information_schema.tables WHERE table_schema='quarm' "
        L"ORDER BY (data_length + index_length) DESC LIMIT 20";
    std::string out = RunCommand(std::wstring(L"docker exec ") + CONTAINER +
        L" mariadb -N -e \"" + sql + L"\" quarm");
    // Size first column (right-justified), then table name
    std::wstring formatted = L"  Size (MB)  Table Name\r\n";
    formatted += L"  ---------  ----------------------------------------\r\n";
    std::istringstream ss(out);
    std::string line;
    double totalMB = 0;
    while (std::getline(ss, line)) {
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (line.empty()) continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::wstring name = ToWide(line.substr(0, tab));
        std::wstring size = ToWide(line.substr(tab + 1));
        // Right-justify size to 9 chars
        while (size.size() < 9) size = L" " + size;
        formatted += L"  " + size + L"  " + name + L"\r\n";
        try { totalMB += std::stod(std::string(size.begin(), size.end())); } catch (...) {}
    }
    wchar_t totalBuf[64];
    swprintf_s(totalBuf, L"\r\n  %9.2f  TOTAL (top 20 tables)", totalMB);
    formatted += totalBuf;
    if (g_hwndBackupInfo)
        SetWindowTextW(g_hwndBackupInfo, formatted.c_str());
}

static void CreateAdvancedPanel(HWND parent) {
    int y = 14;

    MakeLabel(parent, L"Server Operations:", 20, y, 150, 20);
    y += 24;
    MakeButton(parent, L"Rebuild Server", IDC_BTN_REBUILD,     20,  y, 130, 28);
    MakeButton(parent, L"Start Fresh...", IDC_BTN_START_FRESH, 160, y, 130, 28);
    y += 40;
    g_hwndAdvResult = MakeResultBox(parent, IDC_ADV_RESULT, 20, y, 880, 60);
    y += 74;

    MakeLabel(parent, L"Docker System Info:", 20, y, 150, 20);
    y += 24;
    MakeButton(parent, L"Docker Logs",      IDC_BTN_DOCKER_LOGS,     20,  y, 120, 28);
    MakeButton(parent, L"Disk Usage",       IDC_BTN_DISK_USAGE,     150, y, 110, 28);
    MakeButton(parent, L"Container Stats",  IDC_BTN_CONTAINER_STATS, 270, y, 130, 28);
    y += 40;
    g_hwndAdvSysResult = MakeResultBox(parent, IDC_ADV_SYS_RESULT, 20, y, 880, 100);
    y += 96;

    MakeLabel(parent, L"Utilities:", 20, y, 80, 20);
    y += 24;
    MakeButton(parent, L"Copy eqhost.txt",    IDC_BTN_COPY_EQHOST,  20,  y, 140, 26);
    MakeButton(parent, L"Open Install Folder", IDC_BTN_OPEN_FOLDER, 170, y, 150, 26);
    MakeButton(parent, L"Open Docker Desktop", IDC_BTN_OPEN_DOCKER, 330, y, 150, 26);
    y += 40;

    MakeLabel(parent, L"Settings:", 20, y, 80, 20);
    y += 24;
    HWND chkDark = MakeCheck(parent, L"Dark mode",
                              IDC_CHK_DARK_MODE, 20, y, 140, 22);
    if (GetDarkMode())
        SendMessage(chkDark, BM_SETCHECK, BST_CHECKED, 0);
    y += 26;
    HWND chkAOT = MakeCheck(parent, L"Always on top",
                             IDC_CHK_ALWAYS_ON_TOP, 20, y, 160, 22);
    if (GetAlwaysOnTop())
        SendMessage(chkAOT, BM_SETCHECK, BST_CHECKED, 0);
    y += 26;
    HWND chkAutoStart = MakeCheck(parent, L"Start with Windows",
                                   IDC_CHK_AUTOSTART, 20, y, 160, 22);
    if (GetAutoStartEnabled())
        SendMessage(chkAutoStart, BM_SETCHECK, BST_CHECKED, 0);
    y += 26;
    HWND chkNoBackup = MakeCheck(parent,
        L"Disable automatic backup on stop  (not recommended)",
        IDC_CHK_NO_BACKUP, 20, y, 380, 22);
    if (GetNoBackupOnStop())
        SendMessage(chkNoBackup, BM_SETCHECK, BST_CHECKED, 0);
    y += 28;
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

static void DoDockerLogs() {
    if (g_operationBusy) return;
    if (g_hwndAdvSysResult) SetWindowTextW(g_hwndAdvSysResult, L"Fetching Docker logs...");
    SetBusy(true);
    std::thread([]{
        std::string out = RunCommand(L"docker compose logs --tail=200", g_installDir);
        std::wstring norm = NormalizeNewlines(ToWide(out));
        auto* res = new AsyncResult{ true, norm.empty() ? L"(no output)" : norm };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
    }).detach();
}

static void DoDiskUsage() {
    if (g_operationBusy) return;
    if (g_hwndAdvSysResult) SetWindowTextW(g_hwndAdvSysResult, L"Checking disk usage...");
    SetBusy(true);
    std::thread([]{
        std::string out = RunCommand(L"docker system df");
        std::wstring norm = NormalizeNewlines(ToWide(out));
        auto* res = new AsyncResult{ true, norm.empty() ? L"(no output)" : norm };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
    }).detach();
}

static void DoContainerStats() {
    if (g_operationBusy) return;
    if (g_hwndAdvSysResult) SetWindowTextW(g_hwndAdvSysResult, L"Fetching container stats...");
    SetBusy(true);
    std::thread([]{
        std::wstring cmd = std::wstring(L"docker stats --no-stream ") + CONTAINER;
        std::string out = RunCommand(cmd);
        std::wstring norm = NormalizeNewlines(ToWide(out));
        auto* res = new AsyncResult{ true, norm.empty() ? L"(container not running)" : norm };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
    }).detach();
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
        std::wstring currentSignature = GetRebuildInputSignature();
        std::wstring savedSignature = LoadSavedBuildSignature();
        bool hasImage = HasComposeBuildImage();
        bool needsBuild = currentSignature.empty() || !hasImage || savedSignature.empty() ||
                          currentSignature != savedSignature;

        if (!needsBuild) {
            bool wasRunning = IsContainerRunning();
            if (!wasRunning)
                RunCommand(L"docker compose up -d", g_installDir);
            auto* res = new AsyncResult{
                true,
                wasRunning
                    ? L"Build inputs are unchanged. Skipped docker compose build."
                    : L"Build inputs are unchanged. Skipped docker compose build and started the existing image."
            };
            PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
            return;
        }

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
        SaveBuildSignature(currentSignature);
        auto* res = new AsyncResult{ true, L"Rebuild complete. Server is running." };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
    }).detach();
}

static void DoStartFresh() {
    if (g_operationBusy) return;
    int r = MessageBoxW(g_hwndMain,
        L"WARNING \x2014 THIS WILL PERMANENTLY DELETE ALL CHARACTER DATA.\n\n"
        L"This destroys the quarm-data volume.\n"
        L"ALL characters, accounts, and progress will be lost.\n\n"
        L"This cannot be undone.\n\nAre you absolutely sure?",
        L"START FRESH \x2014 DATA WILL BE DELETED",
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
        SaveBuildSignature(GetRebuildInputSignature());
        auto* res = new AsyncResult{ true, L"Fresh start complete. Server is running with a clean database." };
        PostMessageW(g_hwndMain, WM_ASYNC_DONE, TAB_ADVANCED, (LPARAM)res);
    }).detach();
}

// ============================================================
// TAB 3 — PRO TOOLS PANEL
// ============================================================

static HWND g_hwndGameItemSearch = nullptr;
static HWND g_hwndGameItemId     = nullptr;
static HWND g_hwndGameCharName   = nullptr;
static HWND g_hwndGameResult     = nullptr;

static void SetGameResult(const std::wstring& text) {
    if (g_hwndGameResult)
        SetWindowTextW(g_hwndGameResult, text.c_str());
}

static void CreateGameToolsPanel(HWND parent) {
    int y = 8;

    // --- Character Management ---
    MakeLabel(parent, L"Character Management (character must be offline for most operations):", 20, y, 480, 20);
    y += 22;
    MakeLabel(parent, L"Character:", 20, y+4, 70, 20);
    g_hwndProCharName = MakeEdit(parent, IDC_PRO_CHAR_NAME, 96, y, 200, 24);
    MakeLabel(parent, L"Level:", 316, y+4, 40, 20);
    g_hwndProLevelCbo = MakeCombo(parent, IDC_PRO_LEVEL_COMBO, 360, y, 56, 700);
    for (int lvl = 1; lvl <= 65; ++lvl) {
        wchar_t buf[8]; swprintf_s(buf, L"%d", lvl);
        SendMessageW(g_hwndProLevelCbo, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    SendMessage(g_hwndProLevelCbo, CB_SETCURSEL, 49, 0);
    MakeButton(parent, L"Set Level", IDC_BTN_PRO_SET_LEVEL, 422, y, 80, 26);
    MakeLabel(parent, L"AA:", 514, y+4, 24, 20);
    g_hwndProAaEdit = MakeEdit(parent, IDC_PRO_AA_EDIT, 542, y, 60, 24);
    MakeButton(parent, L"Set AA", IDC_BTN_PRO_SET_AA, 612, y, 70, 26);
    y += 28;

    // Rename (own row)
    MakeLabel(parent, L"Rename:", 20, y+4, 52, 20);
    g_hwndProNewName = MakeEdit(parent, IDC_PRO_NEWNAME, 78, y, 180, 24);
    MakeButton(parent, L"Rename Character", IDC_BTN_PRO_RENAME, 266, y, 130, 26);
    y += 28;

    // Surname (own row)
    MakeLabel(parent, L"Surname:", 20, y+4, 58, 20);
    g_hwndProSurname = MakeEdit(parent, IDC_PRO_SURNAME, 82, y, 180, 24);
    MakeButton(parent, L"Set Surname", IDC_BTN_PRO_SET_SURNAME, 270, y, 100, 26);
    MakeLabel(parent, L"(3-20 chars, alpha only)", 380, y+4, 170, 20);
    y += 28;

    // Title (own row)
    MakeLabel(parent, L"AA Title:", 20, y+4, 58, 20);
    g_hwndProTitleCbo = MakeCombo(parent, IDC_PRO_TITLE_COMBO, 82, y, 280, 200);
    SendMessageW(g_hwndProTitleCbo, CB_ADDSTRING, 0, (LPARAM)L"0 \x2014 None");
    SendMessageW(g_hwndProTitleCbo, CB_ADDSTRING, 0, (LPARAM)L"1 \x2014 General  (Baron / Baroness)");
    SendMessageW(g_hwndProTitleCbo, CB_ADDSTRING, 0, (LPARAM)L"2 \x2014 Archtype (Master / Brother / Veteran)");
    SendMessageW(g_hwndProTitleCbo, CB_ADDSTRING, 0, (LPARAM)L"3 \x2014 Class    (Muse / Marshall / Sage)");
    SendMessage(g_hwndProTitleCbo, CB_SETCURSEL, 0, 0);
    MakeButton(parent, L"Set Title", IDC_BTN_PRO_SET_TITLE, 370, y, 86, 26);
    y += 30;

    // Platinum
    MakeLabel(parent, L"Platinum:", 20, y+4, 58, 20);
    g_hwndProPlatAmount = MakeEdit(parent, IDC_PRO_PLAT_AMOUNT, 82, y, 80, 24);
    MakeButton(parent, L"Give Platinum", IDC_BTN_PRO_GIVE_PLAT, 170, y, 110, 26);
    y += 30;

    // --- Loot Table Viewer ---
    MakeLabel(parent, L"Loot Lookup:", 20, y+4, 84, 20);
    g_hwndLootSearch = MakeEdit(parent, IDC_LOOT_SEARCH, 108, y, 220, 24);
    MakeButton(parent, L"By NPC Name", IDC_BTN_LOOT_BY_NPC,  336, y, 100, 26);
    MakeButton(parent, L"By Item Name", IDC_BTN_LOOT_BY_ITEM, 442, y, 100, 26);
    y += 28;

    // --- Item Lookup + Give ---
    MakeLabel(parent, L"Item Lookup:", 20, y+4, 84, 20);
    g_hwndGameItemSearch = MakeEdit(parent, IDC_GAME_ITEM_SEARCH, 108, y, 220, 24);
    MakeButton(parent, L"Search Items", IDC_BTN_ITEM_SEARCH, 336, y, 100, 26);
    MakeLabel(parent, L"Item ID:", 450, y+4, 52, 20);
    g_hwndGameItemId = MakeEdit(parent, IDC_GAME_ITEM_ID, 506, y, 70, 24);
    MakeButton(parent, L"Give Item", IDC_BTN_GIVE_ITEM, 584, y, 80, 26);
    y += 30;

    // --- Spells ---
    MakeLabel(parent, L"Spells:", 20, y+4, 44, 20);
    g_hwndSpellSearch = MakeEdit(parent, IDC_SPELL_SEARCH, 68, y, 180, 24);
    MakeButton(parent, L"Search", IDC_BTN_SPELL_SEARCH, 256, y, 64, 26);
    MakeButton(parent, L"Scribe Selected", IDC_BTN_SCRIBE_SPELL, 326, y, 110, 26);
    MakeButton(parent, L"Scribe All", IDC_BTN_SCRIBE_ALL, 442, y, 80, 26);
    y += 26;
    g_hwndSpellList = MakeListBox(parent, IDC_SPELL_LIST, 20, y, 560, 56);
    y += 60;

    // --- Skills (Max All Skills after Load Skills) ---
    MakeLabel(parent, L"Skills:", 20, y+4, 42, 20);
    g_hwndSkillSearch = MakeEdit(parent, IDC_SKILL_SEARCH, 66, y, 160, 24);
    MakeButton(parent, L"Search", IDC_BTN_SKILL_SEARCH, 232, y, 64, 26);
    MakeLabel(parent, L"Value:", 308, y+4, 38, 20);
    g_hwndSkillValue = MakeEdit(parent, IDC_SKILL_VALUE, 350, y, 46, 24);
    MakeButton(parent, L"Set Skill", IDC_BTN_SET_SKILL, 402, y, 72, 26);
    MakeButton(parent, L"Load Skills", IDC_BTN_LOAD_SKILLS, 480, y, 90, 26);
    MakeButton(parent, L"Max All Skills", IDC_BTN_MAX_SKILLS, 576, y, 110, 26);
    y += 26;
    g_hwndSkillList = MakeListBox(parent, IDC_SKILL_LIST, 20, y, 560, 56);
    y += 60;

    g_hwndGameResult = MakeResultBox(parent, IDC_GAME_RESULT, 20, y, 940, 120);
}

// ============================================================
// CalcEXPForLevel — mirrors Client::GetEXPForLevel from zone/exp.cpp
// 
// Replicates the server-side formula so the GUI can compute
// the correct exp value when setting a character's level.
//
// Exp varies by RACE and LEVEL (not class).
// Formula: (check_level)^3 * playermod * mod
//   where check_level = level - 1
// ============================================================

static uint32_t CalcEXPForLevel(int level, int raceId) {
    if (level <= 1) return 0;

    int check_level = level - 1;
    double base = (double)check_level * (double)check_level * (double)check_level;

    // Race modifier (playermod starts at 10, then scaled by race %)
    // Source: zone/exp.cpp GetEXPForLevel
    double playermod = 10.0;
    switch (raceId) {
        case 11:  playermod *= 95.0;  break;  // Halfling
        case 2:   playermod *= 105.0; break;  // Barbarian
        case 10:  playermod *= 115.0; break;  // Ogre
        case 9:                                // Troll
        case 128: playermod *= 120.0; break;  // Iksar
        default:  playermod *= 100.0; break;  // Human(1), Erudite(3), Wood Elf(4),
                                               // High Elf(5), Dark Elf(6), Half Elf(7),
                                               // Dwarf(8), Gnome(12), Vah Shir(130)
    }

    // Level modifier
    double mod;
    if      (check_level <= 29) mod = 1.0;
    else if (check_level <= 34) mod = 1.1;
    else if (check_level <= 39) mod = 1.2;
    else if (check_level <= 44) mod = 1.3;
    else if (check_level <= 50) mod = 1.4;
    else if (check_level == 51) mod = 1.5;
    else if (check_level == 52) mod = 1.6;
    else if (check_level == 53) mod = 1.7;
    else if (check_level == 54) mod = 1.9;
    else if (check_level == 55) mod = 2.1;
    else if (check_level == 56) mod = 2.3;
    else if (check_level == 57) mod = 2.5;
    else if (check_level == 58) mod = 2.7;
    else if (check_level == 59) mod = 3.0;
    else if (check_level == 60) mod = 3.0;
    else if (check_level == 61) mod = 3.225;
    else if (check_level == 62) mod = 3.45;
    else if (check_level == 63) mod = 3.675;
    else if (check_level == 64) mod = 3.9;
    else                        mod = 4.125;  // 65+

    return (uint32_t)(base * playermod * mod);
}


// ============================================================
// DoSetCharLevel — UPDATED to set both level AND exp
// ============================================================

static void DoSetCharLevel() {
    if (!CheckServerRunning(L"Set Level")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int sel = (int)SendMessage(g_hwndProLevelCbo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) {
        MessageBoxW(g_hwndMain, L"Select a level from the dropdown.",
            L"Level Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int newLevel = sel + 1;

    // Query name, current level, AND race so we can compute correct exp
    std::wstring checkSql =
        L"SELECT cd.name, cd.level, cd.race FROM character_data cd "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring current = RunQuery(checkSql);
    if (current == L"(no results)") {
        SetGameResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain,
            L"That character is currently online.\r\nHave them log out before changing their level.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }

    // Parse the race ID from the raw query result
    // RunQuery returns header\nval1\tval2\tval3  — we need the 3rd column
    int raceId = 1; // default to Human if parsing fails
    {
        std::wstring raceRaw = RunQuery(
            L"SELECT race FROM character_data WHERE LOWER(name)=LOWER('" +
            std::wstring(chr) + L"')");
        // Strip header line, grab the number
        bool pastHeader = false;
        std::wstring num;
        for (auto c : raceRaw) {
            if (c == L'\n') { pastHeader = true; continue; }
            if (c == L'\r') continue;
            if (pastHeader && iswdigit(c)) num += c;
            else if (pastHeader && !num.empty()) break;
        }
        if (!num.empty()) raceId = _wtoi(num.c_str());
    }

    // Compute correct exp for the new level + this character's race
    uint32_t newExp = CalcEXPForLevel(newLevel, raceId);

    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Set '") + chr + L"' to level " + std::to_wstring(newLevel) +
         L"?\r\n(exp will be set to " + std::to_wstring(newExp) + L")" +
         L"\r\n\r\n" + current + L"\r\nContinue?").c_str(),
        L"Confirm Set Level", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;

    // Update BOTH level and exp in one statement
    RunQuery(L"UPDATE character_data SET level=" + std::to_wstring(newLevel) +
             L", exp=" + std::to_wstring(newExp) +
             L" WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')");

    SetGameResult(std::wstring(L"Level set to ") + std::to_wstring(newLevel) +
        L" for '" + chr + L"' (exp=" + std::to_wstring(newExp) + L").\r\n\r\n"
        L"Character will see the new level on next login.\r\n"
        L"Note: XP bar and spell list may need a zone-in to fully update.");
}

static void DoSetAAPoints() {
    if (!CheckServerRunning(L"Set AA Points")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t aaStr[32] = {};
    GetWindowTextW(g_hwndProAaEdit, aaStr, 32);
    if (!aaStr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a number of AA points.",
            L"AA Points Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    for (wchar_t* p = aaStr; *p; p++) {
        if (!iswdigit(*p)) {
            MessageBoxW(g_hwndMain, L"AA Points must be a positive whole number.",
                L"Invalid Value", MB_OK | MB_ICONWARNING);
            return;
        }
    }
    std::wstring checkSql =
        L"SELECT cd.name, cd.aa_points, cd.aa_points_spent FROM character_data cd "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring current = RunQuery(checkSql);
    if (current == L"(no results)") {
        SetGameResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain,
            L"That character is currently online.\r\nHave them log out before modifying AA points.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Set unspent AA points to ") + aaStr + L" for '" + chr + L"'?\r\n\r\n" +
         current + L"\r\nSpent AAs are not changed.\r\n\r\nContinue?").c_str(),
        L"Confirm Set AA Points", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE character_data SET aa_points=" + std::wstring(aaStr) +
             L" WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')");
    SetGameResult(std::wstring(L"AA points set to ") + aaStr +
        L" (unspent) for '" + chr + L"'.\r\n\r\nCharacter will see the updated count on next login.");
}

static void DoSetCharClass() {
    if (!CheckServerRunning(L"Change Class")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int sel = (int)SendMessage(g_hwndProClassCbo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR || sel < 0 || sel >= 15) {
        MessageBoxW(g_hwndMain, L"Select a class from the dropdown.",
            L"Class Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int newClassId = sel + 1; // class IDs are 1-15
    const wchar_t* newClassName = CLASS_NAMES[sel];

    std::wstring checkSql =
        L"SELECT cd.name, cd.level, "
        L"CASE cd.class WHEN 1 THEN 'Warrior' WHEN 2 THEN 'Cleric' WHEN 3 THEN 'Paladin' "
        L"WHEN 4 THEN 'Ranger' WHEN 5 THEN 'Shadow Knight' WHEN 6 THEN 'Druid' "
        L"WHEN 7 THEN 'Monk' WHEN 8 THEN 'Bard' WHEN 9 THEN 'Rogue' "
        L"WHEN 10 THEN 'Shaman' WHEN 11 THEN 'Necromancer' WHEN 12 THEN 'Wizard' "
        L"WHEN 13 THEN 'Magician' WHEN 14 THEN 'Enchanter' WHEN 15 THEN 'Beastlord' "
        L"ELSE CAST(cd.class AS CHAR) END AS current_class "
        L"FROM character_data cd WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring current = RunQuery(checkSql);
    if (current == L"(no results)") {
        SetGameResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain,
            L"That character is currently online.\r\nHave them log out before changing their class.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Change '") + chr + L"' to class " + newClassName + L"?\r\n\r\n" +
         current + L"\r\n"
         L"WARNING: Class change does not validate race/class combinations,\r\n"
         L"re-grant appropriate spells, or refund AA points.\r\n"
         L"A server restart is required for the change to take effect.\r\n\r\nContinue?").c_str(),
        L"Confirm Change Class", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE character_data SET class=" + std::to_wstring(newClassId) +
             L" WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')");
    SetGameResult(std::wstring(L"Class changed to ") + newClassName +
        L" (ID=" + std::to_wstring(newClassId) + L") for '" + chr + L"'.\r\n\r\n"
        L"Restart the server for the change to take effect fully.");
}

static void DoSetCharRace() {
    if (!CheckServerRunning(L"Change Race")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int sel = (int)SendMessage(g_hwndProRaceCbo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR || sel < 0 || sel >= RACE_TABLE_COUNT) {
        MessageBoxW(g_hwndMain, L"Select a race from the dropdown.",
            L"Race Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int newRaceId     = RACE_TABLE[sel].id;
    const wchar_t* newRaceName = RACE_TABLE[sel].name;

    std::wstring checkSql =
        L"SELECT cd.name, cd.level, cd.race FROM character_data cd "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring current = RunQuery(checkSql);
    if (current == L"(no results)") {
        SetGameResult(std::wstring(L"Character '") + chr + L"' not found.");
        return;
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain,
            L"That character is currently online.\r\nHave them log out before changing their race.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Change '") + chr + L"' to race " + newRaceName + L"?\r\n\r\n" +
         current + L"\r\n"
         L"WARNING: Race change does not validate race/class combinations\r\n"
         L"or update race-specific starting stats.\r\n"
         L"A server restart is required for the change to take effect.\r\n\r\nContinue?").c_str(),
        L"Confirm Change Race", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE character_data SET race=" + std::to_wstring(newRaceId) +
             L" WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')");
    SetGameResult(std::wstring(L"Race changed to ") + newRaceName +
        L" (ID=" + std::to_wstring(newRaceId) + L") for '" + chr + L"'.\r\n\r\n"
        L"Restart the server for the change to take effect fully.");
}

static void DoSetCharGender() {
    if (!CheckServerRunning(L"Change Gender")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int sel = (int)SendMessage(g_hwndProGenderCbo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR) return;
    int newGender = sel; // 0=Male, 1=Female
    const wchar_t* genderName = (sel == 0) ? L"Male" : L"Female";

    std::wstring checkSql =
        L"SELECT cd.name, CASE cd.gender WHEN 0 THEN 'Male' WHEN 1 THEN 'Female' ELSE 'Unknown' END AS gender "
        L"FROM character_data cd WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring current = RunQuery(checkSql);
    if (current == L"(no results)") {
        SetGameResult(std::wstring(L"Character '") + chr + L"' not found."); return;
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain,
            L"That character is currently online.\r\nHave them log out before changing their gender.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Change '") + chr + L"' to " + genderName + L"?\r\n\r\n" +
         current + L"\r\nServer restart required.\r\n\r\nContinue?").c_str(),
        L"Confirm Change Gender", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE character_data SET gender=" + std::to_wstring(newGender) +
             L" WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')");
    SetGameResult(std::wstring(L"Gender set to ") + genderName +
        L" for '" + chr + L"'. Restart the server to take effect.");
}

static void DoProGivePlatinum() {
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Give Platinum Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t amtStr[32] = {};
    if (g_hwndProPlatAmount) GetWindowTextW(g_hwndProPlatAmount, amtStr, 32);
    if (!amtStr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a platinum amount.",
            L"Amount Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    for (wchar_t* p = amtStr; *p; p++) {
        if (!iswdigit(*p)) {
            MessageBoxW(g_hwndMain, L"Amount must be a positive whole number.",
                L"Invalid Amount", MB_OK | MB_ICONWARNING);
            return;
        }
    }
    if (!CheckServerRunning(L"Give Platinum")) return;
    std::wstring checkSql =
        L"SELECT cc.platinum AS carried, cc.platinum_bank AS banked "
        L"FROM character_currency cc JOIN character_data cd ON cd.id=cc.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring current = RunQueryTable(checkSql);
    SetGameResult(std::wstring(L"Current platinum for '") + chr + L"':\r\n\r\n" + current);
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Add ") + amtStr + L" platinum to '" + chr + L"' (carried)?\r\n\r\n" +
         current + L"\r\nWARNING: Character should be LOGGED OUT.\r\n\r\nContinue?").c_str(),
        L"Confirm Give Platinum", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE character_currency cc JOIN character_data cd ON cd.id=cc.id "
             L"SET cc.platinum=cc.platinum+" + std::wstring(amtStr) +
             L" WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')");
    SetGameResult(std::wstring(L"Added ") + amtStr + L" platinum to '" + chr + L"' (carried).");
}

static void DoRenameCharacter() {
    if (!CheckServerRunning(L"Rename Character")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter the current character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t newName[128] = {};
    if (g_hwndProNewName) GetWindowTextW(g_hwndProNewName, newName, 128);
    if (!newName[0]) {
        MessageBoxW(g_hwndMain, L"Enter a new name in the Rename field.",
            L"New Name Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    // Basic validation: alpha only, 4-15 chars, starts with uppercase
    size_t len = wcslen(newName);
    if (len < 4 || len > 15) {
        MessageBoxW(g_hwndMain, L"Name must be 4-15 characters.", L"Invalid Name", MB_OK | MB_ICONWARNING);
        return;
    }
    for (size_t i = 0; i < len; i++) {
        if (!iswalpha(newName[i])) {
            MessageBoxW(g_hwndMain, L"Name must contain only letters.", L"Invalid Name", MB_OK | MB_ICONWARNING);
            return;
        }
    }
    // Check not already taken
    std::wstring checkTaken = RunQuery(
        L"SELECT COUNT(*) FROM character_data WHERE LOWER(name)=LOWER('" + std::wstring(newName) + L"')");
    if (checkTaken.find(L"1") != std::wstring::npos) {
        SetGameResult(std::wstring(L"The name '") + newName + L"' is already in use.");
        return;
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain, L"Character must be offline to rename.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Rename '") + chr + L"' to '" + newName + L"'?\r\n\r\nContinue?").c_str(),
        L"Confirm Rename", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE character_data SET name='" + std::wstring(newName) +
             L"' WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')");
    SetGameResult(std::wstring(L"'") + chr + L"' renamed to '" + newName + L"'.\r\n\r\n"
        L"Update the Character field if you want to make further changes to this character.");
}

static void DoSetSurname() {
    if (!CheckServerRunning(L"Set Surname")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t sname[64] = {};
    if (g_hwndProSurname) GetWindowTextW(g_hwndProSurname, sname, 64);
    size_t len = wcslen(sname);
    if (sname[0] && (len < 3 || len > 20)) {
        MessageBoxW(g_hwndMain, L"Surname must be 3-20 characters (or blank to clear).",
            L"Invalid Surname", MB_OK | MB_ICONWARNING);
        return;
    }
    if (sname[0]) {
        for (size_t i = 0; i < len; i++) {
            if (!iswalpha(sname[i]) && sname[i] != L'\'' ) {
                MessageBoxW(g_hwndMain, L"Surname must contain only letters (apostrophe allowed).",
                    L"Invalid Surname", MB_OK | MB_ICONWARNING);
                return;
            }
        }
    }
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain, L"Character must be offline to change surname.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring display = sname[0] ? std::wstring(L"'") + sname + L"'" : L"(clear surname)";
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Set surname for '") + chr + L"' to " + display + L"?\r\n\r\nContinue?").c_str(),
        L"Confirm Set Surname", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE character_data SET last_name='" + std::wstring(sname) +
             L"' WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')");
    SetGameResult(std::wstring(L"Surname set to ") + display + L" for '" + chr + L"'.");
}

static void DoSetAATitle() {
    if (!CheckServerRunning(L"Set AA Title")) return;
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int sel = g_hwndProTitleCbo ?
        (int)SendMessage(g_hwndProTitleCbo, CB_GETCURSEL, 0, 0) : 0;
    if (sel == CB_ERR) return;
    const wchar_t* titleNames[] = {
        L"None (0)", L"General \x2014 Baron/Baroness (1)",
        L"Archtype \x2014 Master/Brother/Veteran/Venerable (2)",
        L"Class \x2014 Muse/Marshall/Sage/Duke (3)"
    };
    if (IsCharacterOnline(chr) == 1) {
        MessageBoxW(g_hwndMain, L"Character must be offline to change AA title.",
            L"Character Online", MB_OK | MB_ICONWARNING);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Set AA title for '") + chr + L"' to:\r\n\r\n" +
         titleNames[sel] + L"\r\n\r\nContinue?").c_str(),
        L"Confirm Set AA Title", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;
    RunQuery(L"UPDATE character_data SET title=" + std::to_wstring(sel) +
             L" WHERE LOWER(name)=LOWER('" + std::wstring(chr) + L"')");
    SetGameResult(std::wstring(L"AA title set to ") + titleNames[sel] + L" for '" + chr + L"'.");
}

// ============================================================
// Pro Tools versions of Currency / Inventory
// Read from Pro Tools character field, output to Pro Tools result
// ============================================================

static void DoProShowInventory() {
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!CheckServerRunning(L"Show Inventory")) return;

    std::wstring sql =
        L"SELECT ci.slotid, i.name, ci.charges "
        L"FROM character_inventory ci "
        L"JOIN items i ON i.id=ci.itemid "
        L"JOIN character_data cd ON cd.id=ci.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"') "
        L"ORDER BY ci.slotid";
    std::wstring result = RunQueryTable(sql);
    SetGameResult(std::wstring(L"Inventory for '") + chr +
        L"' (slots 0-21 are equipped):\r\n\r\n" + result);
}

static void DoProShowCurrency() {
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
    if (!chr[0]) {
        MessageBoxW(g_hwndMain, L"Enter a character name in the Character field.",
            L"Character Required", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!CheckServerRunning(L"Show Currency")) return;

    std::wstring sql =
        L"SELECT cc.platinum, cc.gold, cc.silver, cc.copper, "
        L"cc.platinum_bank, cc.gold_bank, cc.silver_bank, cc.copper_bank, "
        L"cc.platinum_cursor, cc.gold_cursor, cc.silver_cursor, cc.copper_cursor "
        L"FROM character_currency cc "
        L"JOIN character_data cd ON cd.id=cc.id "
        L"WHERE LOWER(cd.name)=LOWER('" + std::wstring(chr) + L"')";
    std::wstring result = RunQueryTable(sql);
    SetGameResult(std::wstring(L"Currency for '") + chr + L"':\r\n\r\n" + result);
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
        std::wstring safe;
        for (wchar_t c : std::wstring(search)) {
            if (c == L'\'') safe += L"''";
            else if (c == L'`') continue;
            else safe += c;
        }
        std::wstring safeUnderscore = safe;
        for (auto& c : safeUnderscore) if (c == L' ') c = L'_';
        sql = L"SELECT id, Name, ItemType, Classes, Races, Slots, Price, StackSize "
              L"FROM items WHERE REPLACE(LOWER(Name),'_',' ') LIKE LOWER('%" + safe + L"%') "
              L"OR LOWER(Name) LIKE LOWER('%" + safeUnderscore + L"%') "
              L"ORDER BY Name LIMIT 50";
    }
    std::wstring result = RunQueryTable(sql);
    SetGameResult(std::wstring(L"Item search for '") + search + L"':\r\n\r\n" + result);
}

static void DoGiveItem() {
    wchar_t chr[128] = {};
    GetWindowTextW(g_hwndProCharName, chr, 128);
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

    // Find open general inventory slot (22-29 = slotGeneral1-8), searching from highest
    std::wstring slotSql = L"SELECT slotid FROM character_inventory "
                           L"WHERE id=" + charIdStr +
                           L" AND slotid BETWEEN 22 AND 29 ORDER BY slotid";
    std::wstring slotResult = RunQuery(slotSql);

    bool slotUsed[8] = {};
    if (slotResult != L"(no results)") {
        std::wstring num;
        for (auto c : slotResult) {
            if (iswdigit(c)) { num += c; }
            else if (!num.empty()) {
                int slot = _wtoi(num.c_str());
                if (slot >= 22 && slot <= 29) slotUsed[slot - 22] = true;
                num.clear();
            }
        }
        if (!num.empty()) {
            int slot = _wtoi(num.c_str());
            if (slot >= 22 && slot <= 29) slotUsed[slot - 22] = true;
        }
    }

    int openSlot = -1;
    for (int i = 7; i >= 0; --i) {
        if (!slotUsed[i]) { openSlot = 22 + i; break; }
    }
    if (openSlot == -1) {
        SetGameResult(std::wstring(L"Character '") + chr +
            L"' has no open general inventory slots (slotGeneral1-8 all full).\r\n"
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
         L"*** SERVER RESTART REQUIRED ***\r\n"
         L"All connected players will be disconnected.\r\n\r\n"
         L"Restart now?").c_str(),
        L"Confirm Era Change", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;

    g_eraUserDirty = false;
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

    SetGameResult(std::wstring(L"Era set to '") + p.name + L"'. Restarting server...\r\n\r\n"
        L"Note: characters currently in zones restricted by this era will be moved\r\n"
        L"to a safe zone (typically East Commons) on their next login.");
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
        case TAB_ZONES:   if(g_serverRunning) DoRefreshZones(); break;
        case TAB_BACKUP:  RefreshBackupList();   break;
        case TAB_NETWORK: RefreshNetworkTab();   break;
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

    if (g_darkMode) {
        switch (msg) {
        case WM_ERASEBKGND:
            if (g_hbrDark) {
                RECT rc; GetClientRect(hwnd, &rc);
                FillRect((HDC)wp, &rc, g_hbrDark);
                return 1;
            }
            break;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, CLR_DARK_TXT);
            SetBkColor(hdc, CLR_DARK_BG);
            return (LRESULT)g_hbrDark;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR: {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, CLR_DARK_TXT);
            SetBkColor(hdc, CLR_DARK_CTL);
            return (LRESULT)g_hbrDarkCtl;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            if (dis->CtlType == ODT_BUTTON) {
                COLORREF bgCol = RGB(52, 52, 58);
                COLORREF txtCol = RGB(210, 210, 215);
                COLORREF borderCol = RGB(70, 70, 78);
                if (dis->itemState & ODS_SELECTED) {
                    bgCol = RGB(65, 65, 75);
                    borderCol = RGB(90, 140, 200);
                }
                if (dis->itemState & ODS_DISABLED) {
                    txtCol = RGB(100, 100, 105);
                    borderCol = RGB(55, 55, 60);
                }
                HBRUSH hbr = CreateSolidBrush(bgCol);
                FillRect(dis->hDC, &dis->rcItem, hbr);
                DeleteObject(hbr);
                HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
                HPEN hOld = (HPEN)SelectObject(dis->hDC, hPen);
                HBRUSH hOldBr = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top,
                         dis->rcItem.right, dis->rcItem.bottom, 4, 4);
                SelectObject(dis->hDC, hOld);
                SelectObject(dis->hDC, hOldBr);
                DeleteObject(hPen);
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, txtCol);
                wchar_t text[256] = {};
                GetWindowTextW(dis->hwndItem, text, 256);
                DrawTextW(dis->hDC, text, -1, &dis->rcItem,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }
        } // end switch
    } // end if(g_darkMode)

    // Always forward WM_HSCROLL regardless of dark mode
    if (msg == WM_HSCROLL) {
        SendMessageW(GetParent(hwnd), msg, wp, lp);
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================
// NEW OPERATIONS — Status Tab (Server Name + Login Marquee)
// ============================================================

static void DoLoadServerName() {
    if (!IsContainerRunning()) return;
    std::string content = RunCommand(
        std::wstring(L"docker exec ") + CONTAINER + L" cat /src/build/bin/eqemu_config.json");
    auto pos = content.find("\"longname\"");
    if (pos == std::string::npos) return;
    pos = content.find(':', pos);
    if (pos == std::string::npos) return;
    pos = content.find('"', pos);
    if (pos == std::string::npos) return;
    ++pos;
    auto end = content.find('"', pos);
    if (end == std::string::npos) return;
    std::wstring val = ToWide(content.substr(pos, end - pos));
    if (!val.empty() && g_hwndServerNameEdit)
        SetWindowTextW(g_hwndServerNameEdit, val.c_str());
}

static void DoLoadMarquee() {
    // ticker lives in tblloginserversettings where type='ticker'
    if (!IsContainerRunning()) return;
    std::wstring result = RunQuery(
        L"SELECT value FROM tblloginserversettings WHERE type='ticker' LIMIT 1");
    if (result == L"(no results)") {
        // No row yet — field stays blank, user can set one
        return;
    }
    std::wstring val;
    bool pastHeader = false;
    for (auto c : result) {
        if (c == L'\n') { pastHeader = true; continue; }
        if (c == L'\r') continue;
        if (pastHeader) val += c;
    }
    while (!val.empty() && (val.back() == L' ' || val.back() == L'\t')) val.pop_back();
    if (!val.empty() && g_hwndMarqueeEdit)
        SetWindowTextW(g_hwndMarqueeEdit, val.c_str());
}

static void DoSetServerName() {
    wchar_t name[256] = {};
    if (g_hwndServerNameEdit) GetWindowTextW(g_hwndServerNameEdit, name, 256);
    if (!name[0]) {
        MessageBoxW(g_hwndMain, L"Enter a server name first.", L"Server Name", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Set server name to:\r\n\r\n\"") + name + L"\"\r\n\r\n"
         L"*** SERVER RESTART REQUIRED ***\r\n"
         L"All connected players will be disconnected.\r\n\r\n"
         L"Continue and restart now?").c_str(),
        L"Confirm Server Name Change", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;
    if (!IsContainerRunning()) {
        SetServerPanelResult(L"Failed: server must be running to update /src/build/bin/eqemu_config.json inside the container.");
        return;
    }

    std::string content = RunCommand(
        std::wstring(L"docker exec ") + CONTAINER + L" cat /src/build/bin/eqemu_config.json");
    if (content.empty()) {
        SetServerPanelResult(
            L"Failed: could not read /src/build/bin/eqemu_config.json from the running container.");
        return;
    }
    auto pos = content.find("\"longname\"");
    if (pos == std::string::npos) {
        SetServerPanelResult(L"Failed: \"longname\" not found in /src/build/bin/eqemu_config.json.");
        return;
    }
    pos = content.find(':', pos);
    pos = content.find('"', pos);
    auto end = content.find('"', pos + 1);
    std::string newName(name, name + wcslen(name));
    content = content.substr(0, pos + 1) + newName + content.substr(end);

    wchar_t tempPath[MAX_PATH];
    wcscpy_s(tempPath, g_installDir);
    PathAppendW(tempPath, L"eqemu_config.container.tmp.json");
    if (!WriteTextFileWide(tempPath, ToWide(content))) {
        SetServerPanelResult(std::wstring(L"Failed: could not write temp file ") + tempPath);
        return;
    }

    DWORD copyOutEc = 0;
    RunCommand(std::wstring(L"docker cp \"") + tempPath + L"\" " + CONTAINER +
               L":/src/build/bin/eqemu_config.json", g_installDir, &copyOutEc);
    if (copyOutEc != 0) {
        SetServerPanelResult(L"Failed: could not copy updated eqemu_config.json into the container.");
        return;
    }

    SetServerPanelResult(std::wstring(L"Server name set to '") + name + L"'.\r\n\r\nRestarting server...");
    DoRestartServerAsync();
}

static void DoSetMarquee() {
    if (!CheckServerRunning(L"Set Login Marquee")) return;
    wchar_t msg[512] = {};
    if (g_hwndMarqueeEdit) GetWindowTextW(g_hwndMarqueeEdit, msg, 512);
    if (!msg[0]) {
        MessageBoxW(g_hwndMain, L"Enter a marquee message first.", L"Login Marquee", MB_OK | MB_ICONINFORMATION);
        return;
    }
    int r = MessageBoxW(g_hwndMain,
        (std::wstring(L"Set login screen marquee to:\r\n\r\n\"") + msg + L"\"\r\n\r\n"
         L"This updates tblloginserversettings and takes effect\r\n"
         L"immediately for new client connections.\r\n\r\n"
         L"Continue?").c_str(),
        L"Confirm Marquee Change", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (r != IDYES) return;
    // First verify the table exists
    std::wstring check = RunQuery(
        L"SHOW TABLES LIKE 'tblloginserversettings'");
    if (check == L"(no results)") {
        SetServerPanelResult(
            L"Failed: table 'tblloginserversettings' does not exist in the quarm database.\r\n\r\n"
            L"The marquee text may be hardcoded in this server build.");
        return;
    }
    std::wstring safe;
    for (wchar_t c : std::wstring(msg)) {
        if (c == L'\'') safe += L"''";
        else safe += c;
    }
    RunQuery(L"UPDATE tblloginserversettings SET value='" + safe + L"' WHERE type='ticker'");
    SetServerPanelResult(std::wstring(L"Login marquee set to '") + msg + L"'.\r\n\r\n"
         L"Reconnect to the server select screen to see the change.");
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

        // Large bold font for player count banner
        g_hFontLarge = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

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
        CreatePlayerPanel(g_hwndPanels[TAB_PLAYER]);
        CreateGameToolsPanel(g_hwndPanels[TAB_GAME]);
        CreateAdminPanel(g_hwndPanels[TAB_ADMIN]);
        CreateZonePanel(g_hwndPanels[TAB_ZONES]);
        CreateServerPanel(g_hwndPanels[TAB_SERVER]);
        CreateBackupPanel(g_hwndPanels[TAB_BACKUP]);
        CreateLogPanel(g_hwndPanels[TAB_LOG]);
        CreateNetworkPanel(g_hwndPanels[TAB_NETWORK]);
        CreateAdvancedPanel(g_hwndPanels[TAB_ADVANCED]);

        for (int i = 0; i < NUM_TABS; ++i)
            ApplyFont(g_hwndPanels[i], g_hFont);

        // Re-apply mono font to result boxes (ApplyFont would overwrite them)
        if (g_hFontMono) {
            if (g_hwndAdmResult)    SendMessage(g_hwndAdmResult,    WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndPlrResult)    SendMessage(g_hwndPlrResult,    WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndAdvResult)    SendMessage(g_hwndAdvResult,    WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndAdvSysResult) SendMessage(g_hwndAdvSysResult, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndLogText)      SendMessage(g_hwndLogText,      WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndProcList)     SendMessage(g_hwndProcList,     WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndGameResult)   SendMessage(g_hwndGameResult,   WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndServerResult) SendMessage(g_hwndServerResult, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndStatusResult) SendMessage(g_hwndStatusResult, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
            if (g_hwndZoneResult)   SendMessage(g_hwndZoneResult,   WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
        }
        if (g_hFontLarge && g_hwndPlayerCount)
            SendMessage(g_hwndPlayerCount, WM_SETFONT, (WPARAM)g_hFontLarge, TRUE);

        SetTimer(hwnd, TIMER_POLL, POLL_MS, nullptr);
        PostMessage(hwnd, WM_STATUS_POLL, 0, 0);
        return 0;
    }

    case WM_SIZE:
        LayoutPanels();
        return 0;

    case WM_ERASEBKGND:
        if (g_darkMode && g_hbrDark) {
            RECT rc; GetClientRect(hwnd, &rc);
            FillRect((HDC)wp, &rc, g_hbrDark);
            return 1;
        }
        break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        if (g_darkMode && g_hbrDark) {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, CLR_DARK_TXT);
            SetBkColor(hdc, CLR_DARK_BG);
            return (LRESULT)g_hbrDark;
        }
        break;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        if (g_darkMode && g_hbrDarkCtl) {
            HDC hdc = (HDC)wp;
            SetTextColor(hdc, CLR_DARK_TXT);
            SetBkColor(hdc, CLR_DARK_CTL);
            return (LRESULT)g_hbrDarkCtl;
        }
        break;

    case WM_TIMER:
        if (wp == TIMER_POLL && !g_operationBusy) {
            std::thread([hwnd]{
                bool running = IsContainerRunning();
                PostMessageW(hwnd, WM_STATUS_POLL, running ? 1 : 0, 0);
            }).detach();
        } else if (wp == TIMER_LOG_REFRESH && !g_operationBusy) {
            int curTab = TabCtrl_GetCurSel(g_hwndTab);
            if (curTab == TAB_LOG) DoLoadLog();
        }
        return 0;

    case WM_STATUS_POLL: {
        g_serverRunning = (wp != 0);
        int curTab = TabCtrl_GetCurSel(g_hwndTab);
        if (curTab == TAB_STATUS) RefreshStatusTab();
        else if (curTab == TAB_ZONES) {
            if (g_serverRunning) DoRefreshZones();
            else if (g_hwndZoneList) {
                SendMessage(g_hwndZoneList, LB_RESETCONTENT, 0, 0);
                SendMessageW(g_hwndZoneList, LB_ADDSTRING, 0, (LPARAM)L"(server is not running)");
            }
        }
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
            if (res->success) {
                g_logFullText = res->message;  // store for filtering
                SetWindowTextW(g_hwndLogText, res->message.c_str());
                DoApplyLogFilter();  // re-apply any active filter
            } else {
                SetWindowTextW(g_hwndLogText, L"Failed to load log.");
            }
        } else if (sourceTab == TAB_ADVANCED) {
            // Docker system info goes to sysresult box; rebuild/fresh errors go to advresult
            if (res->success && g_hwndAdvSysResult &&
                GetWindowTextLengthW(g_hwndAdvSysResult) > 0) {
                // sys info operations set text on g_hwndAdvSysResult directly before async
                SetWindowTextW(g_hwndAdvSysResult, res->message.c_str());
            } else {
                if (g_hwndAdvResult) SetWindowTextW(g_hwndAdvResult, res->message.c_str());
                if (!res->success)
                    MessageBoxW(hwnd, res->message.c_str(), L"Operation Failed", MB_OK | MB_ICONERROR);
            }
        } else if (sourceTab == TAB_NETWORK) {
            if (g_hwndNetStatusMsg)
                SetWindowTextW(g_hwndNetStatusMsg, res->message.c_str());
            SetStatus(L"Port test complete.");
        } else if (sourceTab == TAB_GAME) {
            if (g_hwndGameResult)
                SetWindowTextW(g_hwndGameResult, res->message.c_str());
        } else if (sourceTab == TAB_SERVER) {
            if (g_hwndServerResult)
                SetWindowTextW(g_hwndServerResult, res->message.c_str());
        } else {
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
        // Status bar custom draw for dark mode
        if (g_darkMode && pnm->hwndFrom == g_hwndStatus && pnm->code == NM_CUSTOMDRAW) {
            LPNMCUSTOMDRAW cd = (LPNMCUSTOMDRAW)lp;
            if (cd->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
                SetTextColor(cd->hdc, CLR_DARK_TXT);
                SetBkColor(cd->hdc, CLR_DARK_BG);
                return CDRF_NEWFONT;
            }
        }
        return 0;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
        // Owner-draw buttons in dark mode — subtle flat style
        if (g_darkMode && dis->CtlType == ODT_BUTTON) {
            COLORREF bgCol = RGB(52, 52, 58);
            COLORREF txtCol = RGB(210, 210, 215);
            COLORREF borderCol = RGB(70, 70, 78);
            if (dis->itemState & ODS_SELECTED) {
                bgCol = RGB(65, 65, 75);
                borderCol = RGB(90, 140, 200);
            }
            if (dis->itemState & ODS_DISABLED) {
                txtCol = RGB(100, 100, 105);
                borderCol = RGB(55, 55, 60);
            }
            HBRUSH hbr = CreateSolidBrush(bgCol);
            FillRect(dis->hDC, &dis->rcItem, hbr);
            DeleteObject(hbr);
            // Subtle rounded-feel border
            HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
            HPEN hOld = (HPEN)SelectObject(dis->hDC, hPen);
            HBRUSH hOldBr = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top,
                     dis->rcItem.right, dis->rcItem.bottom, 4, 4);
            SelectObject(dis->hDC, hOld);
            SelectObject(dis->hDC, hOldBr);
            DeleteObject(hPen);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, txtCol);
            wchar_t text[256] = {};
            GetWindowTextW(dis->hwndItem, text, 256);
            DrawTextW(dis->hDC, text, -1, &dis->rcItem,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }

    case WM_HSCROLL: {
        // XP and AA slider notifications (range 4-40 = 1.00x to 10.00x)
        if ((HWND)lp == g_hwndXpSlider && g_hwndXpLabel) {
            int pos = (int)SendMessage(g_hwndXpSlider, TBM_GETPOS, 0, 0);
            wchar_t buf[32];
            swprintf_s(buf, L"%.2fx", pos / 4.0);
            SetWindowTextW(g_hwndXpLabel, buf);
        }
        if ((HWND)lp == g_hwndAaSlider && g_hwndAaLabel) {
            int pos = (int)SendMessage(g_hwndAaSlider, TBM_GETPOS, 0, 0);
            wchar_t buf[32];
            swprintf_s(buf, L"%.2fx", pos / 4.0);
            SetWindowTextW(g_hwndAaLabel, buf);
        }
        break;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {

        // --- STATUS TAB ---
        case IDC_BTN_SET_MOTD:
            DoSetMOTD();
            break;

        case IDC_BTN_SET_SERVERNAME:
            DoSetServerName();
            break;

        case IDC_BTN_SET_MARQUEE:
            DoSetMarquee();
            break;

        case IDC_BTN_RESTART_SERVER:
            if (!g_operationBusy) DoRestartServerAsync();
            break;

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
        case IDC_BTN_MAKE_GM:          DoMakeGM();             break;
        case IDC_BTN_REMOVE_GM:        DoRemoveGM();           break;
        case IDC_BTN_LIST_ACCOUNTS:    DoListAccounts();       break;
        case IDC_BTN_RESET_PASSWORD:   DoResetPassword();      break;
        case IDC_BTN_SUSPEND_ACCT:     DoSuspendAccount();     break;
        case IDC_BTN_UNSUSPEND_ACCT:   DoUnsuspendAccount();   break;
        case IDC_BTN_BAN_ACCT:         DoBanAccount();         break;
        case IDC_BTN_UNBAN_ACCT:       DoUnbanAccount();       break;
        case IDC_BTN_SERVER_STATS:     DoServerStats();        break;
        case IDC_BTN_VIEW_BANS:        DoViewBans();           break;
        case IDC_BTN_DELETE_CHAR:      DoDeleteCharacter();    break;

        // --- PLAYER TAB ---
        case IDC_BTN_PLR_LIST_CHARS:   DoPlrListChars();    break;
        case IDC_BTN_PLR_CHAR_INFO:    DoPlrCharInfo();     break;
        case IDC_BTN_SHOW_INVENTORY:   DoShowInventory();   break;
        case IDC_BTN_SHOW_CURRENCY:    DoShowCurrency();    break;
        case IDC_BTN_SHOW_ACCT_CHAR:   DoShowAcctForChar(); break;
        case IDC_BTN_MOVE_TO_BIND:     DoMoveToBind();      break;
        case IDC_BTN_FIND_ZONE:        DoFindZone();        break;
        case IDC_BTN_MOVE_TO_ZONE:     DoMoveToZone();      break;
        case IDC_BTN_LIST_CORPSES:     DoListCorpses();     break;
        case IDC_BTN_CORPSES_BY_CHAR:  DoCorpsesByChar();   break;
        case IDC_BTN_PLR_SEARCH:       DoSearchCharacters();break;
        case IDC_BTN_WHO_ONLINE:       DoWhoIsOnline();     break;
        case IDC_BTN_RECENT_LOGINS:    DoRecentLogins();    break;
        case IDC_BTN_IP_HISTORY:       DoIPHistory();       break;

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
        case IDC_BTN_APPLY_FILTER:
            DoApplyLogFilter();
            break;
        case IDC_CHK_AUTO_REFRESH: {
            HWND chkAR = GetDlgItem(g_hwndPanels[TAB_LOG], IDC_CHK_AUTO_REFRESH);
            bool on = chkAR && (SendMessage(chkAR, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (on)
                SetTimer(hwnd, TIMER_LOG_REFRESH, 30000, nullptr);
            else
                KillTimer(hwnd, TIMER_LOG_REFRESH);
            break;
        }

        case IDC_GAME_ERA_COMBO: {
            if (HIWORD(wp) == CBN_SELCHANGE)
                g_eraUserDirty = true;
            break;
        }

        // --- NETWORK TAB ---
        case IDC_BTN_CHANGE_NETWORK:  DoChangeNetwork();   break;
        case IDC_BTN_NET_CONFIRM:     DoConfirmNetwork();  break;
        case IDC_BTN_WRITE_EQHOST:    DoWriteEqhost();     break;
        case IDC_BTN_COPY_IP:         DoCopyIP();          break;

        // --- ADVANCED TAB ---
        case IDC_BTN_REBUILD:          DoRebuild();         break;
        case IDC_BTN_START_FRESH:      DoStartFresh();      break;
        case IDC_BTN_COPY_EQHOST:      DoCopyEqhost();      break;
        case IDC_BTN_DOCKER_LOGS:      DoDockerLogs();      break;
        case IDC_BTN_DISK_USAGE:       DoDiskUsage();       break;
        case IDC_BTN_CONTAINER_STATS:  DoContainerStats();  break;

        case IDC_BTN_OPEN_FOLDER:
            ShellExecuteW(nullptr, L"open", g_installDir,
                nullptr, nullptr, SW_SHOWNORMAL);
            break;

        case IDC_BTN_OPEN_DOCKER: {
            std::wstring dockerExe = FindDockerDesktopExe();
            if (!dockerExe.empty()) {
                ShellExecuteW(nullptr, L"open", dockerExe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            } else {
                HINSTANCE hr = ShellExecuteW(nullptr, L"open", L"Docker Desktop",
                    nullptr, nullptr, SW_SHOWNORMAL);
                if ((INT_PTR)hr <= 32) {
                    MessageBoxW(g_hwndMain,
                        L"Docker Desktop could not be found or launched.\n\n"
                        L"Please open Docker Desktop manually from your Start Menu.",
                        L"Docker Desktop Not Found", MB_OK | MB_ICONWARNING);
                }
            }
            break;
        }

        case IDC_CHK_AUTOSTART: {
            HWND chk = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_AUTOSTART);
            bool on = (SendMessage(chk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetAutoStart(on);
            break;
        }

        case IDC_CHK_NO_BACKUP: {
            HWND chkNB  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_NO_BACKUP);
            HWND cboR   = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BACKUP_RETENTION);
            HWND chkAOT = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_ALWAYS_ON_TOP);
            HWND chkDM  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_DARK_MODE);
            bool noBackup = (SendMessage(chkNB, BM_GETCHECK, 0, 0) == BST_CHECKED);
            bool aot = chkAOT ? (SendMessage(chkAOT, BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
            bool dm  = chkDM  ? (SendMessage(chkDM,  BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
            int sel = cboR ? (int)SendMessage(cboR, CB_GETCURSEL, 0, 0) : 1;
            int keep = (sel==0?5 : sel==1?10 : sel==2?20 : 0);
            SaveSettings(noBackup, keep, aot, dm);
            if (noBackup)
                MessageBoxW(hwnd,
                    L"Warning: backups will NOT be taken when stopping the server.\n"
                    L"Your character data will not be protected.",
                    L"Backup Warning", MB_OK | MB_ICONWARNING);
            break;
        }

        case IDC_BACKUP_RETENTION: {
            if (HIWORD(wp) == CBN_SELCHANGE) {
                HWND chkNB  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_NO_BACKUP);
                HWND cboR   = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BACKUP_RETENTION);
                HWND chkAOT = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_ALWAYS_ON_TOP);
                HWND chkDM  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_DARK_MODE);
                bool noBackup = chkNB ? (SendMessage(chkNB, BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
                bool aot = chkAOT ? (SendMessage(chkAOT, BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
                bool dm  = chkDM  ? (SendMessage(chkDM,  BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
                int sel = (int)SendMessage(cboR, CB_GETCURSEL, 0, 0);
                int keep = (sel==0?5 : sel==1?10 : sel==2?20 : 0);
                SaveSettings(noBackup, keep, aot, dm);
                PruneOldBackups(keep);
            }
            break;
        }

        case IDC_CHK_ALWAYS_ON_TOP: {
            HWND chkAOT = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_ALWAYS_ON_TOP);
            HWND chkNB  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_NO_BACKUP);
            HWND cboR   = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BACKUP_RETENTION);
            HWND chkDM  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_DARK_MODE);
            bool aot      = (SendMessage(chkAOT, BM_GETCHECK, 0, 0) == BST_CHECKED);
            bool noBackup = chkNB ? (SendMessage(chkNB, BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
            bool dm  = chkDM  ? (SendMessage(chkDM,  BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
            int sel  = cboR ? (int)SendMessage(cboR, CB_GETCURSEL, 0, 0) : 1;
            int keep = (sel==0?5 : sel==1?10 : sel==2?20 : 0);
            SaveSettings(noBackup, keep, aot, dm);
            ApplyAlwaysOnTop(aot);
            break;
        }

        case IDC_CHK_DARK_MODE: {
            HWND chkDM  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_DARK_MODE);
            HWND chkAOT = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_ALWAYS_ON_TOP);
            HWND chkNB  = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_CHK_NO_BACKUP);
            HWND cboR   = GetDlgItem(g_hwndPanels[TAB_ADVANCED], IDC_BACKUP_RETENTION);
            bool dm       = (SendMessage(chkDM,  BM_GETCHECK, 0, 0) == BST_CHECKED);
            bool aot      = chkAOT ? (SendMessage(chkAOT, BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
            bool noBackup = chkNB  ? (SendMessage(chkNB,  BM_GETCHECK, 0, 0) == BST_CHECKED) : false;
            int sel  = cboR ? (int)SendMessage(cboR, CB_GETCURSEL, 0, 0) : 1;
            int keep = (sel==0?5 : sel==1?10 : sel==2?20 : 0);
            SaveSettings(noBackup, keep, aot, dm);
            ApplyDarkMode(dm);
            break;
        }

        // --- PRO TOOLS TAB ---
        case IDC_BTN_ITEM_SEARCH:        DoItemSearch();         break;
        case IDC_BTN_GIVE_ITEM:          DoGiveItem();           break;
        case IDC_BTN_PRO_GIVE_PLAT:      DoProGivePlatinum();    break;
        case IDC_BTN_PRO_SET_LEVEL:      DoSetCharLevel();       break;
        case IDC_BTN_PRO_SET_AA:         DoSetAAPoints();        break;
        case IDC_BTN_PRO_SET_CLASS:      DoSetCharClass();       break;
        case IDC_BTN_PRO_SET_RACE:       DoSetCharRace();        break;
        case IDC_BTN_PRO_SET_GENDER:     DoSetCharGender();      break;
        case IDC_BTN_PRO_RENAME:         DoRenameCharacter();    break;
        case IDC_BTN_PRO_SET_SURNAME:    DoSetSurname();         break;
        case IDC_BTN_PRO_SET_TITLE:      DoSetAATitle();         break;
        case IDC_BTN_SET_ERA:            DoSetEra();             break;
        case IDC_BTN_SET_ZONE_COUNT:     DoSetZoneCount();       break;

        // --- STATUS TAB: Repop & Announce ---
        case IDC_BTN_REPOP_ZONES:        DoRepopZones();         break;
        case IDC_BTN_SEND_ANNOUNCE:      DoSendAnnouncement();   break;

        // --- SERVER TAB: Rule Editor ---
        case IDC_BTN_LOAD_RULES:         DoLoadRules();          break;
        case IDC_BTN_SAVE_RULE:          DoSaveRule();           break;
        case IDC_BTN_RESET_RULE:         DoResetRule();          break;
        case IDC_BTN_APPLY_XP:           DoApplyXP();            break;
        case IDC_BTN_APPLY_AA:           DoApplyAA();            break;

        // --- SERVER TAB: Guild Manager ---
        case IDC_BTN_LIST_GUILDS:        DoListGuilds();         break;
        case IDC_BTN_CREATE_GUILD:       DoCreateGuild();        break;
        case IDC_BTN_SET_GUILD_LEADER:   DoSetGuildLeader();     break;
        case IDC_BTN_DISBAND_GUILD:      DoDisbandGuild();       break;
        case IDC_BTN_VIEW_ROSTER:        DoViewGuildRoster();    break;

        // --- SERVER TAB: Environment ---
        case IDC_BTN_ENV_DEFAULT:        DoEnvDefault();         break;
        case IDC_BTN_ENV_FIND_ZONE:      DoEnvFindZone();        break;

        // --- ZONES TAB: ZEM ---
        case IDC_BTN_ZEM_SAVE:           DoSaveZem();            break;
        case IDC_BTN_ZEM_DEFAULT:        DoDefaultZem();         break;

        // --- ZONES TAB: Zone list selection loads ZEM ---
        case IDC_ZONE_LIST: {
            if (HIWORD(wp) == LBN_SELCHANGE) {
                DoLoadZem();
            }
            break;
        }

        // --- PRO TOOLS: Spawn, Spells, Factions, Skills ---
        case IDC_BTN_SPAWN_BOSS:         DoSpawnBoss();          break;
        case IDC_BTN_SPELL_SEARCH:       DoSpellSearch();        break;
        case IDC_BTN_SCRIBE_SPELL:       DoScribeSpell();        break;
        case IDC_BTN_SCRIBE_ALL:         DoScribeAll();          break;
        case IDC_BTN_LOAD_FACTIONS:      DoLoadFactions();       break;
        case IDC_BTN_SET_FACTION:        DoSetFaction();         break;
        case IDC_BTN_FACTION_ALLY:       DoSetFaction(2000);     break;
        case IDC_BTN_FACTION_WARMLY:     DoSetFaction(1100);     break;
        case IDC_BTN_FACTION_INDIFF:     DoSetFaction(0);        break;
        case IDC_BTN_FACTION_KOS:        DoSetFaction(-2000);    break;
        case IDC_BTN_MAX_SKILLS:         DoMaxSkills();          break;

        // --- PLAYER TOOLS: Loot Viewer ---
        case IDC_BTN_LOOT_BY_NPC:        DoLootByNPC();          break;
        case IDC_BTN_LOOT_BY_ITEM:       DoLootByItem();         break;

        // --- PRO TOOLS: Skill Editor ---
        case IDC_BTN_SKILL_SEARCH:       DoSkillSearch();        break;
        case IDC_BTN_SET_SKILL:          DoSetSkill();           break;
        case IDC_BTN_LOAD_SKILLS:        DoLoadSkills();         break;

        // --- ADMIN TOOLS: GM/GodMode ---
        case IDC_BTN_TOGGLE_GM:          DoToggleGM();           break;
        case IDC_BTN_TOGGLE_GODMODE:     DoToggleGodMode();      break;

        // --- SERVER TAB: Zone Environment ---
        case IDC_BTN_ENV_LOAD:           DoEnvLoad();            break;
        case IDC_BTN_ENV_SAVE:           DoEnvSave();            break;

        // --- STATUS: Zone Management ---
        case IDC_BTN_REFRESH_ZONES:      DoRefreshZones();       break;
        case IDC_BTN_STOP_ZONE:          DoStopZone();           break;
        case IDC_BTN_RESTART_ZONE:       DoRestartZone();        break;
        case IDC_BTN_START_ZONE:         DoStartZone();          break;
        case IDC_BTN_FIND_ZONE_STATUS:   DoFindZoneStatus();     break;

        // --- STATUS: Boss Management ---
        case IDC_BTN_CHECK_BOSS:         DoCheckBoss();          break;
        case IDC_BTN_DESPAWN_BOSS:       DoDespawnBoss();        break;
        case IDC_BTN_LIST_ACTIVE_BOSS:   DoListActiveBosses();   break;

        // --- BACKUP TAB: Clone + DB Size ---
        case IDC_BTN_CLONE_CHAR:         DoCloneCharacter();     break;
        case IDC_BTN_DB_SIZE:            DoDbSize();             break;

        // --- SERVER TAB: Rule List selection ---
        case IDC_RULE_LIST: {
            if (HIWORD(wp) == LBN_SELCHANGE) {
                int sel = (int)SendMessage(g_hwndRuleList, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR && sel < (int)g_rulesFiltered.size()) {
                    SetWindowTextW(g_hwndRuleSelected, g_rulesFiltered[sel].name.c_str());
                    SetWindowTextW(g_hwndRuleValue, g_rulesFiltered[sel].value.c_str());
                }
            }
            break;
        }

        // --- SERVER TAB: Rule Search (filter on Enter) ---
        case IDC_RULE_SEARCH: {
            if (HIWORD(wp) == EN_CHANGE && !g_rules.empty()) {
                // Re-filter rule list
                wchar_t filterBuf[256] = {};
                GetWindowTextW(g_hwndRuleSearch, filterBuf, 256);
                std::wstring filter = filterBuf;
                for (auto& c : filter) c = towlower(c);
                g_rulesFiltered.clear();
                SendMessage(g_hwndRuleList, LB_RESETCONTENT, 0, 0);
                for (auto& r : g_rules) {
                    if (!filter.empty()) {
                        std::wstring nameLo = r.name;
                        for (auto& c : nameLo) c = towlower(c);
                        if (nameLo.find(filter) == std::wstring::npos) continue;
                    }
                    g_rulesFiltered.push_back(r);
                    std::wstring display = r.name + L" = " + r.value;
                    SendMessageW(g_hwndRuleList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
                }
            }
            break;
        }

        } // end switch id
        return 0;
    }

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize = { 1000, 780 };
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_POLL);
        KillTimer(hwnd, TIMER_LOG_REFRESH);
        if (g_hbrDark)       DeleteObject(g_hbrDark);
        if (g_hbrDarkCtl)    DeleteObject(g_hbrDarkCtl);
        if (g_hbrDarkAccent) DeleteObject(g_hbrDarkAccent);
        if (g_hFontLarge)    DeleteObject(g_hFontLarge);
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

    // Check Docker is installed using registry + all drive letters
    bool dockerInstalled = !FindDockerDesktopExe().empty();
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
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 860,
        nullptr, nullptr, hInst, nullptr);
    if (!g_hwndMain) return 1;

    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);

    if (GetAlwaysOnTop())
        ApplyAlwaysOnTop(true);
    if (GetDarkMode())
        ApplyDarkMode(true);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // Let mousewheel messages pass through directly to controls
        // (IsDialogMessage eats them, breaking combo box scrolling)
        if (msg.message == WM_MOUSEWHEEL || msg.message == WM_MOUSEHWHEEL) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else if (!IsDialogMessage(g_hwndMain, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}
