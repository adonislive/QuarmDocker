# Quarm Docker Server

A self-contained Docker image that builds and runs a [Project Quarm](https://www.projectquarm.com/) (EQMacEmu) server from source. Everything runs in a single container — MariaDB, the login server, world server, and zone servers.

> **This is a local/LAN server intended for personal use.** It is not hardened for public internet hosting.

---

## Download

👉 [Download the latest release](https://github.com/adonislive/QuarmDocker/releases)

Extract the zip to a folder on your computer and follow the Getting Started steps below.

---

## Requirements

- 8GB RAM required (16GB recommended)
- 10GB free disk space
- TAKP client + [Quarm Patcher](https://github.com/SecretsOTheP/eqemupatcher/releases/) + (optional) [Zeal](https://github.com/CoastalRedwood/Zeal/releases/)
- [Docker Desktop](https://docs.docker.com/desktop/install/windows-install/) (Windows/Mac) or Docker Engine (Linux)

---

## Getting Started

The setup script checks your system, configures the server, builds it, and starts it. Just follow the prompts. The build takes **30–45 minutes** on first run — do not close the window.

### Windows
Double-click `setup.bat`

### Mac
Double-click `setup.command`

### Linux
Open a terminal in the QuarmDocker folder and run:
```bash
chmod +x setup.sh && ./setup.sh
```

---

## Managing Your Server

Scripts for starting the server, stopping the server, backing up the database, and restoring the database are in the folder for your operating system. Open the readme inside for instructions.

- **Windows**: config/win/
- **Mac**: config/mac/
- **Linux**: config/linux/

---

## Need More Detail?

See [Manual_Instructions.md](Manual_Instructions.md) for full technical documentation including manual setup, LAN configuration, HeidiSQL access, command line management, and port reference.

---

## Sources

- **Server**: https://github.com/SecretsOTheP/EQMacEmu
- **Quests**: https://github.com/SecretsOTheP/quests
- **Maps**: https://github.com/EQMacEmu/Maps

## Credits

- **For this project: Thanks to surron, darius, kicnlag, solar, and secrets for framework, ingenuity, grounding, advice, and space to code**
- **Based on code frame from EQMacEmuDockerHub at https://github.com/jcon321/EQMacEmuDockerHub**
