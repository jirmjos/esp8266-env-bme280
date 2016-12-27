#include <Arduino.h>

/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

// Requires brzo_i2c
#include "SSD1306Brzo.h"

// Include the UI lib
#include "OLEDDisplayUi.h"

// Include custom images
#include "images.h"

// For BME280
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// Wifi, mDNS and OTA
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// WifiManager for setting up connection
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

// Switches
#include <Bounce2.h>

// BME280 connections
#define BME_SCK   14      // D5
#define BME_MOSI  13      // D7
#define BME_MISO  12      // D6
#define BME_CS    15      // D8

// Switch connections - momentary push-to-make connected between these pins
// and GND
#define SWITCH2   5       // D1
#define SWITCH1   4       // D2

char hostName[32];
char chipId[8];

Adafruit_BME280 bme(BME_CS); // hardware SPI
boolean bme280Connected = false;

float temp = -1.0f;
float pressure = -1.0f;
float humidity = -1.0f;

#define DATA_PER_SECOND_SIZE 112
float tempPerSecond[DATA_PER_SECOND_SIZE];
uint16_t tempPerSecondFirst = 0;
uint16_t tempPerSecondCount = 0;

unsigned long lastSecondMillis = 0;

// Display connections - SDA to D3, SCL to D4 
// (plus VCC to 3.3V and GND to GND connection)
SSD1306Brzo display(GEOMETRY_128_32, 0x3c, D3, D4);

OLEDDisplayUi ui(&display);

// Firmware update state
boolean firmwareUpdateEnabled = false;
int16_t firmwareUpdateEnableTicks = 0;
int16_t firmwareUpdateEnableMinTicks = 60;

// Factory reset state
int16_t factoryResetTicks = 0;
int16_t factoryResetMinTicks = 300;

Bounce switch1 = Bounce(); 
Bounce switch2 = Bounce(); 

void drawLines(int16_t x, int16_t y, String line1, String line2) {
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(2 + x, 5 + y, line1);
  display.drawString(2 + x, 20 + y, line2);
}

void drawProgress(int16_t x, int16_t y, int16_t percent) {
  display.drawRect(x + 3, y + 24, 104, 6);
  display.fillRect(x + 5, y + 26, percent, 2);
}

void displayProgress(String text, int16_t percent) {
  display.clear();
  drawLines(0, 0, text, "");
  drawProgress(0, 0, percent);
  display.display();
}

void displayMessage(String line1, String line2) {
  display.clear();
  drawLines(0, 0, line1, line2);
  display.display();
}

void recordData(float currentData, float *data, uint16_t *dataFirst, uint16_t *dataCount, uint16_t dataSize) {
  uint16_t next = 0;
  
  next = *dataFirst + *dataCount;
  if (next >= dataSize) next -= dataSize;
  
  data[next] = currentData;
  (*dataCount)++;
  if (*dataCount > dataSize) {
    *dataCount = dataSize;
    (*dataFirst)++;
    if (*dataFirst >= dataSize) {
      *dataFirst -= dataSize;
    }
  }
}

void drawGraph(OLEDDisplay *display, int16_t x, int16_t y, float currentData, float *data, uint16_t dataFirst, uint16_t dataCount, uint16_t dataSize, float scale, int16_t radius, int16_t width) {
  
  int16_t i = 0;
  
  int16_t bi = dataFirst + dataCount - 1;  
  if (bi >= dataSize) bi -= dataSize;

  int16_t currentPixel = round(currentData * - scale);
  
  for (i = 0; i < width && i < dataCount; i++) {
    int16_t pixel = round(tempPerSecond[bi] * - scale) - currentPixel;
    boolean outside = true;
    if (pixel < -radius) {
      pixel = -radius;
    } else if (pixel > radius) {
      pixel = radius;
    } else {
      outside = false;
    }
    if (!outside || (i & 1)) {
      display->setPixel(x - i, y + pixel);
    }
    
    bi--;
    if (bi < 0) bi += dataSize;
  }

}

