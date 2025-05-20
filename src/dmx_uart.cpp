#include "dmx_uart.h"

// Constructor: Sets up a new DmxUart with all counters at zero
DmxUart::DmxUart() : packetCounter(0), lastPacketTime(0)
{
  // Initialize SoftwareSerial for DMX output
  dmxSerial = new SoftwareSerial(255, DMX_TX_PIN); // RX pin not used (255), TX on GPIO14
}

// Destructor: Cleans up when we're done with the DmxUart
DmxUart::~DmxUart()
{
  // Clean up the software serial instance
  if (dmxSerial)
  {
    delete dmxSerial;
    dmxSerial = nullptr;
  }
}

// Initialize the UART hardware for DMX output
void DmxUart::begin()
{
  // Initialize Software Serial for DMX output with these settings:
  // - 250,000 bits per second (this is the standard DMX speed)
  // - 8 data bits (each DMX channel value is 8 bits)
  // - No parity bit (DMX doesn't use parity checking)
  // - 2 stop bits (DMX standard requires 2 stop bits)
  dmxSerial->begin(250000);

  // Configure the pin as output
  pinMode(DMX_TX_PIN, OUTPUT);
  digitalWrite(DMX_TX_PIN, HIGH); // Idle state is high

  Serial.print("DMX UART initialized on pin ");
  Serial.println(DMX_TX_PIN);
}

// Send the DMX break signal using the serial method
void DmxUart::sendSerialBreak()
{
  // For software serial, we'll manually create a break by pulling the line low
  digitalWrite(DMX_TX_PIN, LOW);
  delayMicroseconds(DMX_BREAK); // Hold low for break time
  digitalWrite(DMX_TX_PIN, HIGH);
  delayMicroseconds(DMX_MAB); // Hold high for Mark After Break time
}

// No longer needed - removed unused method

// Send DMX lighting control data over UART to the lights
void DmxUart::sendDmxData(uint8_t *data, uint16_t length, uint16_t maxChannels)
{
  noInterrupts(); // Disable interrupts for timing critical section

  // Send the DMX break and mark after break
  sendSerialBreak();
  
  // Send the DMX start code (always 0)
  dmxSerial->write(0);
  
  // Wait a bit longer after the start code for better compatibility
  delayMicroseconds(10);

  // Send each channel value
  uint16_t channelsToSend = min(length, maxChannels);
  for (uint16_t i = 0; i < channelsToSend; i++)
  {
    dmxSerial->write(data[i]);
    
    // Add a small delay between bytes for more reliable transmission
    // This helps ensure all receivers can process each channel
    delayMicroseconds(8);
  }

  interrupts(); // Re-enable interrupts

  packetCounter++;
  unsigned long now = millis();
  if (now - lastPacketTime > 1000)
  {
    lastPacketTime = now;
  }
}

// Get how many DMX packets are being sent per second
float DmxUart::getPacketsPerSecond()
{
  // Get the current time
  unsigned long now = millis();

  // Calculate how much time has passed since our last calculation
  unsigned long elapsed = now - lastPacketTime;

  // Only calculate if:
  // 1. Some time has passed
  // 2. We've sent at least one packet
  if (elapsed > 0 && packetCounter > 0)
  {
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