#ifndef WAVFILE_H
#define WAVFILE_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// Reads PCM (8/16/24/32 bit) and IEEE float (32/64 bit) WAV and RF64 files as float frames.
class WavReader {
public:
    bool open(const std::string& path, std::string& error);
    // Reads up to frames interleaved frames, returns frames read (0 at end of file).
    size_t read(float* out, size_t frames);

    int sampleRate() const { return rate; }
    int channels() const { return channelCount; }
    uint64_t totalFrames() const { return frameCount; }

private:
    std::ifstream file;
    int rate = 0;
    int channelCount = 0;
    int bitsPerSample = 0;
    bool isFloat = false;
    uint64_t frameCount = 0;
    uint64_t framesLeft = 0;
    std::vector<uint8_t> raw;
};

// Writes WAV files as 16/24-bit PCM or 32-bit float. Switches to RF64 when the data exceeds 4 GB.
class WavWriter {
public:
    ~WavWriter();
    bool open(const std::string& path, int sampleRate, int channels, int bits, std::string& error);
    bool write(const float* frames, size_t count);
    bool close();

private:
    std::ofstream file;
    int channelCount = 0;
    int bitsPerSample = 0;
    uint64_t dataBytes = 0;
    std::vector<uint8_t> raw;
};

#endif
