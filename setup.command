#!/bin/bash
cd "$(dirname "$0")"

echo "================================================"
echo " QUARM DOCKER SETUP"
echo "================================================"
echo ""

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="mac"
else
    OS="linux"
fi

# ------------------------------------------------
# STEP 1 - chmod scripts
# ------------------------------------------------
echo "Setting script permissions..."
chmod +x config/$OS/*.sh
echo "Done."
echo ""

# ------------------------------------------------
# STEP 2 - Check Docker is installed
# ------------------------------------------------
echo "Checking Docker..."
if ! command -v docker &> /dev/null; then
    echo ""
    echo "ERROR: Docker is not installed."
    echo ""
    echo "Please download and install Docker Desktop from:"
    echo "  https://docs.docker.com/desktop/install/"
    echo ""
    echo "Then run this setup again."
    echo ""
    exit 1
fi
echo "Docker found."
echo ""

# ------------------------------------------------
# STEP 3 - Check Docker is running
# ------------------------------------------------
echo "Checking Docker is running..."
until docker info > /dev/null 2>&1; do
    echo ""
    echo "Docker is not running. Please open Docker Desktop and wait for it to start."
    echo ""
    read -p "Press Enter to try again..."
    echo ""
done
echo "Docker is running."
echo ""

# ------------------------------------------------
# STEP 4 - Local or LAN play
# ------------------------------------------------
echo "------------------------------------------------"
echo " NETWORK SETUP"
echo "------------------------------------------------"
echo ""
echo "Quarm Docker defaults to local play only."
echo "This means only this machine can connect to the server."
echo ""
echo "If you are not sure, choose local. You can change this later."
echo ""
read -p "Do you want to set up LAN play so other machines can connect? (Y/N): " lanChoice

if [[ "$lanChoice" =~ ^[Yy]$ ]]; then
    echo ""
    echo "Detecting LAN IP address..."

    if [[ "$OS" == "mac" ]]; then
        lanip=$(ipconfig getifaddr en0 2>/dev/null)
        if [ -z "$lanip" ]; then
            lanip=$(ipconfig getifaddr en1 2>/dev/null)
        fi
    else
        lanip=$(hostname -I | tr ' ' '\n' | grep -E '^192\.168\.(0|1)\.' | head -1)
    fi

    if [ -z "$lanip" ]; then
        echo ""
        echo "Could not automatically detect a LAN IP address."
        echo ""
        read -p "Please enter your LAN IP address manually (e.g. 192.168.1.100): " lanip
    else
        echo ""
        echo "Detected LAN IP: $lanip"
        echo ""
        read -p "Is this correct? (Y/N): " ipOk
        if [[ ! "$ipOk" =~ ^[Yy]$ ]]; then
            echo ""
            read -p "Please enter your LAN IP address manually (e.g. 192.168.1.100): " lanip
        fi
    fi

    echo ""
    echo "Updating docker-compose.yml with IP $lanip..."
    sed -i.bak "s/SERVER_ADDRESS=127\.0\.0\.1/SERVER_ADDRESS=$lanip/" docker-compose.yml
    rm -f docker-compose.yml.bak
    echo "Done."
    echo ""
    eqip=$lanip
else
    eqip="127.0.0.1"
fi

# ------------------------------------------------
# STEP 5 - Build
# ------------------------------------------------
echo "------------------------------------------------"
echo " BUILD"
echo "------------------------------------------------"
echo ""
echo "The server needs to be built from source."
echo "This will take 30-45 minutes. Do not close this window."
echo ""
read -p "Ready to build now? (Y/N): " buildChoice

if [[ ! "$buildChoice" =~ ^[Yy]$ ]]; then
    echo ""
    echo "When you are ready, run this command from the QuarmDocker folder:"
    echo "  docker compose build"
    echo ""
else
    echo ""
    echo "Starting build. Please wait..."
    echo ""
    docker compose build
    if [ $? -ne 0 ]; then
        echo ""
        echo "ERROR: Build failed. Check the output above for details."
        echo ""
        exit 1
    fi
    echo ""
    echo "Build complete."
    echo ""

    # ------------------------------------------------
    # STEP 6 - Start server
    # ------------------------------------------------
    echo "------------------------------------------------"
    echo " START SERVER"
    echo "------------------------------------------------"
    echo ""
    read -p "Start the server now? (Y/N): " startChoice

    if [[ ! "$startChoice" =~ ^[Yy]$ ]]; then
        echo ""
        echo "When you are ready, run: config/$OS/start.sh"
        echo ""
    else
        echo ""
        docker compose up -d
        if [ $? -ne 0 ]; then
            echo ""
            echo "ERROR: Failed to start the server. Check the output above for details."
            echo ""
            exit 1
        fi
        echo ""
        echo "Server is running."
        echo ""
    fi
fi

# ------------------------------------------------
# STEP 7 - Post install checklist
# ------------------------------------------------
echo "================================================"
echo " SETUP COMPLETE — NEXT STEPS"
echo "================================================"
echo ""
echo "1. Configure your EQ client"
echo "   Edit eqhost.txt in your TAKP client folder to contain:"
echo ""
echo "   [Registration Servers]"
echo "   {"
echo "   \"$eqip:6000\""
echo "   }"
echo "   [Login Servers]"
echo "   {"
echo "   \"$eqip:6000\""
echo "   }"
echo ""
echo "2. Launch the client and log in"
echo "   Use any username and password."
echo "   Your account is created automatically on first login."
echo ""
echo "3. Make yourself a GM"
echo "   Run this command, replacing YOURNAME with your account name:"
echo ""
echo "   docker exec quarm-server mariadb -e \"UPDATE account SET status=255 WHERE name='YOURNAME';\" quarm"
echo ""
echo "4. For future server management, use the scripts in the config/$OS/ folder:"
echo "     start.sh  : Start the server"
echo "     stop.sh   : Stop the server and back up the database"
echo "     backup.sh : Back up the database while the server is running"
echo "     restore.sh: Restore the database from a previous backup"
echo ""
echo "================================================"
echo ""
