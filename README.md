# Quarm Docker Server

Run your own private [Project Quarm](https://www.projectquarm.com/) Server on Windows — no command line required.

The installer handles everything: WSL2, Docker Desktop, server setup, firewall rules, and a desktop shortcut to **Quarm Docker Server**, a point-and-click management app for your server.

> **This is a local/LAN server intended for personal use.** It is not designed for public internet hosting.

---

## Requirements

- Windows 10 / 11, 64-bit
- 8 GB RAM minimum (16 GB recommended)
- 15 GB free disk space
- TAKP client + [Quarm Patcher](https://github.com/SecretsOTheP/eqemupatcher/releases/) + (optional) [Zeal](https://github.com/CoastalRedwood/Zeal/releases/)
- [Docker Desktop](https://docs.docker.com/desktop/install/windows-install/) (Windows/Mac) or Docker Engine (Linux)

---

## Installation

👉 **[Download the latest release](https://github.com/adonislive/QuarmDocker/releases)**

1. Download `QuarmDockerInstaller.exe`
2. Right-click it and choose **Run as administrator**
3. Follow the on-screen steps — the installer will check your system, install any missing components, and set up the server
4. When it finishes, a **Quarm Docker Server** shortcut will be on your desktop

> If WSL2 needs to be installed, the installer will ask you to reboot. Just run the installer again after rebooting — your choices are saved.

> Building from source takes **40 - 55 minutes**. The window may appear frozen during this time — that is normal. Do not close it.

---

## First Time Setup

### Step 1 — Configure your EQ client

After installation, the completion screen shows your `eqhost.txt` content — copy it and paste it into the `eqhost.txt` file in your TAKP client folder. You can also find it later in **Quarm Docker Server** on the **Network** tab.

Your `eqhost.txt` should look like this (for local play):
```
[Registration Servers]
{
"127.0.0.1:6000"
}
[Login Servers]
{
"127.0.0.1:6000"
}
```

### Step 2 — Log in and create your account

Launch your TAKP client. At the login screen, enter any username and password you want — your account is created automatically on first login. You will be asked to confirm the password twice.

### Step 3 — Make yourself a GM

Open **Quarm Docker Server** shortcut on desktop and go to the **Admin Tools** tab. Type your account name in the Account field and click **Make GM**. Log out and back in for the change to take effect.

---

## Managing Your Server

Open the **Quarm Docker Server** app from your desktop shortcut. All server management is done from here — no command line needed.

| Tab | What it does |
|-----|-------------|
| **Status** | Start, stop, and restart the server. Shows service health, uptime, active player count, and app version. Set the server era (Classic through Planes of Power or All Expansions), change the server name shown at server select, change the login screen scrolling marquee, change the Message of the Day, send server-wide announcements, and manage raid bosses (check, spawn, despawn, list active). |
| **Player Tools** | Look up characters by account or name, view inventory, currency, and corpses. Move stuck characters to their bind point or a specific zone. Search characters by level range and class. View recent logins, who is online, and IP history. Load and edit character faction standings with quick-set presets (Ally, Warmly, Indifferent, KOS). |
| **Pro Tools** | Full character management — set level, AA points, race, class, gender, rename characters, set surnames, and assign AA titles. Give platinum, search and give items by name or ID, open the Custom Item Editor to duplicate stock items into the `900000+` range, customize those variants, restore them from their source item, delete them from loot and merchant data, and still use Give Item with either stock or custom IDs. Search and scribe spells, or use the Spellbook Builder to load class spells by level range, optionally limit to missing spells, optionally clear the spellbook first, and then bulk-scribe the loaded results. A separate Clear Existing Spellbook action wipes both the spellbook and memorized gems. View and edit individual skills or max all skills, and look up loot tables by NPC or item name. |
| **Admin Tools** | Manage accounts — make/remove GM, list accounts, reset passwords, suspend/unsuspend, ban/unban, and view active bans. View server-wide statistics. Delete characters (with double confirmation). Toggle in-game GM flag and God Mode on individual characters. |
| **Zones** | View all running zones with spawn counts and ZEM (Zone Experience Modifier) values. Stop, restart, or repop individual zones. Start new zone instances, search for zones by name, and adjust dynamic zone count. Full zone environment editor — load any zone and adjust weather type, fog (start, end, density, color), and clip distances. |
| **Server** | Adjust server-wide XP and AA rate multipliers with sliders (1x–10x). Full rule editor — search, browse, and modify any server rule value. Guild manager — list guilds, create new guilds, set leaders, disband, and view rosters. |
| **Backup & Restore** | Take manual backups, restore from a previous backup, export/import character data. Clone a character with all gear, AAs, and spells under a new name. View database size. |
| **Log Viewer** | View live server logs from multiple sources (container stdout, eqemu_debug.log, world.log, crash.log). Filter log output by keyword, choose how many lines to display, and enable auto-refresh on a 30-second interval. |
| **Network** | View current server address and network mode (Local, LAN, or WAN). Switch between local-only and LAN mode by selecting a network adapter. Write eqhost.txt directly to your EQ client folder and copy your IP to clipboard. |
| **Advanced** | Rebuild the server image or wipe and start fresh. View Docker logs, disk usage, and container stats. Utilities to copy eqhost.txt, open the install folder, or launch Docker Desktop. Settings for dark mode, always-on-top, start with Windows, backup-on-stop toggle, and backup retention (keep last 5, 10, 20, or unlimited backups). |

---

## Customizing Your Server

### Server Name
The server name is what players see on the server select screen. To change it, go to the **Server** tab, edit the **Server Name** field, and click **Set Name**. A server restart is required for the change to take effect.

### Login Screen Marquee
The scrolling message players see on the login screen can be changed on the **Server** tab. Edit the **Login Marquee** field and click **Set Marquee**. A server restart is required.

### Message of the Day
The in-game MOTD is shown to players when they zone. Edit the **MOTD** field on the **Server** tab and click **Set MOTD**. This takes effect on the next zone-in and does not require a restart.

### Server Era
Select an era from the dropdown on the **Server** tab and click **Set Era**. This restricts zone access and expansion flags to the chosen era. A server restart is required. Characters in zones that become restricted will be moved to a safe zone (typically East Commons) on their next login.

### Spellbook Builder
On the **Pro Tools** tab, the spell section now includes a **Spellbook Builder** for rebuilding or filling out a character's spellbook without manually searching for every spell.

- Enter the target character name
- Leave **Use Character's Actual Class** enabled to pull the class from the database automatically, or turn it off and choose a class manually
- Choose **Min Level** and **Max Level**
- Leave **Only Missing Spells** enabled to avoid re-scribing spells the character already knows
- Optionally enable **Also Clear Before Scribing** if you want the bulk scribe action to wipe the spellbook and memorized gems first
- Click **Load Matching Spells** to preview the exact spell list that will be used
- Click **Scribe Loaded Spells** to apply the loaded set

If you want to wipe the spellbook without loading anything first, use **Clear Existing Spellbook**. This clears both `character_spells` and `character_memmed_spells`.

### Custom Item Editor
On the **Pro Tools** tab, the item section now includes **Load Item Editor**, which opens a separate **Custom Item Editor** window for item variant work.

- Search or enter an item ID, then click **Load Item Editor**
- Stock items load read-only
- Click **Duplicate as Custom** to clone the item into the `900000+` range
- New custom variants default to the name `Custom <Original Name>`
- The duplicate is added alongside the original in both loot tables and merchant lists
- Edit the custom variant's approved fields, then click **Save Changes**
- Use **Restore to Default** to copy the source item's stats and fields back onto the custom item while keeping the custom item ID and its loot/merchant placements
- Use **Delete Custom Item** to remove the custom variant from loot tables, merchant lists, the custom item metadata table, and the `items` row itself

The editor includes the approved v1 fields:

- `Name`, `Lore`, `IDFile`, `Icon`, `Price`, `SellRate`, `Weight`
- `AC`, `HP`, `Mana`, primary stats, and resists
- `Damage`, `Delay`, `Classes`, `Races`, `Slots`
- `ReqLevel`, `RecLevel`, `MaxCharges`
- `Click`, `Proc`, `Worn`, and `Focus` effect IDs
- `Magic`, `Lore Flag`, `No Drop`, `No Rent`, and `Attuneable`

**Give Item** still works with both stock and custom item IDs.

---

## Playing With Others on Your Network (LAN)

To let other computers on your home network connect to your server:

1. Open **Quarm Docker Server** shortcut on desktop and go to the **Network** tab
2. Click **Change Network Setting**
3. Select your network adapter from the list (it will show your LAN IP address)
4. Click **Confirm Selection**
5. Restart the server from the **Status** tab
6. On each connecting computer, edit `eqhost.txt` and replace `127.0.0.1` with your server's LAN IP address (shown in the Network tab)

> Players on other computers will need their own TAKP client with the Quarm Patcher applied.

---

## Backups

The server automatically takes a backup every time you stop it through the app (unless disabled in Advanced settings). Backups are saved to `C:\QuarmDocker\config\backups\`.

To take a manual backup at any time, go to the **Backup & Restore** tab and click **Backup Now**.

To restore: select a backup from the list and click **Restore Selected**. The server will stop, restore, and restart automatically.

Backup retention is configurable in the **Advanced** tab — keep the last 5, 10, 20, or unlimited backups.

---

## Troubleshooting

**The server shows STOPPED and won't start**
Make sure Docker Desktop is open and running (look for the whale icon in the system tray). Then try Start Server again.

**I can't connect with my EQ client**
- Check that the server shows RUNNING in the Status tab and all services show RUNNING
- Check that `eqhost.txt` contains the correct IP address
- For LAN connections, make sure you are using the server machine's LAN IP, not `127.0.0.1`

**Docker Desktop won't open from the app**
The app locates Docker Desktop automatically using the Windows registry and common install paths. If it still fails, open Docker Desktop manually from your Start Menu.

**Firewall rules exit: 1 in the install log**
The installer may not have had administrator rights. Right-click the installer and choose **Run as administrator**, or ask someone to run the firewall PowerShell commands manually from an elevated PowerShell prompt.

**The build failed during installation**
Click **Rebuild Server** in the Advanced tab. Docker will resume from where it stopped — you do not need to start over.

---

## Updating the Server

To update to the latest Quarm server code:

1. Open **Quarm Docker Server** and go to the **Advanced** tab
2. Click **Rebuild Server**
3. The app will take a backup, rebuild the image from the latest source, and restart

This takes 40–55 minutes. Your character data is always preserved.

---

## Uninstalling

Go to **Windows Settings → Apps**, search for **Quarm Docker Server**, and click Uninstall.

This removes the app, firewall rules, shortcuts, and install folder. Your Docker Desktop installation is not removed. You will be asked whether to delete character data during uninstall.

---

## Sources

- **Server**: https://github.com/SecretsOTheP/EQMacEmu
- **Quests**: https://github.com/SecretsOTheP/quests
- **Maps**: https://github.com/EQMacEmu/Maps

## Credits

Thanks to surron, darius, kicnlag, solar, and secrets for framework, ingenuity, grounding, advice, and space to code. Thanks to Starrlord for Quality Assurance testing.

Based on code frame from [EQMacEmuDockerHub](https://github.com/jcon321/EQMacEmuDockerHub).
