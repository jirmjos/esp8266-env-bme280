# esp8266-env-bme280
ESP8266 firmware for reading from a BME280 environmental sensor and displaying to an SSD1306 128x32 screen, with WiFi and OTA update.

**Note:** Firmware requires your WiFi SSID and password to connect - these should be set up as defines in `src/wifiAuth.h`, this file is ignored by `.gitignore` to avoid sharing your WiFi details with the world! You will need to create this file and set up contents:

```
#define WIFI_SSID "yourssid"
#define WIFI_PASSWORD "yourpassword"
```

File will not be displayed by default in platformio, since it is ignored by git.
