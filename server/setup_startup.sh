#!/bin/bash

SERVICE_NAME="camera_server"
PYTHON_SCRIPT_PATH="app.py"
SERVICE_FILE_PATH="/etc/systemd/system/${SERVICE_NAME}.service"

# Check if the Python script exists
if [[ ! -f "$PYTHON_SCRIPT_PATH" ]]; then
    echo "Python script not found at $PYTHON_SCRIPT_PATH"
    exit 1
fi

# Create the systemd service file
echo "Creating service file at $SERVICE_FILE_PATH"
sudo tee "$SERVICE_FILE_PATH" > /dev/null <<EOL
[Unit]
Description=Camera Server Service
After=network.target

[Service]
ExecStart=/usr/bin/python3 $PYTHON_SCRIPT_PATH
WorkingDirectory=$(dirname "$PYTHON_SCRIPT_PATH")
Restart=always
User=$(whoami)
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=multi-user.target
EOL

# Reload systemd to recognize the new service
echo "Reloading systemd daemon..."
sudo systemctl daemon-reload

# Enable the service to start on boot
echo "Enabling the service to start on boot..."
sudo systemctl enable "$SERVICE_NAME"

# Start the service immediately
echo "Starting the service..."
sudo systemctl start "$SERVICE_NAME"

# Check the status of the service
echo "Checking service status..."
sudo systemctl status "$SERVICE_NAME"
