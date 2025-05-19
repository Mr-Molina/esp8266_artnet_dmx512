#ifndef _DMX_I2S_H_
#define _DMX_I2S_H_

#include "dmx_output.h"
#include <I2S.h>
#include <i2s_reg.h>

// ================================================================
// WHAT IS THIS FILE?
// This file defines the DmxI2s class, which sends DMX lighting
// control data using the I2S (Inter-IC Sound) method.
// ================================================================

// The GPIO pin number that we'll use for I2S output
// ESP8266 I2S is only available on GPIO3 (RX pin)
#define I2S_PIN 3

// This class implements the DmxOutput interface using I2S
// I2S stands for Inter-IC Sound and is normally used for audio,
// but we can use it for DMX because it's very precise with timing!
class DmxI2s : public DmxOutput
{
public:
  // Constructor: Creates a new DmxI2s object
  // Parameter:
  //   useSuperSafeTiming: If true, uses extra-safe timing for picky lights
  DmxI2s(bool useSuperSafeTiming = false);

  // Destructor: Cleans up when we're done with the DmxI2s
  virtual ~DmxI2s();

  // Initialize the I2S hardware for DMX output
  // This sets up the I2S system at the right speed and format
  void begin() override;

  // Send DMX lighting control data over I2S to the lights
  // Parameters:
  //   data: The array of lighting values (brightness, colors, etc.)
  //   length: How many values are in the data array
  //   maxChannels: The maximum number of channels to send
  void sendDmxData(uint8_t *data, uint16_t length, uint16_t maxChannels) override;

  // Get how many DMX packets are being sent per second
  // This tells you how smoothly your lights will respond
  float getPacketsPerSecond() override;

  // Flip the order of bits in a byte for I2S transmission
  // I2S sends the Most Significant Bit first, but DMX needs Least Significant Bit first
  // This function reverses the order of bits in a byte (like reading it backwards)
  static uint8_t flipByte(uint8_t c);

private:
  // This structure defines the parts of a DMX packet for I2S transmission
  struct I2sPacket
  {
    uint16_t *mark_before_break; // The high signal before the break
    uint16_t *space_for_break;   // The break signal (all bits low)
    uint16_t mark_after_break;   // The Mark After Break (MAB) signal
    uint16_t *dmx_bytes;         // The actual DMX data bytes
  };

  // Initialize the packet structure and allocate memory
  void initPacket();

  // Free the memory used by the packet structure
  void freePacket();

  // Member variables
  I2sPacket packet;             // The packet structure
  bool superSafeTiming;         // Whether to use extra-safe timing
  unsigned long packetCounter;  // How many packets we've sent
  unsigned long lastPacketTime; // When we last sent a packet
  uint16_t mbbSize;             // Size of the mark before break
  uint16_t sfbSize;             // Size of the space for break
};

#endif // _DMX_I2S_H_