#include "WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint64_t le64(const uint8_t* p) { return static_cast<uint64_t>(le32(p)) | (static_cast<uint64_t>(le32(p + 4)) << 32); }

void put16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>(x));
    v.push_back(static_cast<uint8_t>(x >> 8));
}
void put32(std::vector<uint8_t>& v, uint32_t x) {
    put16(v, static_cast<uint16_t>(x));
    put16(v, static_cast<uint16_t>(x >> 16));
}
void put64(std::vector<uint8_t>& v, uint64_t x) {
    put32(v, static_cast<uint32_t>(x));
    put32(v, static_cast<uint32_t>(x >> 32));
}
void putTag(std::vector<uint8_t>& v, const char* tag) { v.insert(v.end(), tag, tag + 4); }

const uint16_t kFormatPCM = 1;
const uint16_t kFormatFloat = 3;
const uint16_t kFormatExtensible = 0xFFFE;
const std::streamoff kHeaderSize = 80; // RIFF(12) + JUNK/ds64(36) + fmt(24) + data header(8)

} // namespace

bool WavReader::open(const std::string& path, std::string& error) {
    file.open(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }

    uint8_t header[12];
    if (!file.read(reinterpret_cast<char*>(header), 12) ||
        (std::memcmp(header, "RIFF", 4) != 0 && std::memcmp(header, "RF64", 4) != 0) ||
        std::memcmp(header + 8, "WAVE", 4) != 0) {
        error = path + " is not a WAV file";
        return false;
    }

    uint64_t rf64DataSize = 0;
    bool haveFormat = false;
    while (true) {
        uint8_t chunk[8];
        if (!file.read(reinterpret_cast<char*>(chunk), 8)) {
            error = path + " has no data chunk";
            return false;
        }
        const uint32_t size = le32(chunk + 4);

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            std::vector<uint8_t> fmt(size);
            if (size < 16 || !file.read(reinterpret_cast<char*>(fmt.data()), size)) {
                error = path + " has a broken fmt chunk";
                return false;
            }
            uint16_t format = le16(&fmt[0]);
            channelCount = le16(&fmt[2]);
            rate = static_cast<int>(le32(&fmt[4]));
            bitsPerSample = le16(&fmt[14]);
            if (format == kFormatExtensible && size >= 26) format = le16(&fmt[24]);
            if (format == kFormatFloat) isFloat = true;
            else if (format != kFormatPCM) {
                error = path + ": unsupported WAV encoding (only PCM and float)";
                return false;
            }
            const bool validBits = isFloat ? (bitsPerSample == 32 || bitsPerSample == 64)
                                           : (bitsPerSample == 8 || bitsPerSample == 16 || bitsPerSample == 24 || bitsPerSample == 32);
            if (!validBits || channelCount < 1 || rate <= 0) {
                error = path + ": unsupported sample format";
                return false;
            }
            haveFormat = true;
        } else if (std::memcmp(chunk, "ds64", 4) == 0) {
            std::vector<uint8_t> ds(size);
            if (size < 24 || !file.read(reinterpret_cast<char*>(ds.data()), size)) {
                error = path + " has a broken ds64 chunk";
                return false;
            }
            rf64DataSize = le64(&ds[8]);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            if (!haveFormat) {
                error = path + ": data chunk before fmt chunk";
                return false;
            }
            const uint64_t frameBytes = static_cast<uint64_t>(channelCount) * (bitsPerSample / 8);
            uint64_t dataSize = (size == 0xFFFFFFFFu && rf64DataSize) ? rf64DataSize : size;
            if (size == 0 || size == 0xFFFFFFFFu) {
                // Streamed file with an unfinished header: read to the end of file.
                const std::streampos here = file.tellg();
                file.seekg(0, std::ios::end);
                const uint64_t remaining = static_cast<uint64_t>(file.tellg() - here);
                file.seekg(here);
                if (!rf64DataSize) dataSize = remaining;
            }
            frameCount = dataSize / frameBytes;
            framesLeft = frameCount;
            return true;
        } else {
            file.seekg(size + (size & 1), std::ios::cur);
        }
    }
}

