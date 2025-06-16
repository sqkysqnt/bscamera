#!/bin/bash

# ======================
# Gather all user input
# ======================

clear
echo "Welcome to the Theatre OSC Messenging (T.O.M.) installation script."
echo
if [[ $EUID -ne 0 ]]; then
  echo "This script must be run as root. Please use sudo."
  exit 1
fi
echo "Please answer the following setup questions (press Enter for defaults):"
echo

read -p "1) Would you like to install BSCam? (y/n) [y]: " INSTALL_CHOICE
INSTALL_CHOICE=${INSTALL_CHOICE:-y}

read -p "2) Enable the BSCam service on startup? (y/n) [y]: " ENABLE_CHOICE
ENABLE_CHOICE=${ENABLE_CHOICE:-y}

read -p "3) Enable NGINX Dynamic IP/Hostname service on startup? (y/n) [n]: " NGINX_CHOICE
NGINX_CHOICE=${NGINX_CHOICE:-n}

read -p "4) Use TLS/HTTPS via Certbot? (y/n) [n]: " TLS_CHOICE
TLS_CHOICE=${TLS_CHOICE:-n}

DOMAIN_NAME=""
if [[ "$TLS_CHOICE" =~ ^[Yy]$ ]]; then
  read -p "   Domain name for TLS (e.g. example.com) [none]: " DOMAIN_NAME
fi

DOMAIN_NAME="$(echo "$DOMAIN_NAME" | xargs)"  # Trim whitespace

read -p "5) Share the recordings folder over the network via SMB? (y/n) [n]: " SMB_CHOICE
SMB_CHOICE=${SMB_CHOICE:-n}

read -p "6) Start bscamera after installation? (y/n) [y]: " START_CHOICE
START_CHOICE=${START_CHOICE:-y}

read -p "7) Install Pi-hole ad blocker? (y/n) [n]: " PIHOLE_CHOICE
PIHOLE_CHOICE=${PIHOLE_CHOICE:-n}

read -p "8) Default username for BSCam access [admin]: " AUTH_USERNAME
AUTH_USERNAME=${AUTH_USERNAME:-admin}

read -s -p "9) Default password for BSCam access [required]: " AUTH_PASSWORD
echo
if [[ -z "$AUTH_PASSWORD" ]]; then
  echo "Password is required. Exiting."
  exit 1
fi




# Show final answers:
echo
echo "===== SUMMARY OF YOUR CHOICES ====="
echo "Install BSCam:                $INSTALL_CHOICE"
echo "Enable BSCam on startup:      $ENABLE_CHOICE"
echo "Enable dynamic NGINX service: $NGINX_CHOICE"
echo "Use TLS via Certbot:          $TLS_CHOICE"
if [[ -n "$DOMAIN_NAME" ]]; then
  echo "Domain:                       $DOMAIN_NAME"
fi
echo "Share recordings over SMB:    $SMB_CHOICE"
echo "Start BSCam after install:    $START_CHOICE"
echo "Install Pi-hole:              $PIHOLE_CHOICE"
echo "Authentication Username:      $AUTH_USERNAME"
echo "Authentication Password:      [hidden]"
echo "==================================="
echo

# Optional debug line to spot trailing spaces
echo "DEBUG: Domain=[$DOMAIN_NAME]" | cat -A

# Optional prompt to confirm choices:
read -p "Continue with these settings? (y/n) [y]: " FINAL_CONFIRM
FINAL_CONFIRM=${FINAL_CONFIRM:-y}

if [[ "$FINAL_CONFIRM" =~ ^[Nn]$ ]]; then
  echo "Installation aborted."
  exit 0
fi

# If user doesn't want to install, abort:
if [[ "$INSTALL_CHOICE" =~ ^[Nn]$ ]]; then
  echo "Installation aborted."
  exit 0
fi

sudo apt update && sudo apt upgrade -y

