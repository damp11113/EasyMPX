#include <iostream>
#include <vector>
#include "portaudio.h"
#include "PaDSPFunc.h"
#include "PaMPXFunc.h"

#define INPUT_DEVICE_INDEX 16
#define OUTPUT_DEVICE_INDEX 47


int main() {
    PaError err;
    PaStream* inputStream, * outputStream;
    PaStreamParameters inputParameters, outputParameters;
    const PaDeviceInfo* inputInfo;
    const PaDeviceInfo* outputInfo;

    Pa_Initialize();

    inputInfo = Pa_GetDeviceInfo(INPUT_DEVICE_INDEX);
    outputInfo = Pa_GetDeviceInfo(OUTPUT_DEVICE_INDEX);

    // Set input parameters
    inputParameters.device = INPUT_DEVICE_INDEX;
    inputParameters.channelCount = 2;
    inputParameters.sampleFormat = paFloat32; // Example: float32 format
    inputParameters.suggestedLatency = inputInfo->defaultHighInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    // Set output parameters
    outputParameters.device = OUTPUT_DEVICE_INDEX;
    outputParameters.channelCount = 1;
    outputParameters.sampleFormat = paFloat32; // Example: float32 format
    outputParameters.suggestedLatency = outputInfo->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    // Open input audio stream
    err = Pa_OpenStream(&inputStream, &inputParameters, NULL, 192000,
        paFramesPerBufferUnspecified, paClipOff, NULL, NULL);

    if (err != paNoError) {
        std::cerr << "PortAudio input stream error: " << Pa_GetErrorText(err) << std::endl;
        Pa_Terminate();
        return -1;
    }

    // Open output audio stream
    err = Pa_OpenStream(&outputStream, NULL, &outputParameters, 192000,
        paFramesPerBufferUnspecified, paClipOff, NULL, NULL);

    if (err != paNoError) {
        std::cerr << "PortAudio output stream error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(inputStream);
        Pa_Terminate();
        return -1;
    }

    // Start input audio stream
    err = Pa_StartStream(inputStream);
    if (err != paNoError) {
        std::cerr << "PortAudio input stream start error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(inputStream);
        Pa_CloseStream(outputStream);
        Pa_Terminate();
        return -1;
    }

    // Start output audio stream
    err = Pa_StartStream(outputStream);
    if (err != paNoError) {
        std::cerr << "PortAudio output stream start error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(inputStream);
        Pa_CloseStream(outputStream);
        Pa_Terminate();
        return -1;
    }

    const int framesPerBuffer = 4096;
    float* buffer = new float[framesPerBuffer * inputParameters.channelCount];
    std::vector<Band> bands = {
        {137, -20.2, 1, 3, 1, 100},  // Example parameters for Band 1
        {1147, -17.6, 0, 2, 1, 100}, // Example parameters for Band 2
        {6910, -24.8, 0, 2, 1, 100}  // Example parameters for Band 3
        // Add more bands with their parameters
    };

    while (true) {
        err = Pa_ReadStream(inputStream, buffer, framesPerBuffer);
        if (err != paNoError) {
            std::cout << "PortAudio input stream error: " << Pa_GetErrorText(err) << std::endl;
            break;
        }

        AudioBuffer audio{ buffer, framesPerBuffer * inputParameters.channelCount };
        multibandCompressor(audio, bands);

        //limiterProcess(buffer, framesPerBuffer, limiter);

        // Normalize the output to prevent extreme volume fluctuations
        
        float maxSample = 1.0f;
        for (int i = 0; i < framesPerBuffer * inputParameters.channelCount; ++i) {
            if (fabsf(buffer[i]) > maxSample) {
                maxSample = fabsf(buffer[i]);
            }
        }

        if (maxSample > 1.0f) {
            for (int i = 0; i < framesPerBuffer * inputParameters.channelCount; ++i) {
                buffer[i] /= maxSample;
            }
        }

        // Merge stereo channels into mono
        float* monoBuffer = new float[framesPerBuffer];
        float* subtractBuffer = new float[framesPerBuffer];
        for (int i = 0, j = 0; i < framesPerBuffer * inputParameters.channelCount; i += 2, ++j) {
            monoBuffer[j] = 0.5f * (buffer[i] + buffer[i + 1]); // Average the two channels
        }
        for (int i = 0, j = 0; i < framesPerBuffer * inputParameters.channelCount; i += 2, ++j) {
            subtractBuffer[j] = 0.5f * (buffer[i] - buffer[i + 1]); // Average the two channels
        }
        


        err = Pa_WriteStream(outputStream, subtractBuffer, framesPerBuffer);
        if (err != paNoError) {
            std::cout << "PortAudio output stream error: " << Pa_GetErrorText(err) << std::endl;
            break;
        }
        delete[] monoBuffer;
        delete[] subtractBuffer;

    }

    delete[] buffer;

    // Stop and close streams
    Pa_StopStream(inputStream);
    Pa_CloseStream(inputStream);
    Pa_StopStream(outputStream);
    Pa_CloseStream(outputStream);
    Pa_Terminate();

    return 0;
}