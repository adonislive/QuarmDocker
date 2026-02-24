#!/bin/bash
cd "$(dirname "$0")/../.."

docker inspect quarm-server > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "Server is not running. Nothing to stop."
    exit 1
fi

echo "Backing up before shutdown..."
mkdir -p config/backups
datestamp=$(date +%Y-%m-%d_%H%M)
docker exec quarm-server mariadb-dump quarm > config/backups/backup_${datestamp}.sql
filesize=$(stat -f%z config/backups/backup_${datestamp}.sql)
if [ "$filesize" -lt 100 ]; then
    echo "WARNING: Backup file appears empty or failed. Aborting shutdown."
    exit 1
fi
echo "Backup saved as config/backups/backup_${datestamp}.sql (${filesize} bytes)."
echo "Stopping server..."
docker compose down
echo "Done."
