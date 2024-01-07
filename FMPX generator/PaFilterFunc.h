#ifndef PAFILTERFUNC_H
#define M_PI 3.14159265358979323846
#include <vector>

class LowPassFilter {
public:
    LowPassFilter(double sampleRate, double cutoffFreq, int order);
    void apply(float* buffer, int bufferSize);

private:
    std::vector<double> calculateCoefficients(double sampleRate, double cutoffFreq, int order);

    std::vector<double> coefficients;
    std::vector<double> delayLine;
};


#endif 
