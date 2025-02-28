Connects directly, via configurable IP, with an x32/m32 console to retrieve specified channel status:
* On
  * Channel is unmuted and fader is above -90
* Off
  * Channel is muted
* Ready
  * Channel is unmuted and fader is completely down

After retrieving the channel status, an OSC message (configurable) is sent to whatever IP:Port is specified
