#!/bin/bash

SERVICE_NAME="camera_server"
SERVICE_FILE_PATH="/etc/systemd/system/${SERVICE_NAME}.service"

# Stop the service
echo "Stopping the service..."
sudo systemctl stop "$SERVICE_NAME"

# Disable the service to prevent it from starting on boot
echo "Disabling the service..."
sudo systemctl disable "$SERVICE_NAME"

# Remove the service file
if [[ -f "$SERVICE_FILE_PATH" ]]; then
    echo "Removing the service file at $SERVICE_FILE_PATH"
    sudo rm -f "$SERVICE_FILE_PATH"
else
    echo "Service file not found at $SERVICE_FILE_PATH"
fi

# Reload systemd to apply changes
echo "Reloading systemd daemon..."
sudo systemctl daemon-reload

# Verify the service has been removed
echo "Checking for the existence of the service..."
if systemctl list-units --all | grep -q "$SERVICE_NAME.service"; then
    echo "Service $SERVICE_NAME still exists. Manual intervention might be required."
else
    echo "Service $SERVICE_NAME successfully removed."
fi
