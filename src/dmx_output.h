#ifndef _DMX_OUTPUT_H_
#define _DMX_OUTPUT_H_

#include <Arduino.h>
#include <cstdint>

// Abstract base class for DMX output methods
class DmxOutput {
public:
  virtual ~DmxOutput() {}
  
  // Initialize the DMX output
  virtual void begin() = 0;
  
  // Send DMX data
  virtual void sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) = 0;
  
  // Get packets per second
  virtual float getPacketsPerSecond() = 0;
};

#endif // _DMX_OUTPUT_H_