# ======================
# Variables
# ======================
SERVICE_NAME="bscam"
BSCAM_DIR="$(pwd)"
BSCAM_USER="$(whoami)"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
NGINX_CONF_FILE="/etc/nginx/sites-available/${SERVICE_NAME}"
NGINX_CONF_LINK="/etc/nginx/sites-enabled/${SERVICE_NAME}"
HOSTNAME=$(hostname)
IP_ADDRESS=$(hostname -I | awk '{print $1}')  # First IP address
CONFIG_FILE="$BSCAM_DIR/config.ini"

# ======================
# Step 1: Python venv
# ======================
echo "Creating a Python virtual environment..."
python3 -m venv venv || { echo "Failed to create virtual environment."; exit 1; }
source venv/bin/activate

echo "Installing Python dependencies..."
pip install --upgrade pip
pip install -r requirements.txt || { echo "Failed to install dependencies."; deactivate; exit 1; }
deactivate

# ======================
# Step 2: Write initial config.ini
# ======================
if [[ ! -f "$CONFIG_FILE" ]]; then
cat <<EOF > "$CONFIG_FILE"
[OSC]
Port = 27900
SenderName = CameraServer
DefaultChannel = cameras

[Recording]
Directory = recordings
EOF
fi

# Add [Authentication] section
if ! grep -q '^\[Authentication\]' "$CONFIG_FILE"; then
  echo "" >> "$CONFIG_FILE"
  echo "[Authentication]" >> "$CONFIG_FILE"
  echo "Username = $AUTH_USERNAME" >> "$CONFIG_FILE"
  echo "Password = $AUTH_PASSWORD" >> "$CONFIG_FILE"
fi


# Ensure [Server] section
if ! grep -q '^\[Server\]' "$CONFIG_FILE"; then
  echo "" >> "$CONFIG_FILE"
  echo "[Server]" >> "$CONFIG_FILE"
fi

# Remove old lines if they exist
sed -i '/^UseTLS\s*=/d' "$CONFIG_FILE"
sed -i '/^Domain\s*=/d' "$CONFIG_FILE"

USE_TLS_LOWER="no"
if [[ "$TLS_CHOICE" =~ ^[Yy]$ ]]; then
  USE_TLS_LOWER="yes"
  if [[ -n "$DOMAIN_NAME" ]]; then
    echo "Domain = $DOMAIN_NAME" >> "$CONFIG_FILE"
  fi
fi
echo "UseTLS = $USE_TLS_LOWER" >> "$CONFIG_FILE"

# ======================
# Step 3: Attempt Certbot (if TLS chosen)
# ======================
CERT_PATH_FULLCHAIN=""
CERT_PATH_PRIVKEY=""
MANUAL_CERT_CHOICE=""

