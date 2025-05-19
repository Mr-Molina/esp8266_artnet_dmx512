#include "artnet_manager.h"

ArtnetManager::ArtnetManager()
  : packetCounter(0), frameCounter(0), lastFrameTime(0), framesPerSecond(0) {
}

ArtnetManager::~ArtnetManager() {
}

void ArtnetManager::begin() {
  artnet.begin();
}

void ArtnetManager::read() {
  artnet.read();
  frameCounter++;
}

void ArtnetManager::setDmxCallback(ArtnetDmxCallback callback) {
  artnet.setArtDmxCallback([this, callback](uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
    packetCounter++;
    if (callback) {
      callback(universe, length, sequence, data);
    }
  });
}

uint32_t ArtnetManager::getPacketCounter() const {
  return packetCounter;
}

float ArtnetManager::getFramesPerSecond() const {
  return framesPerSecond;
}

void ArtnetManager::updateStatistics() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastFrameTime;
  
  if (elapsed > 1000 && frameCounter > 100) {
    framesPerSecond = 1000.0f * frameCounter / elapsed;
    frameCounter = 0;
    lastFrameTime = now;
  }
}