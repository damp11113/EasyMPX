#include <cmath>

void generateSineWave(float* buffer, int frames, int sampleRate, float frequency, float amplitude = 0.5f) {
    const float twoPi = 2.0f * 3.14159f;
    float phaseIncrement = (frequency / sampleRate) * twoPi;
    float phase = 0.0f;

    for (int i = 0; i < frames; ++i) {
        buffer[i] = amplitude * sin(phase);
        phase += phaseIncrement;
        if (phase > twoPi) {
            phase -= twoPi;
        }
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