#ifndef RESAMPLER_H
#define RESAMPLER_H

#include <cstddef>
#include <vector>

// Streaming windowed-sinc resampler with an adjustable ratio.
// Used to convert file sample rates to the MPX rate, and in realtime mode to
// compensate the clock drift between the input and output soundcards.
class Resampler {
public:
    Resampler(int channels, double inRate, double outRate, int taps = 64, int phases = 512);

    // Fine ratio correction, e.g. +0.0001 consumes input 100 ppm faster.
    void setRatioCorrection(double correction);

    // Input frames that must be pushed before outFrames can be pulled.
    size_t framesNeeded(size_t outFrames) const;

    void push(const float* frames, size_t count);
    void pushSilence(size_t count);

    // Produces up to outFrames interleaved frames, returns the number produced.
    size_t pull(float* out, size_t outFrames);

private:
    int channels;
    int taps;
    int phases;
    double baseStep;
    double step;
    double pos;
    std::vector<float> table;
    std::vector<float> history;
    size_t historyFrames;
};

#endif
