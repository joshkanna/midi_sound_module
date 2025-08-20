// TriangleWave.h

#pragma once
#include "WaveGenerator.h"
#include <math.h>

class TriangleWave : public WaveGenerator {
  void generateWavetable(float* table) override {
    for (int i = 0; i < WAVETABLE_SIZE; i++){
      float position = float(i) / 1024; // convert 0-1023 to 0-1
      
      // 1st half: -1 to 1
      if (position < 0.5f) {
        table[i] = (4.0f * position) - 1.0f;
      } 
      
      // 2nd half: 1 to -1
      else {
        table[i] = 3.0f - (position * 4.0f);
      }  
    }
  }

  const char* getName()  override { 
    return "Triangle";
  }

  float getVolumeScale() override {
    return 8000.0f;
  }
};