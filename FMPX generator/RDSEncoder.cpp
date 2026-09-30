#include "RDSEncoder.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>

namespace {

const double kPi = 3.14159265358979323846;

// Pulse table covers x in [kPulseStart, kPulseEnd] bit periods.
const double kPulseStart = -3.0;
const double kPulseEnd = 3.5;

const uint16_t kOffsetA = 0x0FC;
const uint16_t kOffsetB = 0x198;
const uint16_t kOffsetC = 0x168;
const uint16_t kOffsetD = 0x1B4;

uint16_t checkword(uint16_t info) {
    // Generator polynomial x^10 + x^8 + x^7 + x^5 + x^4 + x^3 + 1
    const uint16_t poly = 0x1B9;
    uint16_t reg = 0;
    for (int i = 15; i >= 0; --i) {
        const int bit = (info >> i) & 1;
        const int msb = (reg >> 9) & 1;
        reg = static_cast<uint16_t>((reg << 1) & 0x3FF);
        if (msb ^ bit) reg ^= poly;
    }
    return reg & 0x3FF;
}

// Impulse response of the transmitter shaping filter H(f) = cos(pi * f * Td / 4), |f| <= 2 / Td,
// with x in bit periods and h(0) = 1.
double shapingFilter(double x) {
    const double u = 4.0 * kPi * x;
    const double q = kPi * kPi / 4.0;
    const double denom = q - u * u;
    if (std::fabs(denom) < 1e-9) return kPi / 4.0;
    return std::cos(u) * q / denom;
}

std::string fitText(const std::string& text, size_t length) {
    std::string out = text.substr(0, length);
    out.resize(length, ' ');
    return out;
}

bool utcTime(std::time_t t, std::tm& out) {
#ifdef _WIN32
    return gmtime_s(&out, &t) == 0;
#else
    return gmtime_r(&t, &out) != nullptr;
#endif
}

} // namespace

RDSEncoder::RDSEncoder(const RDSConfig& cfg, int rate)
    : config(cfg), sampleRate(rate), bitAccumulator(0), bitIndex(0), lastDiffBit(0),
      groupBitPos(104), groupCounter(0), psSegment(0), rtSegment(0), rtSegmentCount(0),
      afIndex(0), rtAB(false), lastClockMinute(-1), pulseResolution(1024), gain(1.0f),
      psPending(false), rtPending(false) {
    for (float& a : amplitudes) a = 0.0f;

    psText = fitText(config.ps, 8);
    setRT(config.rt);
    applyPending();

    // AF method A: first code announces the count (224 + N), 205 is the filler code.
    if (!config.af.empty()) {
        std::vector<uint8_t> codes;
        for (double mhz : config.af) {
            const long code = std::lround((mhz - 87.5) * 10.0);
            if (code >= 1 && code <= 204 && codes.size() < 25) codes.push_back(static_cast<uint8_t>(code));
        }
        if (!codes.empty()) {
            afCodes.push_back(static_cast<uint8_t>(224 + codes.size()));
            afCodes.insert(afCodes.end(), codes.begin(), codes.end());
            if (afCodes.size() % 2) afCodes.push_back(205);
        }
    }

    // Biphase symbol: +h(t) at the bit start and -h(t - Td/2) at the half bit, tapered at the tails.
    const int count = static_cast<int>((kPulseEnd - kPulseStart) * pulseResolution) + 1;
    pulseTable.resize(count);
    for (int i = 0; i < count; ++i) {
        const double x = kPulseStart + static_cast<double>(i) / pulseResolution;
        double v = shapingFilter(x) - shapingFilter(x - 0.5);
        const double edge = std::min(x - kPulseStart, kPulseEnd - x);
        if (edge < 0.5) v *= 0.5 - 0.5 * std::cos(kPi * edge / 0.5);
        pulseTable[i] = static_cast<float>(v);
    }

    // Normalize the peak of the shaped signal using a pseudo-random bit pattern.
    uint32_t lfsr = 0xACE1u;
    std::vector<float> amps(4096);
    for (float& a : amps) {
        lfsr = lfsr * 1664525u + 1013904223u;
        a = (lfsr >> 31) ? 1.0f : -1.0f;
    }
    float peak = 0.0f;
    for (int k = 8; k < static_cast<int>(amps.size()) - 8; ++k) {
        for (int s = 0; s < 32; ++s) {
            const double tau = k + s / 32.0;
            float v = 0.0f;
            for (int j = k - 3; j <= k + 3; ++j) v += amps[j] * pulse(tau - j);
            peak = std::max(peak, std::fabs(v));
        }
    }
    gain = peak > 0.0f ? 1.0f / peak : 1.0f;

    for (int k = 0; k < 4; ++k) amplitudes[k & 15] = static_cast<float>(nextBit() ? 1 : -1);
}

