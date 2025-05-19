/*
   This sketch receives Art-Net data of one DMX universes over WiFi
   and sends it to a MAX485 module as an interface between wireless
   Art-Net and wired DMX512.

   This firmware can either use the UART (aka Serial) interface to
   the MAX485 module, or the I2S interface. Note that the wiring
   depends on whether you use UART or I2S.

   See https://robertoostenveld.nl/art-net-to-dmx512-with-esp8266/
   and comments to that blog post.

   See https://github.com/robertoostenveld/esp8266_artnet_dmx512
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ArtnetWifi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstdint>

// Include our modular components
#include "webinterface.h"
#include "network_manager.h"
#include "artnet_manager.h"
#include "dmx_output.h"

// Uncomment one of these to select the DMX output method
// #define ENABLE_UART
#define ENABLE_I2S

// Include the appropriate DMX output implementation
#ifdef ENABLE_UART
#include "dmx_uart.h"
#endif

#ifdef ENABLE_I2S
#include "dmx_i2s.h"
// Uncomment for super safe timing for picky devices
// #define I2S_SUPER_SAFE
#endif

// Comment in to enable standalone mode
// #define ENABLE_STANDALONE
// #define STANDALONE_PASSWORD "wifisecret"

// Enable OTA (over the air programming)
// #define ENABLE_ARDUINO_OTA
// #define ARDUINO_OTA_PASSWORD "otasecret"

// Enable the web interface
#define ENABLE_WEBINTERFACE

// Enable multicast DNS
#define ENABLE_MDNS

// Enable test code for moving head
// #define WITH_TEST_CODE

// Constants
const char* host = "ARTNET";
const char* version = __DATE__ " / " __TIME__;
constexpr uint16_t DMX_CHANNELS = 512;

// Global objects
ESP8266WebServer server(80);
NetworkManager* networkManager = nullptr;
ArtnetManager* artnetManager = nullptr;
DmxOutput* dmxOutput = nullptr;

// Global variables
unsigned long tic_web = 0;
unsigned long last_packet_received = 0;
uint8_t* dmxData = nullptr;

#ifdef ENABLE_ARDUINO_OTA
#include <ArduinoOTA.h>
bool arduinoOtaStarted = false;
unsigned int last_ota_progress = 0;
#endif

// This will be called for each UDP packet that is received
void onDmxPacket(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
  unsigned long now = millis();
  if (now - last_packet_received > 1000) {
    Serial.print("Received DMX data\n");
  }
  last_packet_received = now;

  // Update ArtNet statistics
  artnetManager->updateStatistics();

  // Process only if universe matches configuration
  if (universe == config.universe) {
    // Copy DMX data to our buffer
    uint16_t channelsToProcess = min(length, (uint16_t)DMX_CHANNELS);
    memcpy(dmxData, data, channelsToProcess);
    
    // Store universe and sequence for reference
    // (These could be moved to the ArtnetManager class)
    // global.universe = universe;
    // global.sequence = sequence;
  }
}

#ifdef WITH_TEST_CODE
void testCode() {
  long now = millis();
  uint8_t x = (now / 60) % 240;
  if (x > 120) {
    x = 240 - x;
  }

  // Set DMX values for test pattern
  dmxData[1] = x;    // x 0 - 170
  dmxData[2] = 0;    // x fine
  dmxData[3] = x;    // y: 0: -horz. 120: vert, 240: +horz
  dmxData[4] = 0;    // y fine
  dmxData[5] = 30;   // color wheel: red
  dmxData[6] = 0;    // pattern
  dmxData[7] = 0;    // strobe
  dmxData[8] = 150;  // brightness
}
#endif

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  Serial.println("Setup starting");

  // Allocate DMX data buffer
  dmxData = new uint8_t[DMX_CHANNELS];
  memset(dmxData, 0, DMX_CHANNELS);

  // Initialize the file system
  LittleFS.begin();

  // Load configuration
  if (!loadConfig()) {
    defaultConfig();
    saveConfig();
  }

  // Initialize network manager
  networkManager = new NetworkManager(host);
  
  // Connect to WiFi
#ifdef ENABLE_STANDALONE
  networkManager->begin(true, STANDALONE_PASSWORD);
#else
  #ifdef STANDALONE_PASSWORD
    networkManager->begin(false, STANDALONE_PASSWORD);
  #else
    networkManager->begin();
  #endif
#endif

#ifdef ENABLE_MDNS
  // Start mDNS service
  networkManager->startMDNS();
#endif

#ifdef ENABLE_ARDUINO_OTA
  Serial.println("Initializing Arduino OTA");
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(ARDUINO_OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (progress != last_ota_progress) {
      Serial.printf("OTA Progress: %u%%\n", (progress / (total / 100)));
      last_ota_progress = progress;
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA End");
  });
  Serial.println("Arduino OTA init complete");

  if (networkManager->isConnected()) {
    Serial.println("Starting Arduino OTA (setup)");
    ArduinoOTA.begin();
    arduinoOtaStarted = true;
  }
#endif

  // Initialize DMX output
#ifdef ENABLE_UART
  dmxOutput = new DmxUart();
#endif

#ifdef ENABLE_I2S
  #ifdef I2S_SUPER_SAFE
    dmxOutput = new DmxI2s(true);
    Serial.println("Using super safe I2S timing");
  #else
    dmxOutput = new DmxI2s(false);
    Serial.println("Using normal I2S timing");
  #endif
#endif

  dmxOutput->begin();

#ifdef ENABLE_WEBINTERFACE
  // Setup web server routes
  setupWebServer(server);
  server.begin();
#endif

  // Initialize ArtNet
  artnetManager = new ArtnetManager();
  artnetManager->begin();
  artnetManager->setDmxCallback(onDmxPacket);

  // Initialize timing variables
  tic_web = 0;
  last_packet_received = 0;

  Serial.println("Setup done");
}

void loop() {
  unsigned long now = millis();
  
  // Handle network tasks when not receiving DMX data
  if (now - last_packet_received > 1000) {
    networkManager->process();
    
#ifdef ENABLE_ARDUINO_OTA
    if (networkManager->isConnected() && !arduinoOtaStarted) {
      Serial.println("Starting Arduino OTA (loop)");
      ArduinoOTA.begin();
      arduinoOtaStarted = true;
    }
    ArduinoOTA.handle();
#endif
  }
  
  // Handle web server requests
  server.handleClient();

  // Check WiFi connection
  if (!networkManager->isConnected()) {
    delay(10);
#ifndef ENABLE_STANDALONE
    return;
#endif
  }

  // Handle web interface activity
  if ((millis() - tic_web) < 5000) {
    // Web interface is active, slow down a bit
    delay(25);
  } else {
    // Read ArtNet data
    artnetManager->read();

    // Send DMX data at configured rate
    if ((millis() - now) > config.delay) {
#ifdef WITH_TEST_CODE
      testCode();
#endif
      // Send DMX data using the selected output method
      dmxOutput->sendDmxData(dmxData, DMX_CHANNELS, config.channels);
    }
  }
}