if [[ "$TLS_CHOICE" =~ ^[Yy]$ && -n "$DOMAIN_NAME" ]]; then
  echo "Installing Certbot..."
  sudo apt-get update
  sudo apt-get install -y certbot python3-certbot-nginx || {
    echo "Failed to install Certbot. We will have to handle certs manually."
  }

  echo "Attempting to obtain certificates for $DOMAIN_NAME via Certbot..."
  # We'll only run certbot if it is installed
  if command -v certbot &> /dev/null; then
    # --force-renewal can be added if you want to always request a new cert
    sudo certbot certonly --nginx -d "$DOMAIN_NAME" || {
      echo "Certbot failed to obtain certificate. We'll handle certs manually."
    }
  fi

  # Try the *default* path first
  CERT_PATH_FULLCHAIN="/etc/letsencrypt/live/$DOMAIN_NAME/fullchain.pem"
  CERT_PATH_PRIVKEY="/etc/letsencrypt/live/$DOMAIN_NAME/privkey.pem"

  if [[ -f "$CERT_PATH_FULLCHAIN" && -f "$CERT_PATH_PRIVKEY" ]]; then
    echo "Found certs at $CERT_PATH_FULLCHAIN and $CERT_PATH_PRIVKEY."
    echo "We'll use these for Nginx."
  else
    # If the default folder doesn't exist, let's see if there's a folder
    # that starts with domain (like domain-0001)
    POSSIBLE_DIR=$(ls -d /etc/letsencrypt/live/"${DOMAIN_NAME}"* 2>/dev/null | head -n 1)
    if [[ -n "$POSSIBLE_DIR" ]]; then
      # Check for fullchain & privkey
      if [[ -f "$POSSIBLE_DIR/fullchain.pem" && -f "$POSSIBLE_DIR/privkey.pem" ]]; then
        CERT_PATH_FULLCHAIN="$POSSIBLE_DIR/fullchain.pem"
        CERT_PATH_PRIVKEY="$POSSIBLE_DIR/privkey.pem"
        echo "Found certs in $POSSIBLE_DIR. We'll use those for Nginx."
      fi
    fi

    # If still not found, prompt the user for manual path or fallback
    if [[ ! -f "$CERT_PATH_FULLCHAIN" || ! -f "$CERT_PATH_PRIVKEY" ]]; then
      echo
      echo "We could not find certificates in /etc/letsencrypt/live/$DOMAIN_NAME or a similar folder."
      echo "Please choose how to proceed:"
      echo "  1) Enter a custom path to fullchain.pem and privkey.pem"
      echo "  2) Generate local self-signed cert (Push Notifications will NOT work)"
      echo "  3) Disable TLS (switch to HTTP only)"
      read -p "Enter 1/2/3 [3]: " MANUAL_CERT_CHOICE
      MANUAL_CERT_CHOICE=${MANUAL_CERT_CHOICE:-3}

      case "$MANUAL_CERT_CHOICE" in
        "1")
          echo
          read -p "Path to fullchain (combined certificate): " CERT_PATH_FULLCHAIN
          read -p "Path to private key (privkey): " CERT_PATH_PRIVKEY
          if [[ ! -f "$CERT_PATH_FULLCHAIN" || ! -f "$CERT_PATH_PRIVKEY" ]]; then
            echo "The paths you entered do not exist. Switching to HTTP only."
            USE_TLS_LOWER="no"
          fi
          ;;
        "2")
          echo
          echo "Generating a self-signed certificate in $BSCAM_DIR/certs."
          mkdir -p "$BSCAM_DIR/certs"
          openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
            -keyout "$BSCAM_DIR/certs/key.pem" \
            -out "$BSCAM_DIR/certs/cert.pem" \
            -subj "/C=US/ST=State/L=City/O=Organization/OU=OrgUnit/CN=$DOMAIN_NAME"
          CERT_PATH_FULLCHAIN="$BSCAM_DIR/certs/cert.pem"
          CERT_PATH_PRIVKEY="$BSCAM_DIR/certs/key.pem"
          echo "Self-signed cert created. (Push notifications won't work with self-signed)."
          ;;
        "3"|"")
          echo "Disabling TLS."
          USE_TLS_LOWER="no"
          ;;
        *)
          echo "Unrecognized choice. Defaulting to HTTP only."
          USE_TLS_LOWER="no"
          ;;
      esac
    fi
  fi
fi

# ======================
# Are we doing the Samba thing?
# ======================

if [[ "$SMB_CHOICE" =~ ^[Yy]$ ]]; then
  echo "Installing Samba..."
  sudo apt-get install -y samba samba-common-bin || { echo "Failed to install Samba."; exit 1; }

  SMB_CONF="/etc/samba/smb.conf"
  RECORDING_DIR="$BSCAM_DIR/recordings"

  echo "Creating Samba share configuration..."
  sudo bash -c "cat <<EOF >> $SMB_CONF

[Recordings]
   path = $RECORDING_DIR
   browseable = yes
   read only = no
   guest ok = yes
   create mask = 0666
   directory mask = 0777
EOF"

  echo "Restarting Samba service..."
  sudo systemctl restart smbd || { echo "Failed to restart Samba."; exit 1; }

  echo "Samba setup complete. You can access the recordings folder at:"
  echo "   \\\\$(hostname -I | awk '{print $1}')\\Recordings"
fi

# ======================
# Adjust Folder Permissions for Samba
# ======================

