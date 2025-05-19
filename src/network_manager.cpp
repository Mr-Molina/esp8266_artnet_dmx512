#include "network_manager.h"

// Constructor: Sets up a new NetworkManager with the given hostname
NetworkManager::NetworkManager(const char* hostname)
  : hostname(hostname), mdnsStarted(false) {
  // Initialize with mDNS not started yet
}

// Destructor: Cleans up when we're done with the NetworkManager
NetworkManager::~NetworkManager() {
  // If we started the mDNS service, shut it down properly
  if (mdnsStarted) {
    MDNS.end();
  }
}

// Initialize the WiFi connection
bool NetworkManager::begin(bool standaloneMode, const char* password) {
  // Step 1: Set the device's hostname on the network
  // This is how other devices will see it
  WiFi.hostname(hostname);
  
  // Step 2: Configure the access point settings
  // These settings are used when the device creates its own WiFi network
  // IP address: 192.168.1.1
  // Gateway: 192.168.1.1
  // Subnet mask: 255.255.255.0
  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  
  // Step 3: Configure blocking mode based on standalone mode
  // In standalone mode, we don't want to block the main program while waiting for WiFi
  if (standaloneMode) {
    wifiManager.setConfigPortalBlocking(false);
  }
  
  // Step 4: Connect to WiFi or start the configuration portal
  bool connected;
  if (password) {
    // If a password was provided, use it
    connected = wifiManager.autoConnect(hostname, password);
  } else {
    // Otherwise, use an open network
    connected = wifiManager.autoConnect(hostname);
  }
  
  // Return whether we connected successfully
  return connected;
}

// Process ongoing network tasks
void NetworkManager::process() {
  // Let the WiFiManager do its regular processing
  // This keeps the WiFi connection healthy
  wifiManager.process();
}

// Check if we're connected to WiFi
bool NetworkManager::isConnected() const {
  // WL_CONNECTED is a constant that means "successfully connected to WiFi"
  return WiFi.status() == WL_CONNECTED;
}

// Start the mDNS service
bool NetworkManager::startMDNS() {
  // Only start mDNS if we're connected to WiFi
  if (isConnected()) {
    // Try to start the mDNS service with our hostname
    mdnsStarted = MDNS.begin(hostname);
    
    // If mDNS started successfully, advertise our web server
    if (mdnsStarted) {
      // This tells other devices we have an HTTP server on port 80
      MDNS.addService("http", "tcp", 80);
    }
    
    // Return whether we started successfully
    return mdnsStarted;
  }
  
  // If we're not connected to WiFi, we can't start mDNS
  return false;
}

// Reset all WiFi settings and start the configuration portal
void NetworkManager::resetAndStartConfigPortal() {
  // Step 1: Reset all saved WiFi settings
  // This erases any saved networks and passwords
  wifiManager.resetSettings();
  
  // Step 2: Configure the access point settings
  // These settings are used when the device creates its own WiFi network
  wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  
  // Step 3: Start the configuration portal
  // This creates a WiFi network with our hostname that you can connect to
  // Once connected, you'll be redirected to a page where you can set up WiFi
  wifiManager.startConfigPortal(hostname);
}