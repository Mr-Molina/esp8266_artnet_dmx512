#ifndef _NETWORK_MANAGER_H_
#define _NETWORK_MANAGER_H_

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>

// ================================================================
// WHAT IS THIS FILE?
// This file defines the NetworkManager class, which handles all
// WiFi and network-related tasks for our lighting controller.
// ================================================================

// The NetworkManager class handles WiFi connections and network services
class NetworkManager {
public:
  // Constructor: Creates a new NetworkManager with the given hostname
  // A hostname is like a nickname for your device on the network
  NetworkManager(const char* hostname);
  
  // Destructor: Cleans up when we're done with the NetworkManager
  ~NetworkManager();
  
  // Initialize the WiFi connection
  // Parameters:
  //   standaloneMode: If true, creates its own WiFi network instead of joining one
  //   password: Optional password for the WiFi network
  // Returns:
  //   true if connected successfully, false otherwise
  bool begin(bool standaloneMode = false, const char* password = nullptr);
  
  // Process ongoing network tasks
  // This should be called regularly in the main loop
  void process();
  
  // Check if we're connected to WiFi
  // Returns:
  //   true if connected, false otherwise
  bool isConnected() const;
  
  // Start the mDNS service
  // mDNS lets you find the device by name instead of IP address
  // Returns:
  //   true if started successfully, false otherwise
  bool startMDNS();
  
  // Reset all WiFi settings and start the configuration portal
  // This is useful if you need to connect to a different WiFi network
  void resetAndStartConfigPortal();

private:
  // The name of this device on the network
  const char* hostname;
  
  // The WiFiManager object that handles connections
  WiFiManager wifiManager;
  
  // Whether the mDNS service is running
  bool mdnsStarted;
};

#endif // _NETWORK_MANAGER_H_