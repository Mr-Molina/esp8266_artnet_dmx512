#include "dmx_i2s.h"

// Constructor: Sets up a new DmxI2s with timing settings
DmxI2s::DmxI2s(bool useSuperSafeTiming) 
  : superSafeTiming(useSuperSafeTiming), 
    packetCounter(0), 
    lastPacketTime(0) {
  
  // Set sizes based on timing mode
  // Super safe timing uses larger buffers for more reliable communication
  mbbSize = superSafeTiming ? 10 : 1;  // Mark Before Break size
  sfbSize = superSafeTiming ? 2 : 1;   // Space For Break size
  
  // Initialize packet structure with NULL pointers
  // (We'll allocate memory for these later in initPacket)
  packet.mark_before_break = nullptr;
  packet.space_for_break = nullptr;
  packet.dmx_bytes = nullptr;
}

// Destructor: Cleans up when we're done with the DmxI2s
DmxI2s::~DmxI2s() {
  // Free any memory we allocated
  freePacket();
}

// Free the memory used by the packet structure
void DmxI2s::freePacket() {
  // Check if each pointer exists before trying to delete it
  
  // Free the Mark Before Break buffer
  if (packet.mark_before_break) {
    delete[] packet.mark_before_break;
    packet.mark_before_break = nullptr;
  }
  
  // Free the Space For Break buffer
  if (packet.space_for_break) {
    delete[] packet.space_for_break;
    packet.space_for_break = nullptr;
  }
  
  // Free the DMX data buffer
  if (packet.dmx_bytes) {
    delete[] packet.dmx_bytes;
    packet.dmx_bytes = nullptr;
  }
}

// Initialize the packet structure and allocate memory
void DmxI2s::initPacket() {
  // Step 1: Allocate memory for packet components
  packet.mark_before_break = new uint16_t[mbbSize];
  packet.space_for_break = new uint16_t[sfbSize];
  
  // Step 2: Fill the Mark Before Break buffer with all 1s (high signal)
  // This is the idle state before we send a break
  for (uint16_t i = 0; i < mbbSize; i++) {
    packet.mark_before_break[i] = 0xFFFF;  // All bits high (16 bits of 1s)
  }
  
  // Step 3: Fill the Space For Break buffer with all 0s (low signal)
  // This is the break signal itself
  for (uint16_t i = 0; i < sfbSize; i++) {
    packet.space_for_break[i] = 0x0000;  // All bits low (16 bits of 0s)
  }
  
  // Step 4: Set up the Mark After Break (MAB) signal
  // 3 bits (12us) MAB. The MAB's LSB 0 acts as the start bit (low) for the null byte
  packet.mark_after_break = (uint16_t)0b000001110;
}

// Initialize the I2S hardware for DMX output
void DmxI2s::begin() {
  // Step 1: Initialize our packet structure
  initPacket();
  
  // Step 2: Configure the I2S output pin
  pinMode(I2S_PIN, OUTPUT);
  digitalWrite(I2S_PIN, 1);  // Start with the pin high (idle state)
  
  // Step 3: Initialize the I2S hardware
  i2s_begin();
  
  // Step 4: Set the I2S data rate
  // We need 250,000 baud for DMX, but I2S sends 32 bits at a time
  // So we need 250,000 ÷ 32 = 7,812 Hz sample rate
  i2s_set_rate(7812);
}

// Flip the order of bits in a byte for I2S transmission
uint8_t DmxI2s::flipByte(uint8_t c) {
  // DMX expects Least Significant Bit (LSB) first
  // But I2S sends Most Significant Bit (MSB) first
  // So we need to reverse the bit order
  
  // Step 1: Swap adjacent bits (positions 0&1, 2&3, 4&5, 6&7)
  c = ((c >> 1) & 0b01010101) | ((c << 1) & 0b10101010);
  
  // Step 2: Swap adjacent pairs of bits (positions 0-1&2-3, 4-5&6-7)
  c = ((c >> 2) & 0b00110011) | ((c << 2) & 0b11001100);
  
  // Step 3: Swap the two halves of the byte (positions 0-3&4-7)
  return (c >> 4) | (c << 4);
}

// Send DMX lighting control data over I2S to the lights
void DmxI2s::sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) {
  // Step 1: Determine how many channels to send
  uint16_t channelsToSend = min(length, maxChannels);
  
  // Step 2: Allocate memory for DMX data if needed
  if (packet.dmx_bytes == nullptr) {
    // +1 for the start code (always channel 0 in DMX)
    packet.dmx_bytes = new uint16_t[channelsToSend + 1];
  }
  
  // Step 3: Set the start code (always 0 for standard DMX)
  // Format: 8 data bits + stop bits and next start bit
  packet.dmx_bytes[0] = (uint16_t)0b0000000011111110;
  
  // Step 4: Fill the DMX data buffer with channel values
  for (uint16_t i = 0; i < channelsToSend; i++) {
    // Flip the byte order for I2S transmission
    uint16_t hi = flipByte(data[i]);
    
    // Add stop bits and start bit of next byte
    // If this is the last byte, use all 1s for the stop bits
    // Otherwise, include a 0 for the start bit of the next byte
    uint16_t lo = (i == channelsToSend - 1) ? 0b0000000011111111 : 0b0000000011111110;
    
    // Combine the data byte with the stop/start bits
    packet.dmx_bytes[i + 1] = (hi << 8) | lo;
  }
  
  // Step 5: Calculate the total size of our DMX packet
  size_t totalSize = mbbSize + sfbSize + 1 + channelsToSend + 1;  // MBB + SFB + MAB + data + start code
  
  // Step 6: Create a contiguous buffer for I2S transmission
  uint16_t* buffer = new uint16_t[totalSize];
  
  // Step 7: Copy all parts of the DMX packet into the buffer
  uint16_t offset = 0;
  
  // Copy the Mark Before Break
  memcpy(buffer + offset, packet.mark_before_break, mbbSize * sizeof(uint16_t));
  offset += mbbSize;
  
  // Copy the Space For Break
  memcpy(buffer + offset, packet.space_for_break, sfbSize * sizeof(uint16_t));
  offset += sfbSize;
  
  // Copy the Mark After Break
  buffer[offset++] = packet.mark_after_break;
  
  // Copy the DMX data (including start code)
  memcpy(buffer + offset, packet.dmx_bytes, (channelsToSend + 1) * sizeof(uint16_t));
  
  // Step 8: Send the data via I2S
  // Divide by 2 because I2S frames contain 2 16-bit words
  i2s_write_buffer((int16_t*)buffer, totalSize / 2);
  
  // Step 9: Free the temporary buffer
  delete[] buffer;
  
  // Step 10: Update our statistics
  packetCounter++;
  unsigned long now = millis();
  if (now - lastPacketTime > 1000) {
    lastPacketTime = now;
  }
}

// Get how many DMX packets are being sent per second
float DmxI2s::getPacketsPerSecond() {
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