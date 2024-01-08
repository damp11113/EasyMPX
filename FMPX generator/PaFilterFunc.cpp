#include "PaFilterFunc.h"
#include <cmath>

LowPassFilter::LowPassFilter(double sampleRate, double cutoffFreq, int order) {
    coefficients = calculateCoefficients(sampleRate, cutoffFreq, order);
    delayLine.resize(coefficients.size(), 0.0);
}

std::vector<double> LowPassFilter::calculateCoefficients(double sampleRate, double cutoffFreq, int order) {
    std::vector<double> coeffs(order + 1);
    double omegaC = 2.0 * M_PI * cutoffFreq / sampleRate;

    for (int n = 0; n <= order; ++n) {
        if (n == order / 2) {
            coeffs[n] = omegaC / M_PI;
        }
        else {
            coeffs[n] = sin(omegaC * (n - order / 2)) / (M_PI * (n - order / 2));
        }
    }

    return coeffs;
}

void LowPassFilter::apply(float* buffer, int bufferSize) {
    for (int i = 0; i < bufferSize; ++i) {
        double output = 0.0;
        delayLine[0] = buffer[i];

        for (size_t j = 0; j < coefficients.size(); ++j) {
            output += coefficients[j] * delayLine[j];
        }

        for (size_t j = coefficients.size() - 1; j >= 1; --j) {
            delayLine[j] = delayLine[j - 1];
        }

        buffer[i] = static_cast<float>(output);
    }
}
