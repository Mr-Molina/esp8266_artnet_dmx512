#include "network_manager.h"

NetworkManager::NetworkManager(const char* hostname)
  : hostname(hostname), mdnsStarted(false) {
}

NetworkManager::~NetworkManager() {
  if (mdnsStarted) {
    MDNS.end();
  }
}

bool NetworkManager::begin(bool standaloneMode, const char* password) {
  // Set hostname
  WiFi.hostname(hostname);
  
  // Configure access point
  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  
  // Set blocking mode based on standalone mode
  if (standaloneMode) {
    wifiManager.setConfigPortalBlocking(false);
  }
  
  // Connect to WiFi or start config portal
  bool connected;
  if (password) {
    connected = wifiManager.autoConnect(hostname, password);
  } else {
    connected = wifiManager.autoConnect(hostname);
  }
  
  return connected;
}

void NetworkManager::process() {
  wifiManager.process();
}

bool NetworkManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool NetworkManager::startMDNS() {
  if (isConnected()) {
    mdnsStarted = MDNS.begin(hostname);
    if (mdnsStarted) {
      MDNS.addService("http", "tcp", 80);
    }
    return mdnsStarted;
  }
  return false;
}

void NetworkManager::resetAndStartConfigPortal() {
  wifiManager.resetSettings();
  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  wifiManager.startConfigPortal(hostname);
}