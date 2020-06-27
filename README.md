# Reader for the Kamstrup Omnipower, based on the Heltec Cubecell, sending over The Things Network

__NOTE: This will probably only work for Radius customers in the greater Copenhagen area. You need to write to [kundesupport@radiuselnet.dk](mailto:kundesupport@radiuselnet.dk) to get your decryption keys__

__IMPORTANT NOTE:__ This does not work yet. The software serial reads the data correctly, but the decryption fails for some reason. 

This sketch was made for a [Heltec CubeCell](https://heltec.org/project/htcc-ab01/) (HTCC-AB01), based on the ASR650x MCU. See [Heltec Docs](https://heltec-automation-docs.readthedocs.io/en/latest/cubecell/quick_start.html)

### Hookup guide:

1. Install support for the CubeCell Boards: https://github.com/HelTecAutomation/ASR650x-Arduino/blob/master/InstallGuide/debian_ubuntu.md
2. ...

### History:
Adapted from [Claustn](https://github.com/Claustn/esp8266-kamstrup-mqtt) to use Lora,TTN instead of WiFi,MQTT.

Claustn adapted his code from [Asbjoern](https://github.com/Asbjoern/Kamstrup-Radius-Interface/) whose repo has many more features than this little thing.

