// SineWave.h

#pragma once
#include "WaveGenerator.h"
#include <math.h>

class SineWave : public WaveGenerator {
  public:
    void generateWavetable(float* table) override {
      for (int i = 0; i < WAVETABLE_SIZE; i++) {
        table[i] = sinf(twoPi * i / WAVETABLE_SIZE);
      }
    }

    const char* getName() override {
      return "Sine";
    }

    float getVolumeScale() override {
      return 8000.0f;
    }
};