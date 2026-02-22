#!/bin/bash
set -e

echo "Initializing Database"
cd data

# Extract only the latest tar.gz (database_full contains the full history)
LATEST=$(ls *.tar.gz | sort | tail -1)
echo "Extracting $LATEST"
tar xzf "$LATEST"
rm -f *.tar.gz

# Navigate into subdirectory if the archive extracted into one
SUBDIR=$(find . -maxdepth 1 -mindepth 1 -type d 2>/dev/null | head -1)
if [ -n "$SUBDIR" ]; then
    echo "Found subdirectory: $SUBDIR"
    cd "$SUBDIR"
fi

# drop_system.sql drops tables intended to be cleaned before a fresh import.
# We are always starting with a clean empty database so we skip it entirely.
# Running it AFTER the other imports would destroy all the tables we just created.

# Expose mysql to all IPs so host can connect directly (HeidiSQL)
sed -i 's/127\.0\.0\.1/0.0.0.0/g' /etc/mysql/mariadb.conf.d/50-server.cnf

# Raise connection limit - EQ services consume many connections
echo -e "[mysqld]\nmax_connections = 500" > /etc/mysql/mariadb.conf.d/60-custom.cnf

service mariadb start
sleep 3

echo "Drop and create database"
mariadb -e "DROP DATABASE IF EXISTS quarm; \
    CREATE DATABASE quarm; \
    CREATE USER IF NOT EXISTS 'quarm'@'%' IDENTIFIED BY 'quarm'; \
    GRANT ALL PRIVILEGES ON *.* TO 'quarm'@'%'; \
    CREATE USER IF NOT EXISTS 'quarm'@'localhost' IDENTIFIED BY 'quarm'; \
    GRANT ALL PRIVILEGES ON *.* TO 'quarm'@'localhost'; \
    FLUSH PRIVILEGES;"

echo "Sourcing quarm tables..."
for f in quarm_*.sql; do [ -f "$f" ] && mariadb --database=quarm -e "source $f"; done
echo "Sourcing data_tables..."
for f in data_tables_*.sql; do [ -f "$f" ] && mariadb --database=quarm -e "source $f"; done
echo "Sourcing login_tables..."
for f in login_tables_*.sql; do [ -f "$f" ] && mariadb --database=quarm -e "source $f"; done
echo "Sourcing player_tables..."
for f in player_tables_*.sql; do [ -f "$f" ] && mariadb --database=quarm -e "source $f"; done

# Loginserver tables - required for client login
echo "Sourcing loginserver tables..."
[ -f /src/loginserver/login_util/tblloginserversettings.sql ] && \
    mariadb --database=quarm -e "source /src/loginserver/login_util/tblloginserversettings.sql"
[ -f /src/loginserver/login_util/updates/2023_07_27_tblLoginServerAccounts.sql ] && \
    mariadb --database=quarm -e "source /src/loginserver/login_util/updates/2023_07_27_tblLoginServerAccounts.sql"

# Local dev scripts if they exist
if [ -d /src/.devcontainer/base/db/local ]; then
    echo "Sourcing local development scripts"
    cd /src/.devcontainer/base/db/local
    for f in *.sql; do [ -f "$f" ] && mariadb --database=quarm -e "source $f"; done
fi

echo "Applying post-import DB fixes"
# Fix: World would never boot a zone for a player without this
mariadb --database=quarm -e "UPDATE rule_values SET rule_value='false' WHERE rule_name='World:DontBootDynamics';"

# Fix: Limit dynamics to a sane count for a personal server.
# Without this, multiple launchers each try to spawn 250 zones = hundreds of processes.
mariadb --database=quarm -e "UPDATE launcher SET dynamics=10 WHERE name='dynzone1';
UPDATE launcher SET dynamics=0 WHERE name='dynzone2';
UPDATE launcher SET dynamics=0 WHERE name='zone1';
UPDATE launcher SET dynamics=0 WHERE name='zone2';
UPDATE launcher SET dynamics=0 WHERE name='zone3';"

# Fix: account_inventory table is missing from the DB dump — create from source schema
mariadb --database=quarm -e "source /src/utils/sql/git/required/2025_3_23_created_account_inventory_table.sql"

# Fix: player_event_logs table missing from DB dump
mariadb --database=quarm -e "source /src/utils/sql/git/required/2024_12_23_player_events_tables.sql"

# Fix: Allow GMs to attack, cast, drop items, and use tradeskill containers like normal players
mariadb --database=quarm -e "UPDATE rule_values SET rule_value='false' WHERE rule_name='Quarm:EnableAdminChecks';"

# Limit connections per IP to 6 (default -1 = unlimited)
mariadb --database=quarm -e "UPDATE rule_values SET rule_value='6' WHERE rule_name='World:MaxClientsPerIP';"

# Limit concurrent sessions per account to 6 (default -1 = unlimited)
mariadb --database=quarm -e "UPDATE rule_values SET rule_value='6' WHERE rule_name='World:AccountSessionLimit';"

echo "Preparing environment"
cd /
mkdir -p src/build/bin/logs
mkdir -p src/build/bin/shared

echo "Symlinking Maps and quests"
ln -s /Maps src/build/bin/Maps
ln -s /quests src/build/bin/quests

echo "Copying static opcodes"
cp -Rup /src/loginserver/login_util/*.conf src/build/bin
cp -Rup /src/utils/patches/*.conf src/build/bin

echo "Copying config files"
cp -Rup /src/.devcontainer/base/eqemu_config.json src/build/bin
# Fix zone binary path (devcontainer points to a different location than our build output)
sed -i 's#/src/.devcontainer/bin/zone#/src/build/bin/zone#g' /src/build/bin/eqemu_config.json
# Patch credentials from devcontainer defaults (peq/peqpass/peq) to quarm/quarm/quarm
sed -i 's/"username": "peq"/"username": "quarm"/g' /src/build/bin/eqemu_config.json
sed -i 's/"password": "peqpass"/"password": "quarm"/g' /src/build/bin/eqemu_config.json
sed -i 's/"db": "peq"/"db": "quarm"/g' /src/build/bin/eqemu_config.json
cp -Rup /src/.devcontainer/base/login.json src/build/bin
# Patch login.json credentials to match
sed -i 's/"db": "peq"/"db": "quarm"/g' /src/build/bin/login.json
sed -i 's/"user": "peq"/"user": "quarm"/g' /src/build/bin/login.json
sed -i 's/"password": "peqpass"/"password": "quarm"/g' /src/build/bin/login.json

echo "Database initialization complete."
