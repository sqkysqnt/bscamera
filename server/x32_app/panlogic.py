# panlogic

def process_pan(channel, x32_ip, x32_sock, osc_build_message, send_osc_message, receive_response):
    """
    Function to retrieve and process pan settings for a given channel.
    """
    # Build OSC address for retrieving pan
    pan_address = f"/ch/{channel}/mix/pan"
    print(f"Requesting pan settings for channel {channel}...")

    # Send OSC message to request the pan setting
    message = osc_build_message(pan_address)
    send_osc_message(x32_sock, message, x32_ip, 10023)

    # Receive the response
    address, args = receive_response(x32_sock)
    if address == pan_address and args:
        pan_value = args[0]  # Assume the first argument is the pan value
        print(f"Pan value for channel {channel}: {pan_value}")
    else:
        print(f"Failed to retrieve pan settings for channel {channel}.")

