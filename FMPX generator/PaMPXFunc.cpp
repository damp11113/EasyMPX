#include <vector>
#include <iostream>

// Function to mix left and right channels (L + R)
std::vector<float> mixChannels(const std::vector<float>& leftChannel, const std::vector<float>& rightChannel) {
    // Ensure both channels have the same size
    if (leftChannel.size() != rightChannel.size()) {
        std::cerr << "Channels have different sizes. Mixing aborted." << std::endl;
        return {};
    }

    std::vector<float> mixedChannel;
    mixedChannel.reserve(leftChannel.size()); // Reserve space for the mixed channel

    // Perform mixing (L + R)
    for (size_t i = 0; i < leftChannel.size(); ++i) {
        mixedChannel.push_back(leftChannel[i] + rightChannel[i]);
    }

    return mixedChannel;
}

// Function to subtract left from right channel (L - R)
std::vector<float> subtractChannels(const std::vector<float>& leftChannel, const std::vector<float>& rightChannel) {
    // Ensure both channels have the same size
    if (leftChannel.size() != rightChannel.size()) {
        std::cerr << "Channels have different sizes. Subtraction aborted." << std::endl;
        return {};
    }

    std::vector<float> subtractedChannel;
    subtractedChannel.reserve(leftChannel.size()); // Reserve space for the subtracted channel

    // Perform subtraction (L - R)
    for (size_t i = 0; i < leftChannel.size(); ++i) {
        subtractedChannel.push_back(leftChannel[i] - rightChannel[i]);
    }

    return subtractedChannel;
}