/*
   WHAT THIS PROGRAM DOES:
   ----------------------
   This program (or "sketch") receives lighting control data over WiFi and sends it to 
   stage lights. It's like a translator between the internet and your lights!
   
   HOW IT WORKS:
   ------------
   1. It receives Art-Net data (a type of lighting control protocol) over WiFi
   2. It converts this data to DMX512 format (the language that most stage lights understand)
   3. It sends this data to a MAX485 chip, which connects to your lights
   
   HARDWARE OPTIONS:
   ---------------
   This program can connect to your lights in two different ways:
   - UART (Universal Asynchronous Receiver/Transmitter): This is like a simple digital walkie-talkie
   - I2S (Inter-IC Sound): This is normally used for audio but works great for DMX too!
   
   IMPORTANT: The way you connect the wires depends on which method you choose (UART or I2S)
   
   MORE INFORMATION:
   ---------------
   For more details and pictures, visit:
   https://robertoostenveld.nl/art-net-to-dmx512-with-esp8266/
   
   The full code is available at:
   https://github.com/robertoostenveld/esp8266_artnet_dmx512
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

// Debug flags - set to true to enable debug messages
bool DEBUG_WEB = false;    // Debug messages for web interface
bool DEBUG_DMX = true;     // Debug messages for DMX data

// Uncomment one of these to select the DMX output method
#define ENABLE_UART
// #define ENABLE_I2S

// Include the appropriate DMX output implementation
#ifdef ENABLE_UART
#include "dmx_uart.h"
#endif

#ifdef ENABLE_I2S
#include "dmx_i2s.h"
// Uncomment for super safe timing for picky devices
#define I2S_SUPER_SAFE
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
#define WITH_TEST_CODE

// Constants
const char *host = "ARTNET";
const char *version = __DATE__ " / " __TIME__;
constexpr uint16_t DMX_CHANNELS = 512;

// Global objects
ESP8266WebServer server(80);
NetworkManager *networkManager = nullptr;
ArtnetManager *artnetManager = nullptr;
DmxOutput *dmxOutput = nullptr;

// Global variables
unsigned long tic_web = 0;
unsigned long last_packet_received = 0;
uint8_t *dmxData = nullptr;
float fps = 0.0f;
uint32_t packetCounter = 0;

#ifdef ENABLE_ARDUINO_OTA
#include <ArduinoOTA.h>
bool arduinoOtaStarted = false;
unsigned int last_ota_progress = 0;
#endif

// This will be called for each UDP packet that is received
void onDmxPacket(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data)
{
  unsigned long now = millis();
  unsigned long packetInterval = now - last_packet_received;
  last_packet_received = now;

  if (packetInterval > 1000)
  {
    if (DEBUG_DMX) Serial.println("Received DMX data after pause");
  }

  // Update ArtNet statistics
  artnetManager->updateStatistics();

  // Art-Net universe numbering can be confusing:
  // - In Art-Net, universes are often 0-based (0-255)
  // - In user interfaces, they're often shown as 1-based (1-256)
  // - Some software uses different offsets or subnets
  
  // Print all universes in debug mode to help diagnose
  if (DEBUG_DMX) {
    Serial.print("Received Art-Net universe: ");
    Serial.print(universe);
    Serial.print(", Configured universe: ");
    Serial.println(config.universe);
  }

  // Process only if universe matches configuration
  // Note: You might need to adjust by +1 or -1 depending on your Art-Net source
  if (universe == config.universe)
  {
    // Copy DMX data to our buffer
    uint16_t channelsToProcess = min(length, (uint16_t)DMX_CHANNELS);
    memcpy(dmxData, data, channelsToProcess);
    
    if (DEBUG_DMX) {
      Serial.print("DMX Universe: ");
      Serial.print(universe);
      Serial.print(", Length: ");
      Serial.print(length);
      Serial.print(", Sequence: ");
      Serial.println(sequence);
      
      // Print first 16 DMX channel values in hex and decimal
      Serial.println("DMX Data (first 16 channels):");
      for (int i = 0; i < min(16, (int)channelsToProcess); i++) {
        Serial.print("Ch");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(data[i]);
        Serial.print(" (0x");
        if (data[i] < 16) Serial.print("0"); // Add leading zero for values < 16
        Serial.print(data[i], HEX);
        Serial.print(") ");
        
        // Print 4 channels per line
        if ((i + 1) % 4 == 0) Serial.println();
      }
      if ((min(16, (int)channelsToProcess) % 4) != 0) Serial.println();
      
      // Print timing info
      Serial.print("Packet interval: ");
      Serial.print(packetInterval);
      Serial.println(" ms");
      
      // Print network stats
      Serial.print("Total packets: ");
      Serial.print(artnetManager->getPacketCounter());
      Serial.print(", FPS: ");
      Serial.println(artnetManager->getFramesPerSecond(), 2);
      
      // Print WiFi signal strength
      Serial.print("WiFi RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      
      Serial.println("----------------------------");
    }

    // Store universe and sequence for reference
    // (These could be moved to the ArtnetManager class)
    // global.universe = universe;
    // global.sequence = sequence;
  }
  else if (DEBUG_DMX) {
    Serial.print("Ignored DMX Universe: ");
    Serial.print(universe);
    Serial.print(" (configured for universe: ");
    Serial.print(config.universe);
    Serial.println(")");
    
    // Try to help diagnose the issue
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
void testCode()
{
  long now = millis();
  uint8_t x = (now / 60) % 240;
  if (x > 120)
  {
    x = 240 - x;
  }

  // Set DMX values for test pattern
  dmxData[0] = 0;    // Start code (always 0)
  dmxData[1] = 255;  // Set channel 1 to full brightness
  dmxData[2] = x;    // Animate channel 2
  dmxData[3] = 255 - x; // Animate channel 3 inversely
  dmxData[4] = 0;   
  dmxData[5] = 30;  
  dmxData[6] = 0;   
  dmxData[7] = 0;   
  dmxData[8] = 150; 
  
  if (DEBUG_DMX) {
    Serial.println("Test pattern generated");
    Serial.print("Position value: ");
    Serial.println(x);
    Serial.println("DMX Test Data: Ch1=255, Ch2=" + String(x) + ", Ch3=" + String(255-x));
  }
}
#endif

void setup()
{
  // Initialize serial for debugging
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }
  Serial.println("Setup starting");

  // Allocate DMX data buffer
  dmxData = new uint8_t[DMX_CHANNELS];
  memset(dmxData, 0, DMX_CHANNELS);

  // Initialize the file system
  LittleFS.begin();

  // Load configuration
  if (!loadConfig())
  {
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
  if (DEBUG_WEB) Serial.println("Initializing Arduino OTA");
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword(ARDUINO_OTA_PASSWORD);
  ArduinoOTA.onStart([]()
                     { if (DEBUG_WEB) Serial.println("OTA Start"); });
  ArduinoOTA.onError([](ota_error_t error)
                     { if (DEBUG_WEB) Serial.printf("Error[%u]: ", error); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    if (progress != last_ota_progress) {
      if (DEBUG_WEB) Serial.printf("OTA Progress: %u%%\n", (progress / (total / 100)));
      last_ota_progress = progress;
    } });
  ArduinoOTA.onEnd([]()
                   { if (DEBUG_WEB) Serial.println("OTA End"); });
  if (DEBUG_WEB) Serial.println("Arduino OTA init complete");

  if (networkManager->isConnected())
  {
    if (DEBUG_WEB) Serial.println("Starting Arduino OTA (setup)");
    ArduinoOTA.begin();
    arduinoOtaStarted = true;
  }
#endif

  // Initialize DMX output
#ifdef ENABLE_UART
  dmxOutput = new DmxUart();
  Serial.println("Using UART DMX output on pin " + String(DMX_TX_PIN));
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
  
  // Print DMX configuration
  Serial.println("DMX debugging enabled");
  
#ifdef ENABLE_UART
  Serial.print("DMX UART pin: ");
  Serial.println(DMX_TX_PIN);
#endif

#ifdef ENABLE_I2S
  Serial.print("DMX I2S pin: ");
  Serial.println(I2S_PIN);
#endif

  Serial.print("DMX Universe: ");
  Serial.println(config.universe);
  Serial.print("DMX Channels: ");
  Serial.println(config.channels);
  Serial.print("DMX Delay: ");
  Serial.println(config.delay);
  
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

void loop()
{
  unsigned long now = millis();

  // Handle network tasks when not receiving DMX data
  if (now - last_packet_received > 1000)
  {
    networkManager->process();

#ifdef ENABLE_ARDUINO_OTA
    if (networkManager->isConnected() && !arduinoOtaStarted)
    {
      if (DEBUG_WEB) Serial.println("Starting Arduino OTA (loop)");
      ArduinoOTA.begin();
      arduinoOtaStarted = true;
    }
    ArduinoOTA.handle();
#endif
  }

  // Handle web server requests
  server.handleClient();

  // Check WiFi connection
  if (!networkManager->isConnected())
  {
    delay(10);
#ifndef ENABLE_STANDALONE
    return;
#endif
  }

  // Handle web interface activity
  if ((millis() - tic_web) < 5000)
  {
    // Web interface is active, slow down a bit
    delay(25);
  }
  else
  {
    // Read ArtNet data
    artnetManager->read();

    // Update global statistics for webinterface.cpp
    packetCounter = artnetManager->getPacketCounter();
    fps = artnetManager->getFramesPerSecond();

    // Send DMX data at configured rate
    if ((millis() - now) > config.delay)
    {
#ifdef WITH_TEST_CODE
      testCode();
#endif
      // Send DMX data using the selected output method
      dmxOutput->sendDmxData(dmxData, DMX_CHANNELS, config.channels);
      
      // Print DMX sending status periodically
      static unsigned long lastStatusTime = 0;
      if (millis() - lastStatusTime > 5000) {
#ifdef ENABLE_UART
        Serial.println("Sending DMX data to pin " + String(DMX_TX_PIN) + ", channels: " + String(config.channels));
#endif
#ifdef ENABLE_I2S
        Serial.println("Sending DMX data to pin " + String(I2S_PIN) + ", channels: " + String(config.channels));
#endif
        lastStatusTime = millis();
      }
    }
  }
}