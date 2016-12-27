# esp8266-env-bme280
ESP8266 firmware for reading from a BME280 environmental sensor and displaying to an SSD1306 128x32 screen, with WiFi and OTA update.

Set up to run in [platformio](http://platformio.org/), but should run in Arduino IDE as well if main.cpp is renamed to `.ino`, with appropriate libraries loaded (see `platformio.ini`)

**Note:** Firmware requires your WiFi SSID and password to connect - these should be set up as defines in `src/wifiAuth.h`, this file is ignored by `.gitignore` to avoid sharing your WiFi details with the world! You will need to create this file and set up contents:

```
#define WIFI_SSID "yourssid"
#define WIFI_PASSWORD "yourpassword"
```

File will not be displayed by default in platformio, since it is ignored by git.

# OTA
Firmware supports OTA. The firmware sets up an mDNS hostname of `env280-CHIPID`, where `CHIPID` is the id of the esp8266 itself. 

To flash the firmware for the first time, go to `platformio.ini` and comment out this line:

```
upload_port = env280-da013c.local 
```

You can then flash using serial port as normal. When this has completed, uncomment the line and update the host name to match the id of your board:

```
upload_port = env280-YOURID.local 
```

This should now update firmware using OTA.

You can find the id from the second screen displayed by the firmware. This also gives the IP address - if you can't use the `.local` hostname (for example if you don't have mDNS support on your PC) then you can use the IP instead.
