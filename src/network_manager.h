#ifndef _NETWORK_MANAGER_H_
#define _NETWORK_MANAGER_H_

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>

class NetworkManager {
public:
  NetworkManager(const char* hostname);
  ~NetworkManager();
  
  // Initialize network connection
  bool begin(bool standaloneMode = false, const char* password = nullptr);
  
  // Process network tasks
  void process();
  
  // Check if connected to WiFi
  bool isConnected() const;
  
  // Start mDNS service
  bool startMDNS();
  
  // Reset WiFi settings and start config portal
  void resetAndStartConfigPortal();

private:
  const char* hostname;
  WiFiManager wifiManager;
  bool mdnsStarted;
};

#endif // _NETWORK_MANAGER_H_