QUARM DOCKER — LINUX
====================

These scripts manage your Quarm server from Linux.
All backup files are saved in the config/backups/ folder in the main QuarmDocker folder.

Before first use, make the scripts executable:
  chmod +x *.sh

start.sh
  Starts the Quarm server.
  Run this before launching EverQuest.

stop.sh
  Backs up the database, then stops the server.
  Always use this instead of running docker compose down directly.

backup.sh
  Backs up the database while the server is running.
  Run this before making any changes to the database or before a rebuild.
  Backups are saved as config/backups/backup_YYYY-MM-DD_HHMM.sql

restore.sh
  Restores the database from a previous backup.
  The server must be running before you restore.
  You will be asked to confirm before anything is overwritten.

IMPORTANT
---------
***Note: Running "docker compose down -v" permanently deletes your database.
Use stop.sh to shut down the server safely.