size_t WavReader::read(float* out, size_t frames) {
    frames = static_cast<size_t>(std::min<uint64_t>(frames, framesLeft));
    if (frames == 0) return 0;
    const size_t bytesPerSample = static_cast<size_t>(bitsPerSample / 8);
    const size_t samples = frames * channelCount;
    raw.resize(samples * bytesPerSample);
    file.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    const size_t gotSamples = static_cast<size_t>(file.gcount()) / bytesPerSample;
    frames = gotSamples / channelCount;
    framesLeft = frames ? framesLeft - frames : 0;

    const uint8_t* p = raw.data();
    for (size_t i = 0; i < frames * channelCount; ++i, p += bytesPerSample) {
        float v;
        if (isFloat) {
            if (bitsPerSample == 32) {
                const uint32_t bits = le32(p);
                std::memcpy(&v, &bits, 4);
            } else {
                const uint64_t bits = le64(p);
                double d;
                std::memcpy(&d, &bits, 8);
                v = static_cast<float>(d);
            }
        } else if (bitsPerSample == 8) {
            v = (static_cast<int>(p[0]) - 128) / 128.0f;
        } else if (bitsPerSample == 16) {
            v = static_cast<int16_t>(le16(p)) / 32768.0f;
        } else if (bitsPerSample == 24) {
            int32_t s = static_cast<int32_t>(static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16)) << 8) >> 8;
            v = static_cast<float>(s / 8388608.0);
        } else {
            v = static_cast<float>(static_cast<int32_t>(le32(p)) / 2147483648.0);
        }
        out[i] = v;
    }
    return frames;
}

WavWriter::~WavWriter() {
    close();
}

bool WavWriter::open(const std::string& path, int sampleRate, int channels, int bits, std::string& error) {
    if (bits != 16 && bits != 24 && bits != 32) {
        error = "output bits must be 16, 24 or 32 (float)";
        return false;
    }
    file.open(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "cannot create " + path;
        return false;
    }
    channelCount = channels;
    bitsPerSample = bits;
    dataBytes = 0;

    const uint16_t blockAlign = static_cast<uint16_t>(channels * bits / 8);
    std::vector<uint8_t> h;
    putTag(h, "RIFF");
    put32(h, 0);
    putTag(h, "WAVE");
    // Placeholder that becomes the ds64 chunk if the file grows past 4 GB.
    putTag(h, "JUNK");
    put32(h, 28);
    h.insert(h.end(), 28, 0);
    putTag(h, "fmt ");
    put32(h, 16);
    put16(h, bits == 32 ? kFormatFloat : kFormatPCM);
    put16(h, static_cast<uint16_t>(channels));
    put32(h, static_cast<uint32_t>(sampleRate));
    put32(h, static_cast<uint32_t>(sampleRate) * blockAlign);
    put16(h, blockAlign);
    put16(h, static_cast<uint16_t>(bits));
    putTag(h, "data");
    put32(h, 0);
    file.write(reinterpret_cast<const char*>(h.data()), static_cast<std::streamsize>(h.size()));
    return static_cast<bool>(file);
}

bool WavWriter::write(const float* frames, size_t count) {
    if (!file.is_open()) return false;
    const size_t samples = count * channelCount;
    raw.clear();
    raw.reserve(samples * (bitsPerSample / 8));
    for (size_t i = 0; i < samples; ++i) {
        const float x = frames[i];
        if (bitsPerSample == 32) {
            uint32_t bits;
            std::memcpy(&bits, &x, 4);
            put32(raw, bits);
        } else {
            const float c = std::max(-1.0f, std::min(1.0f, x));
            if (bitsPerSample == 16) {
                put16(raw, static_cast<uint16_t>(static_cast<int16_t>(std::lrint(c * 32767.0f))));
            } else {
                const int32_t s = static_cast<int32_t>(std::lrint(c * 8388607.0f));
                raw.push_back(static_cast<uint8_t>(s));
                raw.push_back(static_cast<uint8_t>(s >> 8));
                raw.push_back(static_cast<uint8_t>(s >> 16));
            }
        }
    }
    file.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    dataBytes += raw.size();
    return static_cast<bool>(file);
}

bool WavWriter::close() {
    if (!file.is_open()) return true;
    if (dataBytes & 1) file.put(0);

    const uint64_t riffSize = static_cast<uint64_t>(kHeaderSize) - 8 + dataBytes + (dataBytes & 1);
    const uint64_t frames = dataBytes / (static_cast<uint64_t>(channelCount) * (bitsPerSample / 8));
    std::vector<uint8_t> v;
    if (riffSize <= 0xFFFFFFFFu) {
        file.seekp(4);
        put32(v, static_cast<uint32_t>(riffSize));
        file.write(reinterpret_cast<const char*>(v.data()), 4);
        file.seekp(kHeaderSize - 4);
        v.clear();
        put32(v, static_cast<uint32_t>(dataBytes));
        file.write(reinterpret_cast<const char*>(v.data()), 4);
    } else {
        file.seekp(0);
        putTag(v, "RF64");
        put32(v, 0xFFFFFFFFu);
        putTag(v, "WAVE");
        putTag(v, "ds64");
        put32(v, 28);
        put64(v, riffSize);
        put64(v, dataBytes);
        put64(v, frames);
        put32(v, 0);
        file.write(reinterpret_cast<const char*>(v.data()), static_cast<std::streamsize>(v.size()));
        file.seekp(kHeaderSize - 4);
        v.clear();
        put32(v, 0xFFFFFFFFu);
        file.write(reinterpret_cast<const char*>(v.data()), 4);
    }
    const bool ok = static_cast<bool>(file);
    file.close();
    return ok;
}
