// SawtoothWave.h

#pragma once
#include "WaveGenerator.h"
#include <math.h>

class SawtoothWave : public WaveGenerator {
  void generateWavetable(float* table) override {
    for (int i = 0; i < WAVETABLE_SIZE; i++) {

      float position = float(i) / 1024; // 0-1023 -> 0-1

      table[i] = (2.0f * position) - 1.0f;
    }
  }

  const char* getName() override {
    return "Sawtooth";
  }

  float getVolumeScale() override {
    return 8000.0f;
  }
};