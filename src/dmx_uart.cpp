#include "dmx_uart.h"

DmxUart::DmxUart() : packetCounter(0), lastPacketTime(0), useSerialBreak(true) {
}

DmxUart::~DmxUart() {
}

void DmxUart::begin() {
  // Initialize Serial1 for DMX output
  Serial1.begin(250000, SERIAL_8N2);
}

void DmxUart::sendSerialBreak() {
  // Switch to another baud rate
  Serial1.flush();
  Serial1.begin(90000, SERIAL_8N2);
  while (Serial1.available())
    Serial1.read();
  
  // Send the break as a "slow" byte
  Serial1.write(0);
  
  // Switch back to the original baud rate
  Serial1.flush();
  Serial1.begin(250000, SERIAL_8N2);
  while (Serial1.available())
    Serial1.read();
}

void DmxUart::sendLowLevelBreak() {
  // Send break using low-level code
  SET_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_TXD_BRK);
  delayMicroseconds(DMX_BREAK);
  CLEAR_PERI_REG_MASK(UART_CONF0(SEROUT_UART), UART_TXD_BRK);
  delayMicroseconds(DMX_MAB);
}

void DmxUart::sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) {
  // Send DMX break
  if (useSerialBreak) {
    sendSerialBreak();
  } else {
    sendLowLevelBreak();
  }
  
  // Send start byte
  Serial1.write(0);
  
  // Send DMX data
  uint16_t channelsToSend = min(length, maxChannels);
  for (uint16_t i = 0; i < channelsToSend; i++) {
    Serial1.write(data[i]);
  }
  
  // Update packet counter
  packetCounter++;
  unsigned long now = millis();
  if (now - lastPacketTime > 1000) {
    lastPacketTime = now;
  }
}

float DmxUart::getPacketsPerSecond() {
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