#ifndef MPXENCODER_H
#define MPXENCODER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "RDSEncoder.h"

struct MPXConfig {
    int sampleRate = 192000;
    bool stereo = true;
    double audioLevel = 0.88;       // L/R audio share of full deviation
    double pilotLevel = 0.09;       // 19 kHz pilot, 9 %
    double rdsLevel = 0.03;         // 57 kHz RDS, 3 %
    double outputGain = 1.0;
    double preemphasisUs = 0.0;     // 0 = off, 50 (Europe/Asia) or 75 (Americas)
    bool compositeClipper = true;
    bool rds = false;
    RDSConfig rdsConfig;
};

// Symmetric FIR lowpass with a doubled circular buffer.
class LowpassFIR {
public:
    LowpassFIR(int sampleRate, double passHz, double stopHz, double attenuationDb);
    float process(float x);

private:
    std::vector<float> coeffs;
    std::vector<float> buffer;
    size_t taps;
    size_t index;
};

// Stereo audio in, FM composite (MPX) out. All oscillators run from one exact
// integer phase accumulator, so the pilot, 38 kHz and 57 kHz carriers stay
// phase locked and continuous across buffers of any size.
class MPXEncoder {
public:
    explicit MPXEncoder(const MPXConfig& config);

    // stereo: interleaved L/R frames, mpx: mono output
    void process(const float* stereo, float* mpx, size_t frames);

    RDSEncoder* rds() { return rdsEncoder.get(); }

private:
    MPXConfig config;
    LowpassFIR lowpassL;
    LowpassFIR lowpassR;
    std::unique_ptr<RDSEncoder> rdsEncoder;
    int64_t pilotAccumulator;
    float preemphCoeff;
    float preemphGain;
    float prevL;
    float prevR;
};

#endif
