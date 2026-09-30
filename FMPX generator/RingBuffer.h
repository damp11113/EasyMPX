#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

// Lock-free single producer / single consumer ring buffer of interleaved float frames.
// Used to pass audio from the input device callback to the output device callback.
class RingBuffer {
public:
    RingBuffer(size_t capacityFrames, int channels)
        : channels(channels), writeIndex(0), readIndex(0) {
        capacity = 1;
        while (capacity < capacityFrames) capacity <<= 1;
        mask = capacity - 1;
        data.assign(capacity * channels, 0.0f);
    }

    size_t availableRead() const {
        return writeIndex.load(std::memory_order_acquire) - readIndex.load(std::memory_order_relaxed);
    }

    size_t availableWrite() const {
        return capacity - (writeIndex.load(std::memory_order_relaxed) - readIndex.load(std::memory_order_acquire));
    }

    size_t capacityFrames() const { return capacity; }

    size_t write(const float* frames, size_t count) {
        count = std::min(count, availableWrite());
        size_t w = writeIndex.load(std::memory_order_relaxed);
        for (size_t done = 0; done < count;) {
            size_t pos = (w + done) & mask;
            size_t n = std::min(count - done, capacity - pos);
            std::memcpy(&data[pos * channels], frames + done * channels, n * channels * sizeof(float));
            done += n;
        }
        writeIndex.store(w + count, std::memory_order_release);
        return count;
    }

    size_t read(float* frames, size_t count) {
        count = std::min(count, availableRead());
        size_t r = readIndex.load(std::memory_order_relaxed);
        for (size_t done = 0; done < count;) {
            size_t pos = (r + done) & mask;
            size_t n = std::min(count - done, capacity - pos);
            std::memcpy(frames + done * channels, &data[pos * channels], n * channels * sizeof(float));
            done += n;
        }
        readIndex.store(r + count, std::memory_order_release);
        return count;
    }

private:
    int channels;
    size_t capacity;
    size_t mask;
    std::vector<float> data;
    std::atomic<size_t> writeIndex;
    std::atomic<size_t> readIndex;
};

#endif