float RDSEncoder::pulse(double x) const {
    if (x < kPulseStart || x > kPulseEnd) return 0.0f;
    const double p = (x - kPulseStart) * pulseResolution;
    const int i = static_cast<int>(p);
    if (i + 1 >= static_cast<int>(pulseTable.size())) return pulseTable.back();
    const float t = static_cast<float>(p - i);
    return pulseTable[i] + (pulseTable[i + 1] - pulseTable[i]) * t;
}

float RDSEncoder::nextSample() {
    const double tau = static_cast<double>(bitIndex) + static_cast<double>(bitAccumulator) / (2.0 * sampleRate);
    float v = 0.0f;
    for (int64_t k = bitIndex - 3; k <= bitIndex + 3; ++k) {
        if (k < 0) continue;
        v += amplitudes[k & 15] * pulse(tau - static_cast<double>(k));
    }

    bitAccumulator += 2375;
    while (bitAccumulator >= 2LL * sampleRate) {
        bitAccumulator -= 2LL * sampleRate;
        ++bitIndex;
        // Bits 0..3 were generated in the constructor; stay three bits ahead of the clock.
        amplitudes[(bitIndex + 3) & 15] = static_cast<float>(nextBit() ? 1 : -1);
    }
    return v * gain;
}

int RDSEncoder::nextBit() {
    if (groupBitPos >= 104) {
        buildGroup();
        groupBitPos = 0;
    }
    // Differential encoding: output flips on every data 1.
    lastDiffBit ^= groupBits[groupBitPos++];
    return lastDiffBit;
}

void RDSEncoder::setPS(const std::string& ps) {
    std::lock_guard<std::mutex> lock(pendingMutex);
    pendingPS = ps;
    psPending = true;
}

void RDSEncoder::setRT(const std::string& rt) {
    std::lock_guard<std::mutex> lock(pendingMutex);
    pendingRT = rt;
    rtPending = true;
}

