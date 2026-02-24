#!/bin/bash
cd "$(dirname "$0")/../.."

docker inspect quarm-server > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "Server is not running. Please start the server before restoring."
    exit 1
fi

echo "WARNING: This will overwrite all current characters and accounts."
echo ""
read -p "Are you sure you want to restore? (Y/N): " confirm
if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
    echo "Restore cancelled."
    exit 0
fi

newest=$(ls -t config/backups/backup_*.sql 2>/dev/null | head -1)
if [ -z "$newest" ]; then
    echo "No backups found."
    exit 1
fi

echo ""
echo "Most recent backup: $newest"
echo ""
read -p "Restore this backup? (Y/N): " choice
if [[ "$choice" =~ ^[Yy]$ ]]; then
    docker exec -i quarm-server mariadb quarm < "$newest"
    echo "Done. Restored from $newest"
    exit 0
fi

echo ""
echo "Available backups:"
echo ""
ls -t config/backups/backup_*.sql
echo ""
read -p "Enter filename to restore (e.g. backup_2026-02-23_1430.sql): " filename
if [ ! -f "config/backups/$filename" ]; then
    echo "ERROR: File not found. Please check the filename and try again."
    exit 1
fi
docker exec -i quarm-server mariadb quarm < "config/backups/$filename"
echo "Done. Restored from config/backups/$filename"
