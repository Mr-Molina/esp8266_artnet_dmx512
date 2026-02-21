#ifndef _WEBINTERFACE_H_
#define _WEBINTERFACE_H_

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstdint>

// ================================================================
// WHAT IS THIS FILE?
// This file defines the web interface for our lighting controller.
// It lets you control and configure the device through a web page.
// ================================================================

// Debug flags - defined in main.cpp
extern bool DEBUG_WEB;
extern bool DEBUG_DMX;

// Check if we have the right version of ArduinoJson library
#ifndef ARDUINOJSON_VERSION
#error ArduinoJson version 7 not found, please include ArduinoJson.h in your .ino file
#endif

#if ARDUINOJSON_VERSION_MAJOR < 7
#error ArduinoJson version 7 or higher is required
#endif

// Macros for moving data between JSON and config structure
// Copy a number from our config to JSON
#define N_CONFIG_TO_JSON(x, y) \
  {                            \
    root[y] = config.x;        \
  }

// Copy a string from our config to JSON
#define S_CONFIG_TO_JSON(x, y) \
  {                            \
    root[y] = config.x;        \
  }

// This structure holds all our configuration settings
struct Config
{
  uint16_t universe; // DMX universe number (1-32767)
  uint16_t channels; // Total number of DMX channels to transmit (1-512), always starting from channel 1
  uint16_t delay;    // Delay between DMX packets in milliseconds (1-1000)
  char adminPassword[33]; // Shared password for web administration (empty disables auth)
};

// Make our config variable available to other files
extern Config config;

// Setup all the web server routes (pages)
void setupWebServer(ESP8266WebServer& server);

// Configuration functions
bool defaultConfig(void);  // Set default configuration values
bool loadConfig(void);     // Load configuration from file
bool saveConfig(void);     // Save configuration to file

bool ensureAuthorized();   // Require HTTP auth for sensitive endpoints

// Web request handlers - these functions process different web requests
void handleUpdate1(void);  // Handle firmware update (part 1)
void handleUpdate2(void);  // Handle firmware update (part 2)
void handleDirList(void);  // Show a list of files on the device
void handleNotFound(void); // Handle requests for pages that don't exist
void handleRedirect(String);        // Redirect to another page (String version)
void handleRedirect(const char *);  // Redirect to another page (char* version)
bool handleStaticFile(String);      // Serve a static file (String version)
bool handleStaticFile(const char *); // Serve a static file (char* version)
void handleJSON();         // Handle JSON data for configuration

#endif // _WEBINTERFACE_H_