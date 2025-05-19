#ifndef _ARTNET_MANAGER_H_
#define _ARTNET_MANAGER_H_

#include <ArtnetWifi.h>
#include <cstdint>
#include <functional>

// ================================================================
// WHAT IS THIS FILE?
// This file defines the ArtnetManager class, which handles receiving
// lighting control data over WiFi using the Art-Net protocol.
// ================================================================

// This defines a special function type that gets called when DMX data arrives
// It's like setting up a doorbell - when data arrives, this function rings!
// Parameters:
//   universe: Which group of lights to control (like a channel on TV)
//   length: How many lights/channels are in the data
//   sequence: A number that helps keep track of the order of messages
//   data: The actual lighting control values (brightness, colors, etc.)
typedef std::function<void(uint16_t, uint16_t, uint8_t, uint8_t *)> ArtnetDmxCallback;

class ArtnetManager
{
public:
  // Constructor: Creates a new ArtnetManager
  ArtnetManager();
  
  // Destructor: Cleans up when the ArtnetManager is no longer needed
  ~ArtnetManager();

  // Starts the Art-Net system so it can receive data
  void begin();

  // Checks for and processes any new Art-Net data packets that have arrived
  void read();

  // Sets up which function should be called when new DMX data arrives
  // This is like telling the doorbell which sound to make when pressed
  void setDmxCallback(ArtnetDmxCallback callback);

  // --- STATISTICS FUNCTIONS ---
  
  // Returns how many Art-Net packets have been received in total
  uint32_t getPacketCounter() const;
  
  // Returns how many Art-Net frames are being received per second
  // (This tells you how smoothly your lights will respond)
  float getFramesPerSecond() const;

  // Updates the statistics (like frames per second)
  // Should be called regularly to keep stats accurate
  void updateStatistics();

private:
  // The actual Art-Net library that does the network communication
  ArtnetWifi artnet;
  
  // The function that will be called when DMX data arrives
  ArtnetDmxCallback userCallback;
  
  // Counters for tracking statistics
  uint32_t packetCounter;     // Total packets received
  uint32_t frameCounter;      // Frames since last calculation
  unsigned long lastFrameTime; // When we last calculated FPS
  float framesPerSecond;      // Current frames per second rate
  
  // This special function gets called by the Art-Net library when data arrives
  // It then calls your custom function that was set with setDmxCallback
  static void artnetDmxStaticCallback(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data);
  
  // A pointer to the current instance of ArtnetManager
  // This is needed because the static callback needs to access the non-static members
  static ArtnetManager *instance;
};

#endif // _ARTNET_MANAGER_H_