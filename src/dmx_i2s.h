#ifndef _DMX_I2S_H_
#define _DMX_I2S_H_

#include "dmx_output.h"
#include <I2S.h>
#include <i2s_reg.h>

#define I2S_PIN 3

class DmxI2s : public DmxOutput {
public:
  DmxI2s(bool useSuperSafeTiming = false);
  virtual ~DmxI2s();
  
  // Initialize I2S for DMX output
  void begin() override;
  
  // Send DMX data over I2S
  void sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) override;
  
  // Get packets per second
  float getPacketsPerSecond() override;
  
  // Flip byte order for I2S transmission
  static uint8_t flipByte(uint8_t c);

private:
  struct I2sPacket {
    uint16_t* mark_before_break;
    uint16_t* space_for_break;
    uint16_t mark_after_break;
    uint16_t* dmx_bytes;
  };

  void initPacket();
  void freePacket();
  
  I2sPacket packet;
  bool superSafeTiming;
  unsigned long packetCounter;
  unsigned long lastPacketTime;
  uint16_t mbbSize;
  uint16_t sfbSize;
};

#endif // _DMX_I2S_H_