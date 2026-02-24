QUARM DOCKER — WINDOWS
======================

These scripts manage your Quarm server from Windows.
All backup files are saved in the config/backups/ folder in the main QuarmDocker folder.

start.bat
  Starts the Quarm server.
  Double-click this to start the server before launching EverQuest.

stop.bat
  Backs up the database, then stops the server.
  Always use this instead of stopping the server from Docker Desktop.

backup.bat
  Backs up the database while the server is running.
  Run this before making any changes in HeidiSQL or before a rebuild.
  Backups are saved as config/backups/backup_YYYY-MM-DD_HHMM.sql

restore.bat
  Restores the database from a previous backup.
  The server must be running before you restore.
  You will be asked to confirm before anything is overwritten.

IMPORTANT
---------
***Note: Running "docker compose down -v" permanently deletes your database.
Use stop.bat to shut down the server safely.
