#ifndef _ARTNET_MANAGER_H_
#define _ARTNET_MANAGER_H_

#include <ArtnetWifi.h>
#include <cstdint>

// Callback function type for DMX packet reception
typedef void (*ArtnetDmxCallback)(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data);

class ArtnetManager {
public:
  ArtnetManager();
  ~ArtnetManager();
  
  // Initialize ArtNet
  void begin();
  
  // Process incoming ArtNet packets
  void read();
  
  // Set callback for DMX data
  void setDmxCallback(ArtnetDmxCallback callback);
  
  // Get statistics
  uint32_t getPacketCounter() const;
  float getFramesPerSecond() const;
  
  // Update statistics
  void updateStatistics();

private:
  ArtnetWifi artnet;
  uint32_t packetCounter;
  uint32_t frameCounter;
  unsigned long lastFrameTime;
  float framesPerSecond;
};

#endif // _ARTNET_MANAGER_H_