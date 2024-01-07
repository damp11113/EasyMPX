#ifndef PADSPFUNC_H
#define PADSPFUNC_H
#include <vector>

void audioCompressor(float* audioBuffer, int bufferSize, float threshold, float ratio);

struct AudioBuffer {
    float* data;
    int size;
};

struct Band {
    float frequency;
    float threshold;
    float gain;
    float ratio;
    float attackTime;
    float releaseTime;
    float currentGain;
    float envelope;
};

void processBandWithDynamicGain(float* buffer, int size, Band& band);
void multibandCompressor(AudioBuffer& audio, std::vector<Band>& bands);


struct Limiter {
    float threshold;
    float margin;
    float attackTime; // Attack time in milliseconds
    float releaseTime; // Release time in milliseconds
    float ratio;
    float currentGain; // Current gain level for dynamic changes
    float envelope; // Envelope for dynamic changes
};

void limiterProcess(float* buffer, int size, Limiter& limiter);


#endif
