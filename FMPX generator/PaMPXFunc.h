#ifndef PAMPXFUNC_H
#define PAMPXFUNC_H
#define M_PI 3.14159265358979323846
#include <cmath>
#include <vector>

class SignalGenerator {
public:
    static void GenerateSineWave(float* buffer, unsigned long frames, float frequency, float amplitude, int samplerate=192000);
};



void mix(float* mixedBuffer, const float* buffers[], int numBuffers, int frames);

#endif
