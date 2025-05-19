#ifndef _DMX_UART_H_
#define _DMX_UART_H_

#include "dmx_output.h"

#include "c_types.h"
#include "eagle_soc.h"
#include "uart_register.h"

// DMX minimum timings per E1.11
#define DMX_BREAK 92
#define DMX_MAB 12
#define SEROUT_UART 1

class DmxUart : public DmxOutput {
public:
  DmxUart();
  virtual ~DmxUart();
  
  // Initialize UART for DMX output
  void begin() override;
  
  // Send DMX data over UART
  void sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) override;
  
  // Get packets per second
  float getPacketsPerSecond() override;

private:
  // Send DMX break using serial method
  void sendSerialBreak();
  
  // Send DMX break using low-level method
  void sendLowLevelBreak();
  
  unsigned long packetCounter;
  unsigned long lastPacketTime;
  bool useSerialBreak;
};

#endif // _DMX_UART_H_