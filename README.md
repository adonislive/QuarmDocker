# Quarm Docker Server

A self-contained Docker image that builds and runs a [Project Quarm](https://www.projectquarm.com/) (EQMacEmu) server from source. Everything runs in a single container — MariaDB, the login server, world server, and zone servers.

> **This is a local/LAN server intended for personal use.** It is not hardened for public internet hosting.

---

## Requirements

- [Docker Desktop](https://docs.docker.com/desktop/install/windows-install/) (Windows/Mac) or Docker Engine (Linux)
- 8GB RAM recommended
- 10GB free disk space
- [TAKP client]+[Quarm Patcher]+(optional)[Zeal]

---

## Quick Start

**1. Clone this repository**
```bash
git clone https://github.com/adonislive/quarm-docker.git
cd quarm-docker
```

**2. Build the image**

This compiles the Quarm server from source and imports the database. It will take **20–40 minutes** on first build.
```bash
docker compose build
```

**3. Start the server**
```bash
docker compose up -d
```

**4. Configure your EQ client**

In your TAKP client folder, edit `eqhost.txt` to contain:
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

**5. Launch the client and log in**

Use any username and password — your account is created automatically on first login. You will be prompted to enter credentials twice.

---

## Making Yourself a GM

Replace `YOURNAME` with the account name you logged in with:
```bash
docker exec quarm-server mariadb -e "UPDATE account SET status=255 WHERE name='YOURNAME';" quarm
```

Log out and back in for the change to take effect.

---

## Managing the Server

The container can be started and stopped from the **Docker Desktop UI** under the Containers tab, or via the command line:

```bash
# Stop the server
docker compose down

# Start the server
docker compose up -d

# View live logs
docker logs -f quarm-server
```

---

## Database Access (HeidiSQL or similar)

Connect with these settings:
- **Host**: `localhost`
- **Port**: `3306`
- **User**: `quarm`
- **Password**: `quarm`

---

## Starting Fresh

To wipe everything and rebuild from scratch:
```bash
docker compose down -v
docker compose build
docker compose up -d
```

> ⚠️ The `-v` flag deletes the database volume. All characters and account data will be lost.

---

## Ports

| Port | Protocol | Service |
|------|----------|---------|
| 6000 | UDP | Loginserver (client) |
| 5998 | TCP | Loginserver (world) |
| 9000 | UDP+TCP | World server |
| 7778 | UDP | UCS |
| 7375–7400 | UDP+TCP | Boat zones |
| 7000–7374 | UDP+TCP | Dynamic zones |
| 3306 | TCP | MariaDB |

---

## Sources

- **Server**: https://github.com/SecretsOTheP/EQMacEmu
- **Quests**: https://github.com/SecretsOTheP/quests
- **Maps**: https://github.com/EQMacEmu/Maps

## Credits

- **Thanks to surron, darius, kicnlag, solar, and secrets for framework, ingenuity, grounding, advice, and space to code**
