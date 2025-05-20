/*
   ART-NET TO DMX512 BRIDGE FOR ESP8266
   ------------------------------------
   This program receives lighting control data over WiFi using the Art-Net protocol
   and sends it out as DMX512 to stage lights via a MAX485 chip.

   HOW IT WORKS:
   1. Receives Art-Net data (a common lighting control protocol) over WiFi.
   2. Converts this data to DMX512 format (used by most stage lights).
   3. Sends the DMX512 data to a MAX485 chip, which drives the DMX line.

   HARDWARE OPTIONS:
   - UART: Uses a standard serial port (bit-banged or hardware) for DMX output.
   - I2S: Uses the I2S peripheral for precise DMX timing (recommended for reliability).

   NOTE: Wiring depends on the output method (UART or I2S). See documentation for details.

   MORE INFORMATION:
   - Project page: https://robertoostenveld.nl/art-net-to-dmx512-with-esp8266/
   - Source code: https://github.com/robertoostenveld/esp8266_artnet_dmx512
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

// Project modules
#include "webinterface.h"
#include "network_manager.h"
#include "artnet_manager.h"
#include "dmx_output.h"

// Debug flags
bool DEBUG_WEB = false;    // Enable debug messages for web interface
bool DEBUG_DMX = false;    // Enable debug messages for DMX data

// Output method selection
#define ENABLE_UART
// #define ENABLE_I2S

#ifdef ENABLE_UART
#include "dmx_uart.h"
#endif

#ifdef ENABLE_I2S
#include "dmx_i2s.h"
// #define I2S_SUPER_SAFE // Uncomment for extra timing margin
#endif

// #define ENABLE_STANDALONE // Uncomment for AP mode
// #define STANDALONE_PASSWORD "wifisecret"

// #define ENABLE_ARDUINO_OTA // Uncomment to enable OTA updates
// #define ARDUINO_OTA_PASSWORD "otasecret"

#define ENABLE_WEBINTERFACE
#define ENABLE_MDNS
// #define WITH_TEST_CODE // Uncomment to enable DMX test pattern

// --- Constants ---
const char *host = "ARTNET"; // mDNS and WiFi hostname
const char *version = __DATE__ " / " __TIME__; // Build version string
constexpr uint16_t DMX_CHANNELS = 512; // DMX512 standard channel count

// --- Global objects ---
ESP8266WebServer server(80);         // Web server for configuration
NetworkManager *networkManager = nullptr; // Handles WiFi and mDNS
ArtnetManager *artnetManager = nullptr;   // Handles Art-Net reception
DmxOutput *dmxOutput = nullptr;           // Abstract DMX output interface

// --- Global variables ---
unsigned long tic_web = 0;           // Last web UI activity timestamp
unsigned long last_packet_received = 0; // Last Art-Net packet timestamp
uint8_t *dmxDataFront = nullptr;     // Buffer for incoming Art-Net data
uint8_t *dmxDataBack = nullptr;      // Buffer for DMX output (double buffering)
volatile bool dmxBufferReady = false; // Flag: new DMX data ready to send
float fps = 0.0f;                    // Art-Net frames per second
uint32_t packetCounter = 0;          // Art-Net packet counter

#ifdef ENABLE_ARDUINO_OTA
#include <ArduinoOTA.h>
bool arduinoOtaStarted = false;
unsigned int last_ota_progress = 0;
#endif

// Art-Net DMX packet callback: called for each received Art-Net DMX packet
void onDmxPacket(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data)
{
  unsigned long now = millis();
  unsigned long packetInterval = now - last_packet_received;
  last_packet_received = now;

  // Update Art-Net statistics (packet count, FPS)
  artnetManager->updateStatistics();

  // Throttle debug output to avoid flooding serial
  static unsigned long lastDebugOutput = 0;
  static const unsigned long DEBUG_INTERVAL = 2000; // ms

  // Only process packets for the configured universe
  if (universe == config.universe)
  {
    // Copy up to config.channels from Art-Net, zero the rest
    uint16_t channelsToProcess = min(length, (uint16_t)config.channels);
    memset(dmxDataFront, 0, DMX_CHANNELS);
    memcpy(dmxDataFront, data, channelsToProcess);

    // Double-buffer swap (atomic)
    noInterrupts();
    uint8_t* tmp = dmxDataFront;
    dmxDataFront = dmxDataBack;
    dmxDataBack = tmp;
    dmxBufferReady = true;
    interrupts();

    // Print debug info every 2 seconds if enabled
    if (DEBUG_DMX && (now - lastDebugOutput > DEBUG_INTERVAL)) {
      lastDebugOutput = now;
      Serial.println("\n===== DMX DATA UPDATE =====");
      Serial.print("DMX Universe: "); Serial.print(universe);
      Serial.print(", Length: "); Serial.print(length);
      Serial.print(", Sequence: "); Serial.println(sequence);

      // Print first 16 DMX channel values
      Serial.println("DMX Data (first 16 channels):");
      for (int i = 0; i < min(16, (int)channelsToProcess); i++) {
        Serial.print("Ch"); Serial.print(i + 1); Serial.print(": ");
        Serial.print(data[i]); Serial.print(" (0x");
        if (data[i] < 16) Serial.print("0");
        Serial.print(data[i], HEX); Serial.print(") ");
        if ((i + 1) % 4 == 0) Serial.println();
      }
      if ((min(16, (int)channelsToProcess) % 4) != 0) Serial.println();
      Serial.print("Packet interval: "); Serial.print(packetInterval); Serial.println(" ms");
      Serial.print("Total packets: "); Serial.print(artnetManager->getPacketCounter());
      Serial.print(", FPS: "); Serial.println(artnetManager->getFramesPerSecond(), 2);
      Serial.print("WiFi RSSI: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
      Serial.println("===========================");
    }
  }
  // If universe doesn't match, print debug info if enabled
  else if (DEBUG_DMX && (now - lastDebugOutput > DEBUG_INTERVAL)) {
    lastDebugOutput = now;
    Serial.print("Ignored DMX Universe: "); Serial.print(universe);
    Serial.print(" (configured for universe: "); Serial.print(config.universe); Serial.println(")");
    if (universe == config.universe - 1) {
      Serial.println("NOTE: Received universe is 1 less than configured. Art-Net uses 0-based numbering.");
      Serial.println("Consider setting config.universe to " + String(universe) + " in settings.");
    }
    else if (universe == config.universe + 1) {
      Serial.println("NOTE: Received universe is 1 more than configured. Your Art-Net source may use 1-based numbering.");
      Serial.println("Consider setting config.universe to " + String(universe) + " in settings.");
    }
  }
}

#ifdef WITH_TEST_CODE
// Generates a test DMX pattern for moving head fixtures
void testCode()
{
  long now = millis();
  uint8_t x = (now / 60) % 240;
  if (x > 120) x = 240 - x;

  // Fill DMX buffer with test values
  uint8_t* dmxData = dmxDataFront;
  memset(dmxData, 0, DMX_CHANNELS);
  dmxData[1] = 255;      // Channel 2 (1-based) full
  dmxData[2] = x;        // Channel 3 (1-based) animated
  dmxData[3] = 255 - x;  // Channel 4 (1-based) inverse
  dmxData[4] = 0;
  dmxData[5] = 30;
  dmxData[6] = 0;
  dmxData[7] = 0;
  dmxData[8] = 150;

  // Swap buffers atomically
  noInterrupts();
  uint8_t* tmp = dmxDataFront;
  dmxDataFront = dmxDataBack;
  dmxDataBack = tmp;
  dmxBufferReady = true;
  interrupts();

  if (DEBUG_DMX) {
    Serial.println("Test pattern generated");
    Serial.print("Position value: "); Serial.println(x);
    Serial.println("DMX Test Data: Ch1=255, Ch2=" + String(x) + ", Ch3=" + String(255-x) + " (using 1-based channel numbering)");
  }
}
#endif

// Arduino setup: initializes all hardware, network, and DMX output
void setup()
{
  Serial.begin(115200);
  while (!Serial) { ; }
  Serial.println("Setup starting");

  // Allocate double buffers for DMX data
  dmxDataFront = new uint8_t[DMX_CHANNELS];
  dmxDataBack = new uint8_t[DMX_CHANNELS];
  memset(dmxDataFront, 0, DMX_CHANNELS);
  memset(dmxDataBack, 0, DMX_CHANNELS);

  // Initialize file system for config storage
  LittleFS.begin();

  // Load configuration from file, or use defaults
  if (!loadConfig()) {
    defaultConfig();
    saveConfig();
  }

  // Initialize network manager (WiFi, mDNS)
  networkManager = new NetworkManager(host);

  // Connect to WiFi (AP or STA mode)
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
  networkManager->startMDNS();
#endif

#ifdef ENABLE_ARDUINO_OTA
  // Setup Arduino OTA update handler
  if (DEBUG_WEB) Serial.println("Initializing Arduino OTA");
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(ARDUINO_OTA_PASSWORD);
  ArduinoOTA.onStart([]() { if (DEBUG_WEB) Serial.println("OTA Start"); });
  ArduinoOTA.onError([](ota_error_t error) { if (DEBUG_WEB) Serial.printf("Error[%u]: ", error); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (progress != last_ota_progress) {
      if (DEBUG_WEB) Serial.printf("OTA Progress: %u%%\n", (progress / (total / 100)));
      last_ota_progress = progress;
    }
  });
  ArduinoOTA.onEnd([]() { if (DEBUG_WEB) Serial.println("OTA End"); });
  if (DEBUG_WEB) Serial.println("Arduino OTA init complete");
  if (networkManager->isConnected()) {
    if (DEBUG_WEB) Serial.println("Starting Arduino OTA (setup)");
    ArduinoOTA.begin();
    arduinoOtaStarted = true;
  }
#endif

  // Initialize DMX output (UART or I2S)
#ifdef ENABLE_UART
  dmxOutput = new DmxUart();
  Serial.println("Using UART DMX output on pin " + String(DMX_TX_PIN));
#endif
#ifdef ENABLE_I2S
#ifdef I2S_SUPER_SAFE
  dmxOutput = new DmxI2s(true);
  Serial.println("Using super safe I2S timing on pin " + String(I2S_PIN));
#else
  dmxOutput = new DmxI2s(false);
  Serial.println("Using normal I2S timing on pin " + String(I2S_PIN));
#endif
#endif
  dmxOutput->begin();

#ifdef ENABLE_WEBINTERFACE
  setupWebServer(server);
  server.begin();
#endif

  // Initialize Art-Net receiver and set DMX callback
  artnetManager = new ArtnetManager();
  artnetManager->begin();
  artnetManager->setDmxCallback(onDmxPacket);

  // Initialize timing variables
  tic_web = 0;
  last_packet_received = 0;

  Serial.println("Setup done");

  // Print DMX configuration if debugging
  if (DEBUG_DMX) {
    Serial.println("DMX debugging enabled");
#ifdef ENABLE_UART
    Serial.print("DMX UART pin: "); Serial.println(DMX_TX_PIN);
#endif
#ifdef ENABLE_I2S
    Serial.print("DMX I2S pin: "); Serial.println(I2S_PIN);
#endif
    Serial.print("DMX Universe: "); Serial.println(config.universe);
    Serial.print("DMX Channels: "); Serial.println(config.channels);
    Serial.print("DMX Delay: "); Serial.println(config.delay);
  }

  // Print hardware connection instructions
  Serial.println("\nHARDWARE CONNECTION:");
#ifdef ENABLE_UART
  Serial.println("Connect your MAX485 or similar DMX driver to:");
  Serial.println("- GPIO" + String(DMX_TX_PIN) + " for DMX data");
#endif
#ifdef ENABLE_I2S
  Serial.println("Connect your MAX485 or similar DMX driver to:");
  Serial.println("- GPIO" + String(I2S_PIN) + " (RX pin) for DMX data");
#endif
  Serial.println("- Make sure your driver chip has proper power and ground connections");
  Serial.println("- Connect a 120 ohm termination resistor at the end of the DMX line");
}

// Arduino main loop: handles network, web, Art-Net, and DMX output
void loop()
{
  unsigned long now = millis();

  // If no Art-Net received recently, process network tasks (WiFiManager, OTA)
  if (now - last_packet_received > 1000)
  {
    networkManager->process();
#ifdef ENABLE_ARDUINO_OTA
    if (networkManager->isConnected() && !arduinoOtaStarted) {
      if (DEBUG_WEB) Serial.println("Starting Arduino OTA (loop)");
      ArduinoOTA.begin();
      arduinoOtaStarted = true;
    }
    ArduinoOTA.handle();
#endif
  }

  // Handle web server requests
  server.handleClient();

  // If WiFi is disconnected, pause or return (unless in standalone mode)
  if (!networkManager->isConnected())
  {
    delay(10);
#ifndef ENABLE_STANDALONE
    return;
#endif
  }

  // If web interface is active, slow down main loop to improve responsiveness
  if ((millis() - tic_web) < 5000)
  {
    delay(25);
  }
  else
  {
    // Read Art-Net data (non-blocking)
    artnetManager->read();

    // Update statistics for web interface
    packetCounter = artnetManager->getPacketCounter();
    fps = artnetManager->getFramesPerSecond();

    // Send DMX data at a fixed frame rate (~44Hz)
    static unsigned long lastDmxSend = 0;
    static unsigned long lastWatchdogReset = 0;
    const unsigned long DMX_FRAME_PERIOD = 23; // ms between DMX frames
    const unsigned long WATCHDOG_PERIOD = 500; // ms between watchdog resets

    unsigned long currentMillis = millis();
    if ((currentMillis - lastWatchdogReset) >= WATCHDOG_PERIOD) {
      ESP.wdtFeed();
      lastWatchdogReset = currentMillis;
    }

    if ((currentMillis - lastDmxSend) >= DMX_FRAME_PERIOD)
    {
      lastDmxSend = currentMillis;

      // Always send the last known DMX buffer, even if no new Art-Net data
      uint8_t localBuffer[DMX_CHANNELS];
      memset(localBuffer, 0, DMX_CHANNELS);
      uint16_t safeChannels = constrain(config.channels, 1, DMX_CHANNELS);
      memcpy(localBuffer, dmxDataBack, safeChannels);

      dmxOutput->sendDmxData(localBuffer, safeChannels, safeChannels);
    }
  }
}