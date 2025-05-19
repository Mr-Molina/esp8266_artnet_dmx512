#include "dmx_uart.h"

// Constructor: Sets up a new DmxUart with all counters at zero
DmxUart::DmxUart() : packetCounter(0), lastPacketTime(0), useSerialBreak(true) {
  // We start with the serial break method by default
}

// Destructor: Cleans up when we're done with the DmxUart
DmxUart::~DmxUart() {
  // Nothing to clean up right now
}

// Initialize the UART hardware for DMX output
void DmxUart::begin() {
  // Initialize Serial1 for DMX output with these settings:
  // - 250,000 bits per second (this is the standard DMX speed)
  // - 8 data bits (each DMX channel value is 8 bits)
  // - No parity bit (DMX doesn't use parity checking)
  // - 2 stop bits (DMX standard requires 2 stop bits)
  Serial1.begin(250000, SERIAL_8N2);
}

// Send the DMX break signal using the serial method
void DmxUart::sendSerialBreak() {
  // A clever trick: To create a break, we temporarily slow down
  // the serial speed, which makes the bits longer
  
  // First, make sure all pending data is sent
  Serial1.flush();
  
  // Switch to a slower speed (90,000 bits per second)
  Serial1.begin(90000, SERIAL_8N2);
  
  // Clear any incoming data (just to be safe)
  while (Serial1.available())
    Serial1.read();
  
  // Send a zero byte at the slower speed
  // This creates a longer low signal that acts as our break
  Serial1.write(0);
  
  // Switch back to the normal DMX speed
  Serial1.flush();
  Serial1.begin(250000, SERIAL_8N2);
  
  // Clear any incoming data again
  while (Serial1.available())
    Serial1.read();
}

// Send the DMX break signal using a low-level hardware method
void DmxUart::sendLowLevelBreak() {
  // This method uses direct hardware control for more precise timing
  
  // Turn on the break signal in the UART hardware
  SET_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_TXD_BRK);
  
  // Wait for the required break time (92 microseconds minimum)
  delayMicroseconds(DMX_BREAK);
  
  // Turn off the break signal
  CLEAR_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_TXD_BRK);
  
  // Wait for the Mark After Break time (12 microseconds minimum)
  delayMicroseconds(DMX_MAB);
}

// Send DMX lighting control data over UART to the lights
void DmxUart::sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) {
  // Step 1: Send the DMX break signal (tells lights "new data is coming")
  if (useSerialBreak) {
    // Use the serial method
    sendSerialBreak();
  } else {
    // Use the low-level hardware method
    sendLowLevelBreak();
  }
  
  // Step 2: Send the start byte (always zero for standard DMX)
  Serial1.write(0);
  
  // Step 3: Send the actual DMX data for each channel
  // First, figure out how many channels to send (the smaller of length or maxChannels)
  uint16_t channelsToSend = min(length, maxChannels);
  
  // Then send each channel's value one by one
  for (uint16_t i = 0; i < channelsToSend; i++) {
    Serial1.write(data[i]);
  }
  
  // Step 4: Update our statistics
  packetCounter++;
  unsigned long now = millis();
  if (now - lastPacketTime > 1000) {
    // If it's been more than a second, update our time reference
    lastPacketTime = now;
  }
}

// Get how many DMX packets are being sent per second
float DmxUart::getPacketsPerSecond() {
  // Get the current time
  unsigned long now = millis();
  
  // Calculate how much time has passed since our last calculation
  unsigned long elapsed = now - lastPacketTime;
  
  // Only calculate if:
  // 1. Some time has passed
  // 2. We've sent at least one packet
  if (elapsed > 0 && packetCounter > 0) {
    // Calculate packets per second:
    // (packets ÷ milliseconds) × 1000 = packets per second
    float pps = (1000.0 * packetCounter) / elapsed;
    
    // Reset our counter for the next calculation
    packetCounter = 0;
    lastPacketTime = now;
    
    return pps;
  }
  
  // If we can't calculate yet, return zero
  return 0.0;
}