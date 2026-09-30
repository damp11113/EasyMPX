#include "Resampler.h"

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

} // namespace

Resampler::Resampler(int channels, double inRate, double outRate, int taps, int phases)
    : channels(channels), taps(taps), phases(phases), historyFrames(0) {
    baseStep = inRate / outRate;
    step = baseStep;

    // Cutoff relative to the input rate, keep a guard band below the lower Nyquist.
    const double cutoff = 0.5 * std::min(1.0, outRate / inRate) * 0.92;
    const double beta = 8.0;
    const double half = taps / 2.0;
    const double i0beta = besselI0(beta);

    table.resize(static_cast<size_t>(phases + 1) * taps);
    for (int p = 0; p <= phases; ++p) {
        const double frac = static_cast<double>(p) / phases;
        double sum = 0.0;
        for (int m = 0; m < taps; ++m) {
            const double d = (m - taps / 2 + 1) - frac;
            const double x = 2.0 * cutoff * d;
            const double sinc = (std::fabs(x) < 1e-12) ? 1.0 : std::sin(kPi * x) / (kPi * x);
            const double r = d / half;
            const double window = (std::fabs(r) >= 1.0) ? 0.0 : besselI0(beta * std::sqrt(1.0 - r * r)) / i0beta;
            const double v = 2.0 * cutoff * sinc * window;
            table[static_cast<size_t>(p) * taps + m] = static_cast<float>(v);
            sum += v;
        }
        for (int m = 0; m < taps; ++m) {
            table[static_cast<size_t>(p) * taps + m] = static_cast<float>(table[static_cast<size_t>(p) * taps + m] / sum);
        }
    }

    // Pre-roll with silence so the first output sample lines up with the first input sample.
    history.reserve(static_cast<size_t>(65536) * channels);
    pushSilence(taps / 2 - 1);
    pos = taps / 2 - 1;
}

void Resampler::setRatioCorrection(double correction) {
    step = baseStep * (1.0 + correction);
}

size_t Resampler::framesNeeded(size_t outFrames) const {
    if (outFrames == 0) return 0;
    const double last = pos + static_cast<double>(outFrames - 1) * step;
    // One frame of slack covers rounding differences with the incremental position in pull().
    const long long needIndex = static_cast<long long>(std::floor(last)) + taps / 2 + 1;
    const long long need = needIndex + 1 - static_cast<long long>(historyFrames);
    return need > 0 ? static_cast<size_t>(need) : 0;
}

void Resampler::push(const float* frames, size_t count) {
    history.insert(history.end(), frames, frames + count * channels);
    historyFrames += count;
}

void Resampler::pushSilence(size_t count) {
    history.insert(history.end(), count * channels, 0.0f);
    historyFrames += count;
}

size_t Resampler::pull(float* out, size_t outFrames) {
    size_t produced = 0;
    while (produced < outFrames) {
        const long long i0 = static_cast<long long>(std::floor(pos));
        if (i0 + taps / 2 >= static_cast<long long>(historyFrames)) break;

        const double phasePos = (pos - static_cast<double>(i0)) * phases;
        const int p = std::min(static_cast<int>(phasePos), phases - 1);
        const float t = static_cast<float>(phasePos - p);
        const float* c0 = &table[static_cast<size_t>(p) * taps];
        const float* c1 = c0 + taps;
        const float* src = &history[static_cast<size_t>(i0 - taps / 2 + 1) * channels];

        for (int ch = 0; ch < channels; ++ch) {
            float acc0 = 0.0f, acc1 = 0.0f;
            for (int m = 0; m < taps; ++m) {
                const float s = src[m * channels + ch];
                acc0 += c0[m] * s;
                acc1 += c1[m] * s;
            }
            out[produced * channels + ch] = acc0 + (acc1 - acc0) * t;
        }
        ++produced;
        pos += step;
    }

    // Drop input frames that no future output sample can reach.
    const long long keepFrom = static_cast<long long>(std::floor(pos)) - taps / 2 + 1;
    if (keepFrom > 0) {
        const size_t drop = std::min(static_cast<size_t>(keepFrom), historyFrames);
        history.erase(history.begin(), history.begin() + drop * channels);
        historyFrames -= drop;
        pos -= static_cast<double>(drop);
    }
    return produced;
}
