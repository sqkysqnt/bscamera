import logging
import sqlite3
from datetime import datetime
from pythonosc import udp_client
import socket
import netifaces
import os

from flask import render_template  # Import render_template from Flask

# No imports from app.py to avoid circular dependencies

# Variables to be initialized
socketio = None
dispatcher = None
OSC_PORT = None
SENDER_NAME = "CameraServer"
DEFAULT_CHANNEL = "cameras"
SCENES_FILE = 'scenes.json'
BROADCAST_IP = None
osc_client = None

def init_theatrechat(app, sockio, disp, osc_port):
    global socketio, dispatcher, OSC_PORT, BROADCAST_IP, osc_client

    socketio = sockio
    dispatcher = disp
    OSC_PORT = osc_port

    BROADCAST_IP = get_broadcast_ip()
    print(f"BROADCAST_IP = {BROADCAST_IP}")

    # Initialize OSC client
    osc_client = udp_client.SimpleUDPClient(BROADCAST_IP, OSC_PORT)
    osc_client._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    # Initialize the database
    init_db()

    # Register the TheatreChat handler with the dispatcher
    dispatcher.map("/theatrechat/message/*", osc_theatrechat_message_handler)

    # Register socketio event handlers
    register_socketio_handlers()

def register_socketio_handlers():
    @socketio.on('send_message')
    def handle_send_message_event(data):
        message = data.get('message')
        channel = DEFAULT_CHANNEL  # Use your default or specified channel
        sender = SENDER_NAME       # Use the server's configured sender name

        if not message:
            return  # Or you can emit an error back to the client

        # Send the OSC message
        send_osc_message_chat(message, channel, sender)

        # Get the current timestamp
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        # Insert into the database as "me" and enforce message limit
        conn = sqlite3.connect('messages.db')
        cursor = conn.cursor()
        cursor.execute('''
            INSERT INTO messages (timestamp, address, sender_name, message, channel, me)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (timestamp, f'/theatrechat/message/{channel}', sender, message, channel, True))
        conn.commit()

        # Enforce the 100-message limit
        enforce_message_limit(cursor)

        conn.commit()
        conn.close()

        # Broadcast the new message to all connected clients
        socketio.emit('new_message', {
            'timestamp': timestamp,
            'sender_name': 'Me',
            'message': message,
            'channel': channel,
            'me': True
        })


def get_broadcast_ip():
    # Find the primary network interface
    interfaces = netifaces.interfaces()
    for interface in interfaces:
        addrs = netifaces.ifaddresses(interface)
        if netifaces.AF_INET in addrs:
            for link in addrs[netifaces.AF_INET]:
                if 'broadcast' in link:
                    return link['broadcast']
    return None

def init_db():
    conn = sqlite3.connect('messages.db')
    cursor = conn.cursor()

    # Check if the 'messages' table exists
    cursor.execute("""
        SELECT name FROM sqlite_master WHERE type='table' AND name='messages';
    """)
    table_exists = cursor.fetchone()

    if not table_exists:
        # Create the new 'messages' table if it doesn't exist
        cursor.execute('''
            CREATE TABLE messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp TEXT NOT NULL,
                address TEXT NOT NULL,
                sender_name TEXT NOT NULL,
                message TEXT NOT NULL,
                channel TEXT NOT NULL,
                me BOOLEAN NOT NULL
            )
        ''')
    else:
        # Check if the new schema already exists
        cursor.execute("PRAGMA table_info(messages);")
        columns = [column[1] for column in cursor.fetchall()]

        if "channel" not in columns or "me" not in columns:
            # Rename old table
            cursor.execute("ALTER TABLE messages RENAME TO messages_old;")

            # Create new table with additional columns
            cursor.execute('''
                CREATE TABLE messages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp TEXT NOT NULL,
                    address TEXT NOT NULL,
                    sender_name TEXT NOT NULL,
                    message TEXT NOT NULL,
                    channel TEXT NOT NULL,
                    me BOOLEAN NOT NULL
                )
            ''')

            # Copy data from old table to new table with default values for new columns
            cursor.execute('''
                INSERT INTO messages (id, timestamp, address, sender_name, message, channel, me)
                SELECT id, timestamp, address, sender_name, message, 'default_channel', 0 FROM messages_old
            ''')

            # Drop the old table
            cursor.execute("DROP TABLE messages_old;")

    conn.commit()
    conn.close()

def osc_theatrechat_message_handler(address, sender_name, message_text):
    logging.info(f"Received TheatreChat message: Address: {address}, Sender: {sender_name}, Message: {message_text}")

    # Get the current timestamp
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # Assume the channel is extracted from the address for now
    channel = address.split("/")[-1]  # E.g., "cameras"
    is_me = sender_name == SENDER_NAME  # Mark 'me' based on sender name

    # Insert the message into the database and enforce message limit
    conn = sqlite3.connect('messages.db')
    cursor = conn.cursor()
    cursor.execute('''
        INSERT INTO messages (timestamp, address, sender_name, message, channel, me)
        VALUES (?, ?, ?, ?, ?, ?)
    ''', (timestamp, address, sender_name, message_text, channel, is_me))
    conn.commit()

    # Enforce the 100-message limit
    enforce_message_limit(cursor)

    conn.commit()
    conn.close()

    # Broadcast the new message
    socketio.emit('new_message', {
        'timestamp': timestamp,
        'sender_name': sender_name,
        'message': message_text,
        'channel': channel,
        'me': is_me
    })

def enforce_message_limit(cursor):
    # Delete oldest messages while keeping only the most recent 100
    cursor.execute('''
        DELETE FROM messages WHERE id NOT IN (
            SELECT id FROM messages ORDER BY id DESC LIMIT 100
        )
    ''')


def send_osc_message(message, channel=DEFAULT_CHANNEL, sender=SENDER_NAME):
    try:
        # Send OSC message using global channel and sender
        osc_client.send_message(f"/theatrechat/message/{channel}", [sender, message])

        logging.info(f"Sending OSC message to {BROADCAST_IP}:{OSC_PORT}")
        logging.info(f"OSC Address: /theatrechat/message/{channel}")
        logging.info(f"Arguments: Sender: {sender}, Message: {message}")

    except Exception as e:
        logging.error(f"Failed to send OSC message: {e}")

def send_osc_message_chat(message, channel, sender):
    try:
        # Construct the OSC address according to the TheatreChat protocol
        osc_address = f"/theatrechat/message/{channel}"

        # Send the OSC message using the constructed address
        osc_client.send_message(osc_address, [sender, message])

        logging.info(f"Sending OSC message to {BROADCAST_IP}:{OSC_PORT}")
        logging.info(f"OSC Address: {osc_address}")
        logging.info(f"Arguments: Sender: {sender}, Message: {message}")

    except Exception as e:
        logging.error(f"Failed to send OSC message: {e}")

def messages_page():
    conn = sqlite3.connect('messages.db')
    cursor = conn.cursor()
    cursor.execute('''
        SELECT timestamp, sender_name, message, channel, me 
        FROM messages 
        ORDER BY id DESC 
        LIMIT 100
    ''')
    messages = cursor.fetchall()
    conn.close()

    # Reverse the order to display messages from oldest to newest
    messages.reverse()
    return render_template('messages.html', messages=messages)