if [[ "$SMB_CHOICE" =~ ^[Yy]$ ]]; then
  echo "Ensuring proper permissions for Samba share..."

  RECORDING_DIR="$BSCAM_DIR/recordings"

  # Ensure the recordings directory exists
  mkdir -p "$RECORDING_DIR"

  # Determine the absolute path to recordings
  ABS_RECORDING_DIR=$(realpath "$RECORDING_DIR")

  # Get the user's home directory dynamically
  USER_HOME=$(eval echo ~$BSCAM_USER)

  # Adjust permissions on the home directory (if necessary)
  sudo chmod 755 "$USER_HOME"

  # Adjust permissions for all parent directories leading to the recordings folder
  PARENT_DIR="$ABS_RECORDING_DIR"
  while [ "$PARENT_DIR" != "/" ]; do
    sudo chmod 755 "$PARENT_DIR"
    PARENT_DIR=$(dirname "$PARENT_DIR")
  done

  # Ensure the recordings directory is fully accessible
  sudo chmod 777 "$ABS_RECORDING_DIR"
  sudo chown -R nobody:nogroup "$ABS_RECORDING_DIR"

  echo "Folder permissions set successfully!"

  # Restart Samba to apply changes
  sudo systemctl restart smbd
  sudo systemctl restart nmbd

  echo "Samba has been restarted. Your share should now be accessible."
fi



# ======================
# Step 4: Install/Configure Nginx
# ======================
echo
echo "Setting up Nginx..."
if ! command -v nginx &> /dev/null; then
  echo "Nginx not found. Installing..."
  sudo apt-get update && sudo apt-get install -y nginx || {
    echo "Failed to install Nginx."; exit 1;
  }
fi

# Backup existing config if present
if [[ -f "$NGINX_CONF_FILE" ]]; then
  echo "Backing up existing Nginx config..."
  sudo mv "$NGINX_CONF_FILE" "${NGINX_CONF_FILE}.bak"
fi

sudo mkdir -p /etc/nginx/sites-available /etc/nginx/sites-enabled

if [[ "$USE_TLS_LOWER" == "yes" ]]; then
  # Ensure we have valid paths
  if [[ ! -f "$CERT_PATH_FULLCHAIN" || ! -f "$CERT_PATH_PRIVKEY" ]]; then
    # If we got here, user couldn't provide valid certs, so fallback to HTTP
    echo "Falling back to HTTP config due to missing cert files."
    USE_TLS_LOWER="no"
  fi
fi

if [[ "$USE_TLS_LOWER" == "yes" ]]; then
  # HTTPS config
  # If no domain was specified, fallback to $HOSTNAME
  DOMAIN_OR_HOST="$DOMAIN_NAME"
  [[ -z "$DOMAIN_OR_HOST" ]] && DOMAIN_OR_HOST="$HOSTNAME"

  cat <<EOF | sudo tee "$NGINX_CONF_FILE"
server {
    listen 80;
    server_name $DOMAIN_OR_HOST $HOSTNAME $IP_ADDRESS;
    return 301 https://\$host\$request_uri;
}

server {
    listen 443 ssl http2;
    server_name $DOMAIN_OR_HOST $HOSTNAME $IP_ADDRESS;

    ssl_certificate $CERT_PATH_FULLCHAIN;
    ssl_certificate_key $CERT_PATH_PRIVKEY;

    # WebSocket-specific location for Socket.IO
    location /socket.io/ {
        proxy_pass http://127.0.0.1:15000/socket.io/;

        # WebSocket headers
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection "Upgrade";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;

        # Disable proxy buffering for WebSocket
        proxy_buffering off;
        proxy_cache_bypass \$http_upgrade;
    }

    # General Flask application
    location / {
        proxy_pass http://127.0.0.1:15000;

        # Common headers
        proxy_http_version 1.1;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }
}
EOF

else
  # HTTP-only config
  cat <<EOF | sudo tee "$NGINX_CONF_FILE"
