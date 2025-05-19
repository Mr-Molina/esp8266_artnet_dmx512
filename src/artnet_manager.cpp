#include "artnet_manager.h"

// Initialize the static instance pointer to null (empty)
ArtnetManager *ArtnetManager::instance = nullptr;

// Constructor: Sets up a new ArtnetManager with all counters at zero
ArtnetManager::ArtnetManager()
    : packetCounter(0), frameCounter(0), lastFrameTime(0), framesPerSecond(0)
{
  // Save a reference to this instance so the static callback can find it
  instance = this;
}

// Destructor: Cleans up when we're done with the ArtnetManager
ArtnetManager::~ArtnetManager()
{
  // Nothing to clean up right now
}

// Start the Art-Net system so it can receive data over WiFi
void ArtnetManager::begin()
{
  // Initialize the Art-Net library
  artnet.begin();
}

// Check for and process any new Art-Net data packets
void ArtnetManager::read()
{
  // Ask the Art-Net library to check for new data
  artnet.read();
  
  // Count this frame for our statistics
  frameCounter++;
}

// Set up which function should be called when new DMX data arrives
void ArtnetManager::setDmxCallback(ArtnetDmxCallback callback)
{
  // Save the user's callback function
  userCallback = callback;
  
  // Tell the Art-Net library to use our static callback function
  // (which will then call the user's function)
  artnet.setArtDmxCallback(artnetDmxStaticCallback);
}

// This function gets called by the Art-Net library when data arrives
void ArtnetManager::artnetDmxStaticCallback(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t *data)
{
  // Make sure we have a valid instance to work with
  if (instance)
  {
    // Count this packet for our statistics
    instance->packetCounter++;
    
    // If the user set up a callback function, call it with the data
    if (instance->userCallback)
    {
      instance->userCallback(universe, length, sequence, data);
    }
  }
}

// Get the total number of Art-Net packets received
uint32_t ArtnetManager::getPacketCounter() const
{
  return packetCounter;
}

// Get how many Art-Net frames are being received per second
float ArtnetManager::getFramesPerSecond() const
{
  return framesPerSecond;
}

// Update the statistics (like frames per second)
void ArtnetManager::updateStatistics()
{
  // Get the current time in milliseconds
  unsigned long now = millis();
  
  // Calculate how much time has passed since our last update
  unsigned long elapsed = now - lastFrameTime;

  // Only update statistics if:
  // 1. At least 1 second has passed (1000 milliseconds)
  // 2. We've received at least 100 frames (to get a good average)
  if (elapsed > 1000 && frameCounter > 100)
  {
    // Calculate frames per second:
    // (frames ÷ milliseconds) × 1000 = frames per second
    framesPerSecond = 1000.0f * frameCounter / elapsed;
    
    // Reset the frame counter for the next calculation
    frameCounter = 0;
    
    // Remember when we did this calculation
    lastFrameTime = now;
  }
}