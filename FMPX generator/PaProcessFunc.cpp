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