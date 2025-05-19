#ifndef _DMX_OUTPUT_H_
#define _DMX_OUTPUT_H_

#include <Arduino.h>
#include <cstdint>

// ================================================================
// WHAT IS THIS FILE?
// This file defines the DmxOutput class, which is a blueprint for
// different ways to send DMX lighting control data to actual lights.
// ================================================================

// This is what we call an "abstract base class" - it's like a template
// that other classes will use. It defines what functions all DMX output
// methods must have, but doesn't say exactly how they should work.
//
// Think of it like a recipe that says "add vegetables" without specifying
// which vegetables - the specific recipes (child classes) will decide that!
class DmxOutput {
public:
  // Destructor: Cleans up when we're done with the DMX output
  virtual ~DmxOutput() {}
  
  // Initialize the DMX output hardware
  // This gets everything ready to send data to the lights
  // The "= 0" means "child classes MUST implement this function"
  virtual void begin() = 0;
  
  // Send DMX lighting control data to the lights
  // Parameters:
  //   data: The array of lighting values (brightness, colors, etc.)
  //   length: How many values are in the data array
  //   maxChannels: The maximum number of channels to send
  virtual void sendDmxData(uint8_t* data, uint16_t length, uint16_t maxChannels) = 0;
  
  // Get how many DMX packets are being sent per second
  // This tells you how smoothly your lights will respond
  virtual float getPacketsPerSecond() = 0;
};

#endif // _DMX_OUTPUT_H_