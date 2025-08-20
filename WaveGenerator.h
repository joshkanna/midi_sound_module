// WaveGenerator.h
#pragma once
#include <math.h>

#define WAVETABLE_SIZE 1024

const float twoPi = 2 * 3.14159265359f;

class WaveGenerator {
  public:
    virtual ~WaveGenerator() {}

    virtual void generateWavetable(float* table) = 0;
    virtual const char* getName() = 0;
    virtual float getVolumeScale() = 0;

    virtual float getPhaseOffset() { return 0.0f; }
    virtual bool useCustomPhase() { return false; }
    virtual float generateSample(float phase) { return 0.0f; } // For real-time generation
};

