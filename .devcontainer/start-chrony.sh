#!/bin/bash
#
# Startup script for Chrony NTP server in DevContainer
# This script starts chronyd with proper permissions

set -e

echo "Starting Chrony NTP server..."

# Create necessary directories if they don't exist
sudo mkdir -p /var/lib/chrony
sudo mkdir -p /var/log/chrony
sudo mkdir -p /run/chrony

# Set proper permissions
sudo chown -R _chrony:_chrony /var/lib/chrony 2>/dev/null || sudo chown -R chrony:chrony /var/lib/chrony 2>/dev/null || true
sudo chown -R root:root /var/log/chrony

# Start chronyd in the foreground  
sudo chronyd -d -f /etc/chrony/chrony.conf &

# Wait a moment for chronyd to start
sleep 2

# Check if chronyd is running
if pgrep chronyd > /dev/null; then
    echo "✓ Chrony NTP server started successfully"
    echo "  Server is listening on UDP port 123"
    echo "  Use 'chronyc tracking' to check synchronization status"
    echo "  Use 'chronyc sources' to see NTP sources"
else
    echo "✗ Failed to start Chrony NTP server"
    exit 1
fi
