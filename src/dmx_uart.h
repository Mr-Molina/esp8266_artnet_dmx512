#ifndef _DMX_UART_H_
#define _DMX_UART_H_

#include "dmx_output.h"

#include "c_types.h"
#include "eagle_soc.h"
#include "uart_register.h"
#include <SoftwareSerial.h>

// ================================================================
// WHAT IS THIS FILE?
// This file defines the DmxUart class, which sends DMX lighting
// control data using the UART (serial communication) method.
// ================================================================

// DMX timing requirements from the official DMX512 standard (E1.11)
// These are measured in microseconds (millionths of a second)
#define DMX_BREAK 200  // The "break" signal must be at least 92 microseconds, using 200 for reliability
#define DMX_MAB 20     // The "Mark After Break" must be at least 12 microseconds

// Define the pin to use for DMX output
#define DMX_TX_PIN 14  // Using GPIO14 for DMX output

// This class implements the DmxOutput interface using UART (serial communication)
// UART stands for Universal Asynchronous Receiver/Transmitter
// It's like a digital walkie-talkie that sends data one bit at a time
class DmxUart : public DmxOutput {
public:
  // Constructor: Creates a new DmxUart object
  DmxUart();
  
  // Destructor: Cleans up when we're done with the DmxUart
  virtual ~DmxUart();
  
  // Initialize the UART hardware for DMX output
  // This sets up the serial communication at the right speed and format
  void begin() override;
  
  // Send DMX lighting control data over UART to the lights
  // Parameters:
  //   data: The array of lighting values (brightness, colors, etc.)
  //   length: How many values are in the data array
  //   maxChannels: The maximum number of channels to send
  void sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) override;
  
  // Get how many DMX packets are being sent per second
  // This tells you how smoothly your lights will respond
  float getPacketsPerSecond() override;

private:
  // Send the DMX break signal using the serial method
  // A "break" is a special signal that tells the lights "new data is coming"
  void sendSerialBreak();
  
  // Software serial instance for DMX output on custom pin
  SoftwareSerial* dmxSerial;
  
  // Variables to track statistics
  unsigned long packetCounter;   // How many packets we've sent
  unsigned long lastPacketTime;  // When we last sent a packet
};

#endif // _DMX_UART_H_