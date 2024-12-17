import logging
import sqlite3
from datetime import datetime
from pythonosc import udp_client
import socket
import netifaces
import os
import threading
import uuid
from flask import render_template

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

# Global variables for pending commands
pending_commands = {}
pending_commands_lock = threading.Lock()
numCamList = 3  # Adjust this value as needed


def init_theatrechat(app, sockio, disp, osc_port, num_cameras_func):
    global socketio, dispatcher, OSC_PORT, BROADCAST_IP, osc_client, get_num_cameras

    socketio = sockio
    dispatcher = disp
    OSC_PORT = osc_port
    get_num_cameras = num_cameras_func  # Assign the function

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
        channel = data.get('channel', DEFAULT_CHANNEL)
        # Get the username from the client data or fallback if not provided
        sender = data.get('username', SENDER_NAME)

        if not message:
            return

        if channel == "cameras":
            # Cameras channel requires target and command
            parts = message.split(' ', 1)
            if len(parts) < 2:
                return
            target, command = parts

            # Number of cameras for reply logic
            if get_num_cameras is not None:
                numCamList = get_num_cameras()
            else:
                numCamList = 0

            # Send OSC
            send_osc_message_chat(message, channel, sender)

            # Create pending command if needed
            command_id = str(uuid.uuid4())
            with pending_commands_lock:
                pending_commands[command_id] = {
                    'command': command,
                    'target': target,
                    'sent_time': datetime.now(),
                    'replies_received': set(),
                    'num_expected_replies': numCamList if target == 'all' else 1,
                    'resend_attempts': 0,
                    'message': message,
                    'channel': channel,
                    'sender': sender
                }

            timer = threading.Timer(1.0, check_for_replies, args=(command_id,))
            timer.start()

        else:
            # Non-cameras channels: just send once
            send_osc_message_chat(message, channel, sender)

        # Insert into DB
        timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        conn = sqlite3.connect('messages.db')
        cursor = conn.cursor()
        cursor.execute('''
            INSERT INTO messages (timestamp, address, sender_name, message, channel, me)
            VALUES (?, ?, ?, ?, ?, ?)
        ''', (timestamp, f'/theatrechat/message/{channel}', sender, message, channel, False))
        conn.commit()

        new_message_id = cursor.lastrowid  # Get the newly inserted message ID
        conn.close()

        # Do not call enforce_message_limit since we want to keep all messages
        # socketio emit including the new_message_id
        socketio.emit('new_message', {
            'id': new_message_id,
            'timestamp': timestamp,
            'sender_name': sender,
            'message': message,
            'channel': channel
        })





def check_for_replies(command_id):
    with pending_commands_lock:
        pending_command = pending_commands.get(command_id)
        if not pending_command:
            return  # Command might have been completed or removed

        num_replies = len(pending_command['replies_received'])
        num_expected = pending_command['num_expected_replies']

        if num_replies >= num_expected:
            # Received all expected replies, remove from pending
            del pending_commands[command_id]
            logging.info(f"Received all expected replies for command {pending_command['command']}")
        else:
            # Not all replies received, resend the message
            if pending_command['resend_attempts'] < 5:  # Limit the number of resends
                pending_command['resend_attempts'] += 1
                logging.info(f"Resending message for command {pending_command['command']}, attempt {pending_command['resend_attempts']}")

                # Resend the message
                send_osc_message_chat(pending_command['message'], pending_command['channel'], pending_command['sender'])

                # Restart the timer
                timer = threading.Timer(1.0, check_for_replies, args=(command_id,))
                timer.start()
            else:
                # Max resend attempts reached, give up
                del pending_commands[command_id]
                logging.warning(f"Max resend attempts reached for command {pending_command['command']}")


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


def osc_theatrechat_message_handler(address, sender_name, message_text, *extra_args):
    logging.info(f"Received TheatreChat message: Address: {address}, Sender: {sender_name}, Message: {message_text}")

    if "local_origin" in extra_args:
        # This message came from our own server
        return

    # Prevent processing of messages that the server itself sent
    if sender_name == SENDER_NAME:
        return

    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    channel = address.split("/")[-1]
    is_me = (sender_name == SENDER_NAME)

    # Check pending commands for replies if needed
    with pending_commands_lock:
        pending_commands_items = list(pending_commands.items())
    for command_id, pending_command in pending_commands_items:
        if pending_command['command'] in message_text:
            with pending_commands_lock:
                pending_command['replies_received'].add(sender_name)
                logging.info(f"Received reply from {sender_name} for command {pending_command['command']}")
                if len(pending_command['replies_received']) >= pending_command['num_expected_replies']:
                    del pending_commands[command_id]
                    logging.info(f"Received all expected replies for command {pending_command['command']}")
            break



    conn = sqlite3.connect('messages.db')
    cursor = conn.cursor()
    cursor.execute('''
        INSERT INTO messages (timestamp, address, sender_name, message, channel, me)
        VALUES (?, ?, ?, ?, ?, ?)
    ''', (timestamp, address, sender_name, message_text, channel, is_me))
    conn.commit()

    new_message_id = cursor.lastrowid
    conn.close()

    # Emit the new message to clients
    socketio.emit('new_message', {
        'id': new_message_id,
        'timestamp': timestamp,
        'sender_name': sender_name,
        'message': message_text,
        'channel': channel
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
        osc_client.send_message(osc_address, [sender, message, "local_origin"])

        logging.info(f"Sending OSC message to {BROADCAST_IP}:{OSC_PORT}")
        logging.info(f"OSC Address: {osc_address}")
        logging.info(f"Arguments: Sender: {sender}, Message: {message}")

    except Exception as e:
        logging.error(f"Failed to send OSC message: {e}")


def messages_page():
    conn = sqlite3.connect('messages.db')
    cursor = conn.cursor()
    # Select the last 50 messages by ordering by ID descending and limiting to 50
    cursor.execute('''
        SELECT id, timestamp, sender_name, message, channel, me 
        FROM messages
        ORDER BY id DESC
        LIMIT 50
    ''')
    messages = cursor.fetchall()
    conn.close()

    # Reverse to show from oldest to newest
    messages.reverse()
    return render_template('messages.html', messages=messages)
