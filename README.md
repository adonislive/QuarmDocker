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
| **Status** | Start and stop the server. Shows whether each internal service (database, world, zones, etc.) is running. |
| **Admin Tools** | Manage accounts — make/remove GM, list accounts, reset passwords, see who is online and recent logins. |
| **Player Tools** | Look up characters, view inventory and currency, move a stuck character, give platinum, view corpses. |
| **Backup & Restore** | Take a manual backup, restore from a previous backup, export/import character data. |
| **Log Viewer** | View the live server log output from inside the container. |
| **Network** | Switch between local-only and LAN mode. Write eqhost.txt to your EQ client folder. |
| **Advanced** | Rebuild the server image, wipe and start fresh, open install folder, settings. |

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

The server automatically takes a backup every time you stop it through the app. Backups are saved to `C:\QuarmDocker\config\backups\`.

To take a manual backup at any time, go to the **Backup & Restore** tab and click **Backup Now**.

To restore: select a backup from the list and click **Restore Selected**. The server will stop, restore, and restart automatically.

---

## Troubleshooting

**The server shows STOPPED and won't start**
Make sure Docker Desktop is open and running (look for the whale icon in the system tray). Then try Start Server again.

**I can't connect with my EQ client**
- Check that the server shows RUNNING in the Status tab and all services show RUNNING
- Check that `eqhost.txt` contains the correct IP address
- For LAN connections, make sure you are using the server machine's LAN IP, not `127.0.0.1`

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

Thanks to surron, darius, kicnlag, solar, and secrets for framework, ingenuity, grounding, advice, and space to code.  Thanks to Starrlord for Quality Assurance testing.

Based on code frame from [EQMacEmuDockerHub](https://github.com/jcon321/EQMacEmuDockerHub).
