#!/bin/bash
echo "Starting EQMacEmu..."

# Start database
service mariadb start
sleep 3

# Patch server address from environment variable into config
# Default is 127.0.0.1 (local only). Set SERVER_ADDRESS in docker-compose.yml for LAN/internet.
sed -i "s/\"address\": \"127.0.0.1\"/\"address\": \"${SERVER_ADDRESS}\"/g" /src/build/bin/eqemu_config.json
sed -i "s/\"localaddress\": \"127.0.0.1\"/\"localaddress\": \"${SERVER_ADDRESS}\"/g" /src/build/bin/eqemu_config.json
sed -i "s/\"local_network\": \"127.0.0.1\"/\"local_network\": \"${SERVER_ADDRESS}\"/g" /src/build/bin/login.json
sed -i "s/\"network_ip\": \"127.0.0.1\"/\"network_ip\": \"${SERVER_ADDRESS}\"/g" /src/build/bin/login.json

cd /src/build/bin

# Start shared_memory
./shared_memory &> shared_memory.log
sleep 3

# Start background processes
./loginserver &
./world &
./eqlaunch 'dynzone1' &
./eqlaunch 'boats' &
./queryserv &
./ucs &

echo "Server is ready..."

exec tail -f /dev/null
