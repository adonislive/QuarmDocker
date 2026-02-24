#!/bin/bash
cd "$(dirname "$0")/../.."
echo "Starting Quarm server..."
docker compose up -d
echo "Done."
