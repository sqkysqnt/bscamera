### TheatreChat

Theatre Chat is a messaging system used to facilitate communication between different devices in a theatrical setup. It utilizes OSC (Open Sound Control) messages to send commands and status updates, allowing seamless coordination and control over elements like sound, lighting, and camera systems during a live performance.

BSCamera integrates seamlessly with Theatre Chat, allowing it to send and receive OSC messages to facilitate device communication during live performances. By utilizing Theatre Chat, the camera can relay information such as microphone status changes or PIR sensor triggers to other systems, enabling dynamic adjustments like lighting changes or audio cues. The integration enhances coordination between different theatrical elements, ensuring smooth transitions and synchronized actions, ultimately improving the overall quality and consistency of the performance.

### TheatreChat Message Format

/theatrechat/message/[channel] "[sender]" "[message]"


- **Channel**: Specifies the destination or category of the message (e.g., "lighting", "audio").
- **Sender**: Identifies the source device, allowing receiving systems to know which device originated the message.
- **Message**: The command or information to be conveyed, such as "Mic On", "Mic Off", or a custom message.

### From within TheatreChat

Receiving:

BSCamera is set to send messages to TheatreChat. 

- **Channel**: Configurable from the web interface - default is "cameras"
- **Sender**: Configured as the device name (which is configurable from the web interface)
- **Message**: Pre determined. Test messages can be sent from the web interface. 
- **Destination**: The broadcast address on the dhcp subnet (i.e. 192.168.1.255).
- **Sending Port**: Defaults to 27900. Configurable in web interface.
- **Receiving Port**: Defaults to 27900. Configurable in web interface.

- **Sound Threshold**: The threshold before a message is triggered. This is an arbitrary unit (i.e. 6000 is enough for me to snap my fingers to trigger it but not the keyboard next to it)
- **PIR DEBOUNCE**: The time between the PIR triggering and sending messages.
- **SOUND DEBOUNCE**: The time between the MIC triggering and sending messages.

Sending:

Messages from TheatreChat are interpreted as follows:

[destination] [command] [optional argument/s]

If the device name is ChurchEntrance,

"ChurchEntrance display Modern Major General" would display the text "Modern Major General" on the screen.

Optionally, "All" can be used which will be interpretted by every device and not just one. The destination is not case sensitive, the command is.

| OSC                       | TheatreChat                   | What it does                                                                                                                                                                                          |
|---------------------------|-------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| /clear                    | All clear                     | Turn the leds and screen off temporarily                                                                                                                                                              |
| /display [text to display | All display [text to display] | Display text on the screen                                                                                                                                                                            |
| /micOn                    | All micOn                     | Turns the leds green and displays Mic On on the screen.                                                                                                                                               |
| /micReady                 | All micReady                  | Slowly blinks the leds blue and displays Mic Ready on the screen.                                                                                                                                     |
| /micOff                   | All micOff                    | Turns the leds red and displays Mic Off on the screen.                                                                                                                                                |
| /ledOn [color]            | All ledOn [color]             | Turns the leds on to the color specified. The color format can be hex (i.e. #ffffff) or specified using a [common gel color or number](https://github.com/sqkysqnt/bscamera/blob/main/docs/colors.md) |