server {
    listen 80;
    server_name $HOSTNAME $IP_ADDRESS;

    # Recommended for websockets:
    proxy_http_version 1.1;
    proxy_set_header Upgrade \$http_upgrade;
    proxy_set_header Connection "upgrade";

    location / {
        proxy_pass http://127.0.0.1:15000;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }
}
EOF
fi

if [[ ! -L "$NGINX_CONF_LINK" ]]; then
  echo "Creating symlink in /etc/nginx/sites-enabled..."
  sudo ln -s "$NGINX_CONF_FILE" "$NGINX_CONF_LINK"
fi

echo "Testing Nginx config..."
sudo nginx -t || { echo "Nginx config test failed."; exit 1; }

echo "Restarting Nginx..."
sudo systemctl restart nginx || { echo "Failed to restart Nginx."; exit 1; }
echo "Nginx setup complete."

# Dynamic Nginx service if chosen
if [[ "$NGINX_CHOICE" =~ ^[Yy]$ ]]; then
  echo "Setting up the update-nginx service..."
  DYNAMIC_NGINX_SCRIPT="/usr/local/bin/generate_nginx_config.sh"
  cat <<'EOF' | sudo tee "$DYNAMIC_NGINX_SCRIPT"
#!/bin/bash

CONFIG_FILE="/etc/nginx/sites-available/bscam"
HOSTNAME=$(hostname)
IP_ADDRESS=$(hostname -I | awk '{print $1}')
BSCAM_DIR="$1"

# Potentially re-validate config and reload:
sudo nginx -t && sudo systemctl reload nginx
EOF
  sudo chmod +x "$DYNAMIC_NGINX_SCRIPT"

  UPDATE_NGINX_SERVICE="/etc/systemd/system/update-nginx.service"
  cat <<EOF | sudo tee "$UPDATE_NGINX_SERVICE"
[Unit]
Description=Update Nginx configuration with dynamic hostname and IP
After=network-online.target

[Service]
ExecStart=$DYNAMIC_NGINX_SCRIPT $BSCAM_DIR
Type=oneshot

[Install]
WantedBy=multi-user.target
EOF

  sudo systemctl daemon-reload
  sudo systemctl start update-nginx.service || { echo "Failed to start update-nginx service."; exit 1; }
  if [[ "$ENABLE_CHOICE" =~ ^[Yy]$ ]]; then
    sudo systemctl enable update-nginx.service
    echo "update-nginx service enabled on startup."
  fi
  echo "update-nginx service has been set up and executed."
fi


# ======================
# Step 4.5: Download socket.io.js
# ======================
STATIC_DIR="$BSCAM_DIR/static"
SOCKET_IO_JS="$STATIC_DIR/socket.io.js"

echo "Downloading socket.io.js..."
mkdir -p "$STATIC_DIR"
curl -o "$SOCKET_IO_JS" https://cdn.jsdelivr.net/npm/socket.io-client/dist/socket.io.js

if [[ -f "$SOCKET_IO_JS" ]]; then
  echo "socket.io.js downloaded successfully to $SOCKET_IO_JS"
else
  echo "Failed to download socket.io.js."
  exit 1
fi

# ======================
# Step 5: BSCam service
# ======================
echo "Setting up the BSCam service..."
cat <<EOF | sudo tee "$SERVICE_FILE"
[Unit]
Description=BSCam Service
After=network.target

[Service]
WorkingDirectory=$BSCAM_DIR
ExecStart=$BSCAM_DIR/venv/bin/python app.py
Restart=always
User=$BSCAM_USER
Group=$BSCAM_USER

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl start "$SERVICE_NAME" || {
  echo "Failed to start BSCam service."; exit 1;
}

if systemctl is-active --quiet "$SERVICE_NAME"; then
  echo "BSCam service started successfully."
  if [[ "$ENABLE_CHOICE" =~ ^[Yy]$ ]]; then
    sudo systemctl enable "$SERVICE_NAME"
    echo "BSCam service enabled on startup."
  fi
else
  echo "BSCam service failed to start. Check logs with: sudo journalctl -u $SERVICE_NAME"
  exit 1
fi

# ======================
# Step 6: VAPID keys
# ======================
echo
echo "Installing web-push globally (for push notifications)..."
sudo apt-get install -y npm jq  # also install jq
sudo npm install -g web-push || { echo "Failed to install web-push."; exit 1; }

echo
echo "Generating VAPID keys (JSON format)..."
WEBPUSH_OUTPUT=$(web-push generate-vapid-keys --json 2>&1)
echo "$WEBPUSH_OUTPUT"

# Use jq to parse
PUBKEY=$(echo "$WEBPUSH_OUTPUT" | jq -r '.publicKey')
PRIVKEY=$(echo "$WEBPUSH_OUTPUT" | jq -r '.privateKey')

if [[ -z "$PUBKEY" || -z "$PRIVKEY" || "$PUBKEY" == "null" || "$PRIVKEY" == "null" ]]; then
  echo "Error parsing VAPID keys from JSON output."
  exit 1
fi

# Ensure [Push] and [Server] sections exist in config.ini
if ! grep -q '^\[Push\]' "$CONFIG_FILE"; then
  echo "" >> "$CONFIG_FILE"
  echo "[Push]" >> "$CONFIG_FILE"
fi

if ! grep -q '^\[Server\]' "$CONFIG_FILE"; then
  echo "" >> "$CONFIG_FILE"
  echo "[Server]" >> "$CONFIG_FILE"
fi

# Remove existing PublicKey and PrivateKey lines to avoid duplicates
sed -i '/^PublicKey\s*=/d' "$CONFIG_FILE"
sed -i '/^PrivateKey\s*=/d' "$CONFIG_FILE"

# Add PublicKey and PrivateKey to [Push] section
sed -i '/^\[Push\]/a PublicKey = '"$PUBKEY"'\nPrivateKey = '"$PRIVKEY"'' "$CONFIG_FILE"

# Add PublicKey and PrivateKey to [Server] section, ensuring it is appended properly
sed -i '/^\[Server\]/a PublicKey = '"$PUBKEY"'\nPrivateKey = '"$PRIVKEY"'' "$CONFIG_FILE"




echo
echo "VAPID keys successfully added to config.ini"

# ======================
# Start program if requested
# ======================

if [[ "$START_CHOICE" =~ ^[Yy]$ ]]; then
  echo "Starting bscamera..."
  sudo systemctl start bscam || { echo "Failed to start BSCam service."; exit 1; }

  # Wait a few seconds to allow the service to start
  sleep 3

  # Check if the service is running
  if systemctl is-active --quiet bscam; then
    echo "BSCam service started successfully!"
    echo ""
    echo "======================================"
    echo "How to Access BSCam:"
    echo "--------------------------------------"
    echo "Web Interface: http://$(hostname -I | awk '{print $1}'):15000"
    echo "Check Service Logs: sudo journalctl -u bscam -f"
    echo "Restart the Service: sudo systemctl restart bscam"
    echo "Stop the Service: sudo systemctl stop bscam"
    echo "======================================"
  else
    echo "BSCam service failed to start. Check logs:"
    echo "   sudo journalctl -u bscam -n 50 --no-pager"
    exit 1
  fi
fi

# ======================
# Optional: Pi-hole Installation
# ======================
if [[ "$PIHOLE_CHOICE" =~ ^[Yy]$ ]]; then
  echo "Installing Pi-hole..."

  # Install dependencies
  sudo apt-get update
  sudo apt-get install -y git curl

  # Clone and run installer
  git clone --depth 1 https://github.com/pi-hole/pi-hole.git Pi-hole || {
    echo "Failed to clone Pi-hole repository."
    exit 1
  }

  cd "Pi-hole/automated install/" || {
    echo "Failed to change directory to Pi-hole installer."
    exit 1
  }

  sudo bash basic-install.sh
  cd "$BSCAM_DIR" || echo "Warning: failed to return to BSCam directory."

  echo "Pi-hole installation step completed."
fi


# ======================
# Done
# ======================
echo
echo "====================================================="
echo "BSCam installation completed successfully!"
echo "TLS used: $([[ "$USE_TLS_LOWER" == "yes" ]] && echo 'ENABLED' || echo 'DISABLED')"
if [[ "$USE_TLS_LOWER" == "yes" ]]; then
  echo "Certificate files:"
  echo "  cert: $CERT_PATH_FULLCHAIN"
  echo "  key : $CERT_PATH_PRIVKEY"
fi
echo "Service: $SERVICE_NAME"
echo "Config file: $CONFIG_FILE"
echo "Nginx config: $NGINX_CONF_FILE"
echo "====================================================="
