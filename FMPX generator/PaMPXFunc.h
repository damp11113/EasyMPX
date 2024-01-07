#ifndef PAMPXFUNC_H
#define PAMPXFUNC_H

#include <vector>

std::vector<float> mixChannels(const std::vector<float>& leftChannel, const std::vector<float>& rightChannel);
std::vector<float> subtractChannels(const std::vector<float>& leftChannel, const std::vector<float>& rightChannel);

#endif