void RDSEncoder::applyPending() {
    // Never block the audio thread; try again at the next group if the lock is busy.
    std::unique_lock<std::mutex> lock(pendingMutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    if (psPending) {
        psText = fitText(pendingPS, 8);
        psPending = false;
    }
    if (rtPending) {
        std::string rt = pendingRT.substr(0, 64);
        if (rt != rtText || rtSegmentCount == 0) {
            rtText = rt;
            rtAB = !rtAB; // Tell receivers to clear their RadioText buffer.
            rtSegment = 0;
            std::string padded = rtText;
            if (padded.size() < 64) padded += '\r';
            rtSegmentCount = rtText.empty() ? 0 : static_cast<int>((padded.size() + 3) / 4);
        }
        rtPending = false;
    }
}

void RDSEncoder::setBlocks(uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
    const uint16_t info[4] = { a, b, c, d };
    const uint16_t offsets[4] = { kOffsetA, kOffsetB, kOffsetC, kOffsetD };
    int pos = 0;
    for (int blk = 0; blk < 4; ++blk) {
        const uint16_t check = checkword(info[blk]) ^ offsets[blk];
        for (int i = 15; i >= 0; --i) groupBits[pos++] = (info[blk] >> i) & 1;
        for (int i = 9; i >= 0; --i) groupBits[pos++] = (check >> i) & 1;
    }
}

void RDSEncoder::buildGroup() {
    applyPending();
    if (config.clockTime && buildGroup4A()) return;

    // Two PS groups per RadioText group keeps the PS name snappy.
    if (rtSegmentCount > 0 && groupCounter % 3 == 2) buildGroup2A();
    else buildGroup0A();
    ++groupCounter;
}

void RDSEncoder::buildGroup0A() {
    const int seg = psSegment;
    psSegment = (psSegment + 1) & 3;

    // Decoder Identification bits d3..d0 on segments 0..3, d0 = stereo.
    const int di = (seg == 3 && config.stereo) ? 1 : 0;
    const uint16_t b = static_cast<uint16_t>((0 << 12) | (0 << 11) | (config.tp << 10) | ((config.pty & 31) << 5) |
                                             (config.ta << 4) | (config.music << 3) | (di << 2) | seg);

    uint16_t c = 0xE0CD; // No AF exists + filler
    if (!afCodes.empty()) {
        c = static_cast<uint16_t>((afCodes[afIndex] << 8) | afCodes[afIndex + 1]);
        afIndex = (afIndex + 2) % static_cast<int>(afCodes.size());
    }

    const uint16_t d = static_cast<uint16_t>((static_cast<uint8_t>(psText[seg * 2]) << 8) |
                                             static_cast<uint8_t>(psText[seg * 2 + 1]));
    setBlocks(config.pi, b, c, d);
}

void RDSEncoder::buildGroup2A() {
    const int seg = rtSegment;
    rtSegment = (rtSegment + 1) % rtSegmentCount;

    std::string padded = rtText;
    if (padded.size() < 64) padded += '\r';
    padded.resize(static_cast<size_t>(rtSegmentCount) * 4, ' ');
    const char* t = padded.c_str() + seg * 4;

    const uint16_t b = static_cast<uint16_t>((2 << 12) | (0 << 11) | (config.tp << 10) | ((config.pty & 31) << 5) |
                                             (rtAB << 4) | seg);
    const uint16_t c = static_cast<uint16_t>((static_cast<uint8_t>(t[0]) << 8) | static_cast<uint8_t>(t[1]));
    const uint16_t d = static_cast<uint16_t>((static_cast<uint8_t>(t[2]) << 8) | static_cast<uint8_t>(t[3]));
    setBlocks(config.pi, b, c, d);
}

bool RDSEncoder::buildGroup4A() {
    const std::time_t now = std::time(nullptr);
    std::tm utc;
    if (!utcTime(now, utc) || utc.tm_min == lastClockMinute) return false;
    lastClockMinute = utc.tm_min;

    // Local offset from UTC in half hours.
    std::tm asLocal = utc;
    asLocal.tm_isdst = -1;
    const double offsetSeconds = std::difftime(now, std::mktime(&asLocal));
    const int halfHours = static_cast<int>(std::lround(offsetSeconds / 1800.0));
    const int offsetSign = halfHours < 0 ? 1 : 0;
    const int offsetValue = std::abs(halfHours) & 31;

    // Modified Julian Day (IEC 62106 Annex G).
    const int y = utc.tm_year;
    const int m = utc.tm_mon + 1;
    const int l = (m == 1 || m == 2) ? 1 : 0;
    const long mjd = 14956 + utc.tm_mday + static_cast<long>((y - l) * 365.25) +
                     static_cast<long>((m + 1 + l * 12) * 30.6001);

    const uint16_t b = static_cast<uint16_t>((4 << 12) | (0 << 11) | (config.tp << 10) | ((config.pty & 31) << 5) |
                                             ((mjd >> 15) & 0x3));
    const uint16_t c = static_cast<uint16_t>(((mjd & 0x7FFF) << 1) | ((utc.tm_hour >> 4) & 1));
    const uint16_t d = static_cast<uint16_t>(((utc.tm_hour & 0xF) << 12) | (utc.tm_min << 6) | (offsetSign << 5) |
                                             offsetValue);
    setBlocks(config.pi, b, c, d);
    return true;
}
