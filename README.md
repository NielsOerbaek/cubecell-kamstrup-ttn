# Reader for the Kamstrup Omnipower, based on the Heltec Cubecell, sending over The Things Network

__NOTE: This will probably only work for Radius customers in the greater Copenhagen area. You need to write to [kundesupport@radiuselnet.dk](mailto:kundesupport@radiuselnet.dk) to get your decryption keys. You should get 2 keys. If you only get 1, write back and ask them for 2.__

__IMPORTANT NOTE:__ This does not work yet. The software serial reads the data correctly, but the decryption fails for some reason. 

This sketch was made for a [Heltec CubeCell](https://heltec.org/project/htcc-ab01/) (HTCC-AB01), based on the ASR650x MCU. See [Heltec Docs](https://heltec-automation-docs.readthedocs.io/en/latest/cubecell/quick_start.html)

### Hookup guide:

1. Install support for the CubeCell Boards: https://github.com/HelTecAutomation/ASR650x-Arduino/blob/master/InstallGuide/debian_ubuntu.md
2. Use the `secrets.h.TEMPLATE` to create your own `secrets.h` file with your meter and device info.
3. In the arduino IDE:
    1. Select the CubeCell Board (HTCC-AB01)
    2. Select your frequency plan (Probably EU868)
    3. Set RGB to "Deactivate"
    4. Set NETMODE to "OTAA"
4. Flash your code to the board.
5. Connect the antenna to the board.
6. Connect a small lipo battery to the battery socket.
7. Connect pins: 
    1. Kamstrup upper left <-> CubeCell `GND`
    2. Kamstrup lower left <-> CubeCell `VIN`
    3. Kamstrup upper center <-> CubeCell `0`

If working, the LED should:

- Blink white three times on start-up
- Blink green when decrypt succeeds, once per successful read.
- Blink red once when decryption fails.
- Blink blue 3 times when sending to TTN.
- Blink red 3 times if no valid frames were read before timeout.


### TODO:

- Make the KamstrupSerial into an object instead of a bunch of standalone functions.
- Clean up the code...

### History:
Adapted from [Claustn](https://github.com/Claustn/esp8266-kamstrup-mqtt) to use Lora,TTN instead of WiFi,MQTT.

Claustn adapted his code from [Asbjoern](https://github.com/Asbjoern/Kamstrup-Radius-Interface/).
