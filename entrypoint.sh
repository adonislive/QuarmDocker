#!/bin/bash
echo "Starting EQMacEmu..."

# Start database
service mariadb start
sleep 3

# Patch server address from environment variable into config
# Default is 127.0.0.1 (local only). Set SERVER_ADDRESS in docker-compose.yml for LAN/internet.
sed -i "s/\"address\": \"127.0.0.1\"/\"address\": \"${SERVER_ADDRESS}\"/g" /src/build/bin/eqemu_config.json
sed -i "s/\${SERVER_ADDRESS}/${SERVER_ADDRESS}/g" /src/build/bin/login.json

cd /src/build/bin

# Start shared_memory
./shared_memory &> shared_memory.log
sleep 3

# Start background processes
./loginserver > /dev/stdout 2> /dev/stderr &
./world > /dev/stdout 2> /dev/stderr &
./eqlaunch 'dynzone1' > /dev/stdout 2> /dev/stderr &
./eqlaunch 'boats' > /dev/stdout 2> /dev/stderr &
./queryserv > /dev/stdout 2> /dev/stderr &
./ucs > /dev/stdout 2> /dev/stderr &

echo "Server is ready..."

exec tail -f /dev/null
