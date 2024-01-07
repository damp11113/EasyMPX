#include <vector>
#include <cmath>

struct AudioBuffer {
    float* data; // Audio data
    int size;    // Size of audio buffer
};

struct Band {
    float frequency; // Center frequency of the band
    float threshold;
    float gain;
    float ratio;
    float attackTime; // Attack time in millsiseconds
    float releaseTime; // Release time in milliseconds
    float currentGain; // Current gain level for dynamic changes
    float envelope; // Envelope for dynamic changes
};

// Function to process a single band with dynamic gain changes (attack and release)
void processBandWithDynamicGain(float* buffer, int size, Band& band) {
    const float attackCoef = 1.0f - expf(-1.0f / (0.001f * band.attackTime * 44100.0f)); // Calculate attack coefficient
    const float releaseCoef = 1.0f - expf(-1.0f / (0.001f * band.releaseTime * 44100.0f)); // Calculate release coefficient

    for (int i = 0; i < size; ++i) {
        float input = buffer[i];

        // Envelope follower for dynamic gain adjustment (simulating RMS detection)
        float envelopeLevel = fabsf(input);

        if (envelopeLevel > band.envelope) {
            band.envelope += attackCoef * (envelopeLevel - band.envelope);
        }
        else {
            band.envelope += releaseCoef * (envelopeLevel - band.envelope);
        }

        // Apply compression with dynamic gain based on negative threshold
        float gainReduction = 1.0f;
        if (band.envelope < band.threshold) { // Check if the envelope level is below the negative threshold
            float excessDB = 20.0f * log10f(band.threshold / band.envelope);
            gainReduction = band.gain + (1.0f - band.ratio) * excessDB; // Calculate gain reduction based on ratio
        }

        buffer[i] *= gainReduction;
    }
}

// Function to split audio into bands and compress each band with dynamic gain
void multibandCompressor(AudioBuffer& audio, std::vector<Band>& bands) {
    std::vector<std::vector<float>> bandBuffers(bands.size(), std::vector<float>(audio.size));

    // Split audio into frequency bands
    for (int i = 0; i < audio.size; ++i) {
        for (size_t j = 0; j < bands.size(); ++j) {
            // Split audio into frequency bands (in this simplified example)
            // Replace with appropriate band-splitting logic
            bandBuffers[j][i] = audio.data[i]; // Simulated band splitting
        }
    }

    // Process each band separately with dynamic gain changes
    for (size_t i = 0; i < bands.size(); ++i) {
        processBandWithDynamicGain(bandBuffers[i].data(), audio.size, bands[i]);
    }

    // Recombine bands
    for (int i = 0; i < audio.size; ++i) {
        float mixedValue = 0.0f;
        for (size_t j = 0; j < bands.size(); ++j) {
            mixedValue += bandBuffers[j][i]; // Simulated band recombination
        }
        audio.data[i] = mixedValue; // Replace with proper recombination logic
    }
}

void audioCompressor(float* audioBuffer, int bufferSize, float threshold, float ratio) {
    for (int i = 0; i < bufferSize; ++i) {
        // Apply compression algorithm to each sample
        if (audioBuffer[i] > threshold) {
            audioBuffer[i] = threshold + (audioBuffer[i] - threshold) / ratio;
        }
        else if (audioBuffer[i] < -threshold) {
            audioBuffer[i] = -threshold + (audioBuffer[i] + threshold) / ratio;
        }
    }
}

struct Limiter {
    float threshold;
    float margin;
    float attackTime; // Attack time in milliseconds
    float releaseTime; // Release time in milliseconds
    float ratio;
    float currentGain; // Current gain level for dynamic changes
    float envelope; // Envelope for dynamic changes
};



void limiterProcess(float* buffer, int size, Limiter& limiter) {
    const float attackCoef = 1.0f - expf(-1.0f / (0.001f * limiter.attackTime * 44100.0f)); // Calculate attack coefficient
    const float releaseCoef = 1.0f - expf(-1.0f / (0.001f * limiter.releaseTime * 44100.0f)); // Calculate release coefficient

    for (int i = 0; i < size; ++i) {
        float input = buffer[i];

        // Envelope follower for dynamic gain adjustment (simulating RMS detection)
        float envelopeLevel = fabsf(input);

        if (envelopeLevel > limiter.envelope) {
            limiter.envelope += attackCoef * (envelopeLevel - limiter.envelope);
        }
        else {
            limiter.envelope += releaseCoef * (envelopeLevel - limiter.envelope);
        }

        // Apply limiting with dynamic gain
        if (limiter.envelope > limiter.threshold) {
            float reductionDB = limiter.threshold + limiter.margin - limiter.envelope;
            float gainReduction = powf(10.0f, reductionDB / 20.0f); // Convert dB reduction to linear gain

            // Apply compression with dynamic gain
            float softKnee = 1.0f / limiter.ratio; // Soft knee effect
            float softKneeCoefficient = 1.0f - expf(-softKnee); // Soft knee coefficient

            float gain = 1.0f / (1.0f + softKneeCoefficient * (gainReduction - 1.0f));
            float limitedGain = (gainReduction < gain) ? gainReduction : gain;

            buffer[i] *= limitedGain;
        }
    }
}