void drawGraphStatic(OLEDDisplay *display, int16_t x, int16_t y, float currentData, float *data, uint16_t dataFirst, uint16_t dataCount, uint16_t dataSize, float scale, int16_t radius) {
  
  int16_t i = 0;


  int16_t bi = dataFirst + dataCount - 1;  
  if (bi >= dataSize) bi -= dataSize;

//  int16_t currentPixel = round(data[bi] * - scale);
  int16_t currentPixel = round(currentData * - scale);

  for (i = 0; i < dataCount; i++) {
    int16_t pixel = round(tempPerSecond[i] * - scale) - currentPixel;
    boolean outside = true;
    if (pixel < -radius) {
      pixel = -radius;
    } else if (pixel > radius) {
      pixel = radius;
    } else {
      outside = false;
    }
    if (!(i >= bi - 2 && i < bi + 8)) {
      if (!outside || (i & 1)) {
        display->setPixel(x + i, y + pixel);
      }
    }
  }

  display->drawCircle(x + bi, y, 1);    
  if (bi & 1) {
    display->setPixel(x + bi, y);
  }

}

void drawMain(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  char line[32];

//  drawGraph(display, x + 113, y + 11, temp, tempPerSecond,tempPerSecondFirst, tempPerSecondCount, DATA_PER_SECOND_SIZE, 8.0f, 8, 112);
  drawGraphStatic(display, x + 3, y + 10, temp, tempPerSecond,tempPerSecondFirst, tempPerSecondCount, DATA_PER_SECOND_SIZE, 8.0f, 8);

  display->setFont(ArialMT_Plain_10);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  dtostrf(temp, 4, 1, line);
  display->drawString(24 + x, 20 + y, line);

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(24 + x, 20 + y, " C");
  display->drawCircle(26 + x, 22 + y, 1);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  sprintf(line, "%2d", round(humidity));
  display->drawString(55 + x, 20 + y, line);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(55 + x, 20 + y, "%");

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  sprintf(line, "%4d", round(pressure));
  display->drawString(96 + x, 20 + y, line);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(96 + x, 20 + y, "hPa");
}

void drawIPAndId(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);

  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(16 + x, 5 + y, "ip:");
  display->drawString(16 + x, 20 + y, "id:");

  display->drawString(105 + x, 5 + y, WiFi.localIP().toString().c_str());
  display->drawString(105 + x, 20 + y, hostName);
}

void drawFirmwareEnable(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);

  if (firmwareUpdateEnabled) {
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(2 + x, 5 + y, "Update ENABLED");
    display->drawString(2 + x, 20 + y, "Press select to disable");

    if (firmwareUpdateEnableTicks > 0) {
      if (switch2.fell()) {
        firmwareUpdateEnableTicks = 0;
      }      
    } else {
      if (switch2.rose()) {
        firmwareUpdateEnabled = false;
      }      
    }
    
  } else {
    
    if (!switch2.read()) {
      firmwareUpdateEnableTicks++;
      if (firmwareUpdateEnableTicks > firmwareUpdateEnableMinTicks) {
        firmwareUpdateEnabled = true;
        firmwareUpdateEnableTicks = firmwareUpdateEnableMinTicks;
      }
    } else {
      firmwareUpdateEnableTicks -= 4;
      if (firmwareUpdateEnableTicks <= 0) firmwareUpdateEnableTicks = 0;
    }
    
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(2 + x, 5 + y, "Update DISABLED");
    if (firmwareUpdateEnableTicks == 0) {
      display->drawString(2 + x, 20 + y, "Hold select to enable");
    } else {
      drawProgress(x, y, firmwareUpdateEnableTicks * 100 / firmwareUpdateEnableMinTicks);
    }
  }

}

void drawFactoryReset(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);

  if (!switch2.read()) {
    factoryResetTicks++;
    if (factoryResetTicks > factoryResetMinTicks) {
      ESP.eraseConfig();
      displayMessage("Settings reset", "Restarting...");
      delay(1000);
      ESP.reset();
    }
  } else {
    factoryResetTicks -= 4;
    if (factoryResetTicks <= 0) factoryResetTicks = 0;
  }
  
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(2 + x, 5 + y, "Reset settings");
  if (factoryResetTicks == 0) {
    display->drawString(2 + x, 20 + y, "Hold select to reset");
  } else {
    drawProgress(x, y, factoryResetTicks * 100 / factoryResetMinTicks);
  }

}



// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = { drawMain, drawIPAndId, drawFirmwareEnable, drawFactoryReset};

// how many frames are there?
int frameCount = 4;

//void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
//  display->setTextAlignment(TEXT_ALIGN_RIGHT);
//  display->setFont(ArialMT_Plain_10);
//  display->drawString(128, 0, String(millis()));
//}

// Overlays are statically drawn on top of a frame eg. a clock
//OverlayCallback overlays[] = { msOverlay };
//int overlaysCount = 1;

OverlayCallback overlays[] = { };
int overlaysCount = 0;

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  char line1[32];
  char line2[32];
  
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  
  sprintf(line1, "Use WiFi network %s", chipId);
  sprintf(line2, "Go to http://%s", WiFi.softAPIP().toString().c_str());
  displayMessage(line1, line2);  
}

void setup() {
  
  sprintf(hostName, "env280-%06x", ESP.getChipId());

  sprintf(chipId, "%06x", ESP.getChipId());
  
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  //Set up switches
  pinMode(SWITCH1, INPUT_PULLUP);
  pinMode(SWITCH2, INPUT_PULLUP);
  switch1.attach(SWITCH1);
  switch2.attach(SWITCH2);
  switch1.interval(5); // interval in ms
  switch2.interval(5);

  bme280Connected = bme.begin();
  if (!bme280Connected) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }

	// The ESP is capable of rendering 60fps in 80Mhz mode
	// but that won't give you much time for anything else
	// run it in 160Mhz mode or just set it to 30 fps
  ui.setTargetFPS(60);

	// Customize the active and inactive symbol
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(RIGHT);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_UP);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  ui.setOverlays(overlays, overlaysCount);

  ui.setTimePerTransition(400);

  ui.disableAutoTransition();

  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();

  displayMessage("WiFi connecting", "Please wait...");
  
  WiFiManager wifiManager;
  
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  
  //timeout - this will quit WiFiManager if it's not configured in 3 minutes, causing a restart
  wifiManager.setConfigPortalTimeout(180);

  wifiManager.autoConnect(chipId);
  
  // WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  // while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  //   displayMessage("No WiFi connection", "Rebooting...");
  //   Serial.println("No WiFi connection, rebooting...");
  //   delay(5000);
  //   ESP.restart();
  // }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostName);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with its md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    // String type;
    // if (ArduinoOTA.getCommand() == U_FLASH)
    //   type = "sketch";
    // else // U_SPIFFS
    //   type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    // Serial.println("Start updating " + type);
    Serial.println("Start updating");
    displayMessage("Updating...", "");

  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    displayMessage("Update complete", "Restarting...");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char line[32];
    // sprintf(line, "Progress: %u%%\r", (progress / (total / 100)));
    displayProgress("Updating...", (progress / (total / 100)));

    // Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
      displayMessage("Update error", "Auth failed!");
      
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
      displayMessage("Update error", "Begin failed!");
      
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
      displayMessage("Update error", "Connect failed!");
      
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
      displayMessage("Update error", "Receive failed!");
      
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
      displayMessage("Update error", "End failed!");
    }
  });
  
  ArduinoOTA.begin();
  
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  lastSecondMillis = millis();
}


void loop() {
  ArduinoOTA.handle();

  switch1.update();
  switch2.update();
  if (switch1.rose()) {
    ui.nextFrame();
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {

    if (bme280Connected) {
      temp = bme.readTemperature();
      pressure = bme.readPressure() / 100.0f;
      humidity = bme.readHumidity();

      while (millis() - lastSecondMillis > 1000) {
        lastSecondMillis += 1000;

        recordData(temp, tempPerSecond, &tempPerSecondFirst, &tempPerSecondCount, DATA_PER_SECOND_SIZE);
      }
    }
    
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }
  
}
