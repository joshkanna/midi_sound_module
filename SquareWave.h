// SquareWave.h

#pragma once
#include "WaveGenerator.h"
#include <math.h>

class SquareWave : public WaveGenerator {
  public:
    void generateWavetable(float* table) override {
      for (int i = 0; i < WAVETABLE_SIZE; i++) {
        table[i] = (i < WAVETABLE_SIZE/2) ? 1.0f : -1.0f;
      }
    }

    const char* getName() override {
      return "Square";
    }

    float getVolumeScale() override {
      return 4000.0f;
    }
};