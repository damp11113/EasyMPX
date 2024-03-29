#include <iostream>
#include <vector>
#include "portaudio.h"
#include "PaDSPFunc.h"
#include "PaMPXFunc.h"
#include "PaFilterFunc.h"
#include "rtfir.hpp"


#define INPUT_DEVICE_INDEX 4
#define OUTPUT_DEVICE_INDEX 39

#define CompositeClipper true;

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

    const int framesPerBuffer = 19200;
    float* buffer = new float[framesPerBuffer * inputParameters.channelCount];
    std::vector<Band> bands = {
        {137, -20.2, 1, 3, 1, 100},  // Example parameters for Band 1
        {1147, -17.6, 0, 2, 1, 100}, // Example parameters for Band 2
        {6910, -24.8, 0, 2, 1, 100}  // Example parameters for Band 3
        // Add more bands with their parameters
    };
    float* piloToneBuffer = new float[framesPerBuffer];
    float* stereoToneBuffer = new float[framesPerBuffer];

    SignalGenerator::GenerateSineWave(piloToneBuffer, framesPerBuffer, 19000, 0.08f);
    SignalGenerator::GenerateSineWave(stereoToneBuffer, framesPerBuffer, 38000, 1);


    std::cout << "MPX encoder is running" << std::endl;

    while (true) {
        err = Pa_ReadStream(inputStream, buffer, framesPerBuffer);
        if (err != paNoError) {
            std::cout << "PortAudio input stream error: " << Pa_GetErrorText(err) << std::endl;
            break;
        }
        // audio processing

        //AudioBuffer audio{ buffer, framesPerBuffer * inputParameters.channelCount };
        //multibandCompressor(audio, bands);



        // MPX Processing here
        float* monoBuffer = new float[framesPerBuffer];
        float* subtractBuffer = new float[framesPerBuffer];
        float* stereoMultipledBuffer = new float[framesPerBuffer];
        float* mixedBuffer = new float[framesPerBuffer];
        
        for (int i = 0, j = 0; i < framesPerBuffer * inputParameters.channelCount; i += 2, ++j) {
            monoBuffer[j] = 0.5f * (buffer[i] + buffer[i + 1]); // Average the two channels
        }
        for (int i = 0, j = 0; i < framesPerBuffer * inputParameters.channelCount; i += 2, ++j) {
            subtractBuffer[j] = 0.5f * (buffer[i] - buffer[i + 1]); // Average the two channels
        }

        // limit mono signal
        /*
        float maxSample2 = 2.5f;
        for (int i = 0; i < framesPerBuffer; ++i) {
            if (fabsf(subtractBuffer[i]) > maxSample2) {
                maxSample2 = fabsf(subtractBuffer[i]);
            }
        }

        if (maxSample2 > 1.0f) {
            for (int i = 0; i < framesPerBuffer; ++i) {
                subtractBuffer[i] /= maxSample2;
            }
        }
        */
        for (int i = 0; i < framesPerBuffer; ++i) {
            stereoMultipledBuffer[i] = stereoToneBuffer[i] * subtractBuffer[i];
        }
        // limit mono signal
        /*
        float maxSample = 2.0f;
        for (int i = 0; i < framesPerBuffer; ++i) {
            if (fabsf(monoBuffer[i]) > maxSample) {
                maxSample = fabsf(monoBuffer[i]);
            }
        }

        if (maxSample > 1.0f) {
            for (int i = 0; i < framesPerBuffer; ++i) {
                monoBuffer[i] /= maxSample;
            }
        }
        */

        // mix mpx
        const float* mixbuffers[3] = {monoBuffer, piloToneBuffer, stereoMultipledBuffer };
        mix(mixedBuffer, mixbuffers, 3, framesPerBuffer);

        if (CompositeClipper) {
            float maxSample = 2.0f;
            for (int i = 0; i < framesPerBuffer; ++i) {
                if (fabsf(mixedBuffer[i]) > maxSample) {
                    maxSample = fabsf(mixedBuffer[i]);
                }
            }

            if (maxSample > 1.0f) {
                for (int i = 0; i < framesPerBuffer; ++i) {
                    mixedBuffer[i] /= maxSample;
                }
            }
        }


        err = Pa_WriteStream(outputStream, mixedBuffer, framesPerBuffer);
        if (err != paNoError) {
            std::cout << "PortAudio output stream error: " << Pa_GetErrorText(err) << std::endl;
            break;
        }
        delete[] monoBuffer;
        delete[] subtractBuffer;
        delete[] mixedBuffer;
        delete[] stereoMultipledBuffer;
        //delete[] lowpassed15KBuffer;
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