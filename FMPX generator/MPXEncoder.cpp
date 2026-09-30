#include "MPXEncoder.h"

#include <algorithm>
#include <cmath>

namespace {

const double kPi = 3.14159265358979323846;

double besselI0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < sum * 1e-12) break;
    }
    return sum;
}

inline float clip(float x, float limit) {
    return std::max(-limit, std::min(limit, x));
}

} // namespace

LowpassFIR::LowpassFIR(int sampleRate, double passHz, double stopHz, double attenuationDb) : index(0) {
    // Kaiser window design
    const double transition = 2.0 * kPi * (stopHz - passHz) / sampleRate;
    int n = static_cast<int>(std::ceil((attenuationDb - 8.0) / (2.285 * transition))) + 1;
    if (n % 2 == 0) ++n;
    const double beta = attenuationDb > 50.0 ? 0.1102 * (attenuationDb - 8.7)
                                             : 0.5842 * std::pow(attenuationDb - 21.0, 0.4) + 0.07886 * (attenuationDb - 21.0);
    const double fc = (passHz + stopHz) / 2.0 / sampleRate;
    const double mid = (n - 1) / 2.0;
    const double i0beta = besselI0(beta);

    taps = static_cast<size_t>(n);
    coeffs.resize(taps);
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double d = i - mid;
        const double sinc = (d == 0.0) ? 2.0 * fc : std::sin(2.0 * kPi * fc * d) / (kPi * d);
        const double r = d / mid;
        const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0beta;
        coeffs[i] = static_cast<float>(sinc * w);
        sum += coeffs[i];
    }
    for (float& c : coeffs) c = static_cast<float>(c / sum);
    buffer.assign(taps * 2, 0.0f);
}

float LowpassFIR::process(float x) {
    buffer[index] = x;
    buffer[index + taps] = x;
    const float* p = &buffer[index];
    const size_t half = taps / 2;
    float acc = coeffs[half] * p[half];
    for (size_t k = 0; k < half; ++k) acc += coeffs[k] * (p[k] + p[taps - 1 - k]);
    index = (index == 0) ? taps - 1 : index - 1;
    return acc;
}

MPXEncoder::MPXEncoder(const MPXConfig& cfg)
    : config(cfg),
      lowpassL(cfg.sampleRate, 15000.0, 16500.0, 70.0),
      lowpassR(cfg.sampleRate, 15000.0, 16500.0, 70.0),
      pilotAccumulator(0), preemphCoeff(0.0f), preemphGain(1.0f), prevL(0.0f), prevR(0.0f) {
    if (config.rds) {
        config.rdsConfig.stereo = config.stereo;
        rdsEncoder.reset(new RDSEncoder(config.rdsConfig, config.sampleRate));
    }
    if (config.preemphasisUs > 0.0) {
        // y[n] = (x[n] - a * x[n-1]) / (1 - a) matches 1 + s*tau well below 15 kHz at MPX rates.
        const double a = std::exp(-1.0 / (config.preemphasisUs * 1e-6 * config.sampleRate));
        preemphCoeff = static_cast<float>(a);
        preemphGain = static_cast<float>(1.0 / (1.0 - a));
    }
}

void MPXEncoder::process(const float* stereo, float* mpx, size_t frames) {
    const int64_t rate = config.sampleRate;
    const float audioLevel = static_cast<float>(config.audioLevel);
    const float pilotLevel = static_cast<float>(config.pilotLevel);
    const float rdsLevel = static_cast<float>(config.rdsLevel);
    const float gain = static_cast<float>(config.outputGain);
    const double phaseScale = 2.0 * kPi / static_cast<double>(rate);

    for (size_t i = 0; i < frames; ++i) {
        float l = stereo[i * 2];
        float r = stereo[i * 2 + 1];

        if (preemphCoeff > 0.0f) {
            const float pl = (l - preemphCoeff * prevL) * preemphGain;
            const float pr = (r - preemphCoeff * prevR) * preemphGain;
            prevL = l;
            prevR = r;
            l = pl;
            r = pr;
        }
        if (config.compositeClipper) {
            // Clip before the lowpass so the clipping products above 15 kHz get filtered out.
            l = clip(l, 1.0f);
            r = clip(r, 1.0f);
        }
        l = lowpassL.process(l);
        r = lowpassR.process(r);

        // Exact phase: pilotAccumulator / rate is the 19 kHz phase in cycles, no drift.
        const double theta = phaseScale * static_cast<double>(pilotAccumulator);
        pilotAccumulator += 19000;
        if (pilotAccumulator >= rate) pilotAccumulator -= rate;

        float out;
        if (config.stereo) {
            const float mono = 0.5f * (l + r);
            const float side = 0.5f * (l - r);
            out = audioLevel * (mono + side * static_cast<float>(std::sin(2.0 * theta))) +
                  pilotLevel * static_cast<float>(std::sin(theta));
        } else {
            out = audioLevel * 0.5f * (l + r);
        }
        if (rdsEncoder) {
            out += rdsLevel * rdsEncoder->nextSample() * static_cast<float>(std::sin(3.0 * theta));
        }

        out *= gain;
        if (config.compositeClipper) out = clip(out, 1.0f);
        mpx[i] = out;
    }
}
