#include "dmx_i2s.h"

DmxI2s::DmxI2s(bool useSuperSafeTiming) 
  : superSafeTiming(useSuperSafeTiming), 
    packetCounter(0), 
    lastPacketTime(0) {
  
  // Set sizes based on timing mode
  mbbSize = superSafeTiming ? 10 : 1;
  sfbSize = superSafeTiming ? 2 : 1;
  
  // Initialize packet structure with NULL pointers
  packet.mark_before_break = nullptr;
  packet.space_for_break = nullptr;
  packet.dmx_bytes = nullptr;
}

DmxI2s::~DmxI2s() {
  freePacket();
}

void DmxI2s::freePacket() {
  if (packet.mark_before_break) {
    delete[] packet.mark_before_break;
    packet.mark_before_break = nullptr;
  }
  
  if (packet.space_for_break) {
    delete[] packet.space_for_break;
    packet.space_for_break = nullptr;
  }
  
  if (packet.dmx_bytes) {
    delete[] packet.dmx_bytes;
    packet.dmx_bytes = nullptr;
  }
}

void DmxI2s::initPacket() {
  // Allocate memory for packet components
  packet.mark_before_break = new uint16_t[mbbSize];
  packet.space_for_break = new uint16_t[sfbSize];
  
  // Fill with appropriate values
  for (uint16_t i = 0; i < mbbSize; i++) {
    packet.mark_before_break[i] = 0xFFFF;  // All bits high
  }
  
  for (uint16_t i = 0; i < sfbSize; i++) {
    packet.space_for_break[i] = 0x0000;  // All bits low
  }
  
  // 3 bits (12us) MAB. The MAB's LSB 0 acts as the start bit (low) for the null byte
  packet.mark_after_break = (uint16_t)0b000001110;
}

void DmxI2s::begin() {
  // Initialize packet structure
  initPacket();
  
  // Configure I2S pin
  pinMode(I2S_PIN, OUTPUT);
  digitalWrite(I2S_PIN, 1);
  
  // Initialize I2S
  i2s_begin();
  
  // 250.000 baud / 32 bits = 7812
  i2s_set_rate(7812);
}

uint8_t DmxI2s::flipByte(uint8_t c) {
  // Reverse byte order because DMX expects LSB first but I2S sends MSB first
  c = ((c >> 1) & 0b01010101) | ((c << 1) & 0b10101010);
  c = ((c >> 2) & 0b00110011) | ((c << 2) & 0b11001100);
  return (c >> 4) | (c << 4);
}

void DmxI2s::sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) {
  // Ensure we have enough memory for the DMX data
  uint16_t channelsToSend = min(length, maxChannels);
  
  // Allocate or reallocate dmx_bytes if needed
  if (packet.dmx_bytes == nullptr) {
    packet.dmx_bytes = new uint16_t[channelsToSend + 1];  // +1 for start code
  }
  
  // Set start code (index 0) to 0
  packet.dmx_bytes[0] = (uint16_t)0b0000000011111110;
  
  // Fill DMX data
  for (uint16_t i = 0; i < channelsToSend; i++) {
    uint16_t hi = flipByte(data[i]);
    // Add stop bits and start bit of next byte unless there is no next byte
    uint16_t lo = (i == channelsToSend - 1) ? 0b0000000011111111 : 0b0000000011111110;
    packet.dmx_bytes[i + 1] = (hi << 8) | lo;
  }
  
  // Calculate total size in 16-bit words
  size_t totalSize = mbbSize + sfbSize + 1 + channelsToSend + 1;  // MBB + SFB + MAB + data + start code
  
  // Create a contiguous buffer for I2S
  uint16_t* buffer = new uint16_t[totalSize];
  
  // Copy data to buffer
  uint16_t offset = 0;
  
  // Mark before break
  memcpy(buffer + offset, packet.mark_before_break, mbbSize * sizeof(uint16_t));
  offset += mbbSize;
  
  // Space for break
  memcpy(buffer + offset, packet.space_for_break, sfbSize * sizeof(uint16_t));
  offset += sfbSize;
  
  // Mark after break
  buffer[offset++] = packet.mark_after_break;
  
  // DMX data (including start code)
  memcpy(buffer + offset, packet.dmx_bytes, (channelsToSend + 1) * sizeof(uint16_t));
  
  // Send data via I2S
  i2s_write_buffer((int16_t*)buffer, totalSize / 2);  // Divide by 2 for I2S frame format
  
  // Free the temporary buffer
  delete[] buffer;
  
  // Update packet counter
  packetCounter++;
  unsigned long now = millis();
  if (now - lastPacketTime > 1000) {
    lastPacketTime = now;
  }
}

float DmxI2s::getPacketsPerSecond() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastPacketTime;
  
  if (elapsed > 0 && packetCounter > 0) {
    float pps = (1000.0 * packetCounter) / elapsed;
    packetCounter = 0;
    lastPacketTime = now;
    return pps;
  }
  
  return 0.0;
}