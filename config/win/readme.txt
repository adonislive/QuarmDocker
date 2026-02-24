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

manage.bat
  Interactive server management tool. Run this to access all admin features.
  The server must be running before using any feature.

  ACCOUNT MANAGEMENT
  ------------------
  1.  Make GM
        Grants GM status to an account.
        The account must log out and back in for the change to take effect.

  2.  Remove GM
        Removes GM status from an account.
        The account must log out and back in for the change to take effect.

  3.  List Accounts
        Shows all accounts with their GM status, last login time, and last IP address.
        status 255 = GM, status 0 = normal player.

  4.  Reset Password
        Resets the password for an account.
        Supports any characters in the new password including special characters.
        The account must log out and back in for the change to take effect.

  PLAYER INFO
  -----------
  5.  Who Is Online
        Shows all characters currently logged in with their level, class, race, and zone.
        Class and race are shown as numeric IDs.

  6.  Show Recent Logins
        Shows the 20 most recently active accounts with their last login time and IP address.

  7.  Show Last Known IP
        Shows the most recent IP address used by a specific account.
        Only the last IP is stored — full login history is not available.

  8.  List Characters on Account
        Shows all characters on a specific account with their level, class, race, and zone.

  9.  Character Info
        Shows detailed info for a specific character: level, class, race, zone, online status,
        current HP, and mana.

  10. Show Inventory
        Shows all items in a character's inventory with slot ID and charges.
        Equipped items are slots 0-21.

  11. Show Currency
        Shows all currency for a character: carried, bank, and cursor platinum, gold,
        silver, and copper.

  PLAYER ACTIONS
  --------------
  12. Move Character to Bind Point
        Moves a stuck character to their bind point so they can log in safely.
        The character MUST be logged out before using this. If they are logged in,
        the server will overwrite the change when they log out.

  13. Move Character to Zone
        Moves a character to the safe spawn point of any zone.
        Enter the zone short name e.g. qeynos, commons, unrest, nektulos.
        The character MUST be logged out before using this.

  14. Give Platinum
        Adds platinum to a character's carried currency.
        The character should be logged out before using this.

  CORPSES
  -------
  15. List All Corpses
        Shows all corpses on the server with character name, zone, time of death,
        rez status, and whether the corpse is buried.

  16. Show Corpses by Character
        Shows all corpses belonging to a specific character with zone, time of death,
        rez status, buried status, and any coin on the corpse.

  SERVER
  ------
  17. Server Status
        Shows whether each server process is running: mariadb, loginserver, world,
        eqlaunch, queryserv, ucs, and a count of active zone processes.

IMPORTANT
---------
***Note: Running "docker compose down -v" permanently deletes your database.
Use stop.bat to shut down the server safely.
