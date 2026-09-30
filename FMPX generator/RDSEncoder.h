#ifndef RDSENCODER_H
#define RDSENCODER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct RDSConfig {
    uint16_t pi = 0x1234;           // Programme Identification
    std::string ps = "EasyMPX";     // Programme Service name (8 chars)
    std::string rt = "";            // RadioText (up to 64 chars)
    int pty = 0;                    // Programme Type (0-31)
    bool tp = false;                // Traffic Programme
    bool ta = false;                // Traffic Announcement
    bool music = true;              // Music/Speech switch
    bool stereo = true;             // Decoder Identification: stereo
    bool clockTime = false;         // Send group 4A clock time every minute
    std::vector<double> af;         // Alternative frequencies in MHz (87.6 - 107.9)
};

// Generates the RDS baseband signal (IEC 62106): groups 0A (PS, AF, flags),
// 2A (RadioText) and 4A (clock time), CRC checkwords, differential and
// biphase coding at 1187.5 bit/s with the standard cosine pulse shaping.
// The caller modulates the output onto a 57 kHz subcarrier locked to the pilot.
class RDSEncoder {
public:
    RDSEncoder(const RDSConfig& config, int sampleRate);

    // Next baseband sample, peak amplitude is normalized to about 1.0.
    float nextSample();

    // Thread-safe updates, picked up at the next group boundary.
    void setPS(const std::string& ps);
    void setRT(const std::string& rt);

private:
    void buildGroup();
    void buildGroup0A();
    void buildGroup2A();
    bool buildGroup4A();
    void setBlocks(uint16_t a, uint16_t b, uint16_t c, uint16_t d);
    int nextBit();
    void applyPending();
    float pulse(double x) const;

    RDSConfig config;
    int sampleRate;

    // Bit clock: accumulates 2375 per sample against 2 * sampleRate (1187.5 bit/s exactly).
    int64_t bitAccumulator;
    int64_t bitIndex;
    float amplitudes[16];
    int lastDiffBit;

    int groupBits[104];
    int groupBitPos;
    int groupCounter;
    int psSegment;
    int rtSegment;
    int rtSegmentCount;
    int afIndex;
    bool rtAB;
    std::vector<uint8_t> afCodes;
    std::string psText;
    std::string rtText;
    int lastClockMinute;

    std::vector<float> pulseTable;
    int pulseResolution;
    float gain;

    std::mutex pendingMutex;
    bool psPending;
    bool rtPending;
    std::string pendingPS;
    std::string pendingRT;
};

#endif
