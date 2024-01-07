#ifndef PAMPXFUNC_H
#define PAMPXFUNC_H

#include <cmath>

void generateSineWave(float* buffer, int frames, int sampleRate, float frequency, float amplitude = 0.5f);

void mix(float* mixedBuffer, const float* buffers[], int numBuffers, int frames);

#endif
