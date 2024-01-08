#include "PaMPXFunc.h"
#include <cmath>
#include <vector>

void SignalGenerator::GenerateSineWave(float* buffer, unsigned long frames, float frequency, float amplitude, int samplerate) {
    const double phaseIncrement = frequency / samplerate;
    const double cycles = frames / samplerate; // Number of cycles in the buffer
    double phase = 0.0;

    for (unsigned long i = 0; i < frames; ++i) {
        buffer[i] = amplitude * sin(2.0 * M_PI * phase);

        phase += phaseIncrement;


    }
}

void mix(float* mixedBuffer, const float* buffers[], int numBuffers, int frames) {
    for (int i = 0; i < frames; ++i) {
        mixedBuffer[i] = 0.0f; // Initialize mixed buffer with zeros
        for (int j = 0; j < numBuffers; ++j) {
            mixedBuffer[i] += buffers[j][i]; // Accumulate samples from each buffer
        }
    }
}