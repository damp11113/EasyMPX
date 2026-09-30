#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "portaudio.h"
#include "MPXEncoder.h"
#include "Resampler.h"
#include "RingBuffer.h"
#include "WavFile.h"

namespace {

std::atomic<bool> running(true);

void onSignal(int) {
    running = false;
}

struct Options {
    MPXConfig mpx;
    std::string inputFile;
    std::string outputFile;
    int outputBits = 32;
    int inputDevice = -1;
    int outputDevice = -1;
    int inputRate = 192000;
    int outputChannels = 1;
    double bufferMs = 100.0;
    double latencyMs = -1.0;
    std::string rtFile;
    bool listDevices = false;
};

void printUsage() {
    std::cout <<
        "EasyMPX - FM stereo MPX encoder with RDS\n"
        "\n"
        "Usage:\n"
        "  FMPXgen --list-devices\n"
        "  FMPXgen [--in-dev N] [--out-dev N] [options]         realtime soundcard to soundcard\n"
        "  FMPXgen --input in.wav --output mpx.wav [options]    file to file\n"
        "\n"
        "General:\n"
        "  --rate HZ            MPX sample rate (default 192000)\n"
        "  --mono               mono only, no pilot and no 38 kHz subcarrier\n"
        "  --preemphasis US     pre-emphasis 0 (off, default), 50 or 75\n"
        "  --audio-level X      audio level (default 0.88)\n"
        "  --pilot-level X      19 kHz pilot level (default 0.09)\n"
        "  --gain X             output gain (default 1.0)\n"
        "  --no-clipper         disable the audio and composite safety clipper\n"
        "\n"
        "File mode:\n"
        "  --input PATH         input WAV (PCM or float, any rate, mono or stereo)\n"
        "  --output PATH        output MPX WAV (mono at --rate)\n"
        "  --bits N             output format 16, 24 or 32 (float, default)\n"
        "\n"
        "Realtime mode:\n"
        "  --in-dev N           input device index (default: system default)\n"
        "  --out-dev N          output device index (default: system default)\n"
        "  --in-rate HZ         input sample rate (default 192000)\n"
        "  --out-channels N     copy the MPX to N output channels (default 1)\n"
        "  --buffer-ms MS       jitter buffer between the devices (default 100)\n"
        "  --latency MS         device latency (default: device high latency)\n"
        "\n"
        "RDS (any RDS option enables RDS):\n"
        "  --rds                enable RDS\n"
        "  --rds-level X        57 kHz RDS level (default 0.03)\n"
        "  --pi HEX             programme identification (default 1234)\n"
        "  --ps TEXT            programme service name, 8 characters (default EasyMPX)\n"
        "  --rt TEXT            radiotext, up to 64 characters\n"
        "  --rt-file PATH       realtime mode: read radiotext from a file, reloaded on change\n"
        "  --pty N              programme type 0-31 (default 0)\n"
        "  --tp                 traffic programme flag\n"
        "  --ta                 traffic announcement flag\n"
        "  --speech             speech instead of music flag\n"
        "  --ct                 send clock time every minute\n"
        "  --af LIST            alternative frequencies in MHz, e.g. 98.5,101.0\n";
}

bool parseArgs(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&](std::string& out) -> bool {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << a << std::endl;
                return false;
            }
            out = argv[++i];
            return true;
        };
        std::string v;

        if (a == "-h" || a == "--help") { printUsage(); std::exit(0); }
        else if (a == "--list-devices") o.listDevices = true;
        else if (a == "--mono") o.mpx.stereo = false;
        else if (a == "--no-clipper") o.mpx.compositeClipper = false;
        else if (a == "--rds") o.mpx.rds = true;
        else if (a == "--tp") { o.mpx.rds = true; o.mpx.rdsConfig.tp = true; }
        else if (a == "--ta") { o.mpx.rds = true; o.mpx.rdsConfig.ta = true; }
        else if (a == "--speech") { o.mpx.rds = true; o.mpx.rdsConfig.music = false; }
        else if (a == "--ct") { o.mpx.rds = true; o.mpx.rdsConfig.clockTime = true; }
        else if (!value(v)) return false;
        else if (a == "--rate") o.mpx.sampleRate = std::atoi(v.c_str());
        else if (a == "--preemphasis") o.mpx.preemphasisUs = std::atof(v.c_str());
        else if (a == "--audio-level") o.mpx.audioLevel = std::atof(v.c_str());
        else if (a == "--pilot-level") o.mpx.pilotLevel = std::atof(v.c_str());
        else if (a == "--gain") o.mpx.outputGain = std::atof(v.c_str());
        else if (a == "--input") o.inputFile = v;
        else if (a == "--output") o.outputFile = v;
        else if (a == "--bits") o.outputBits = std::atoi(v.c_str());
        else if (a == "--in-dev") o.inputDevice = std::atoi(v.c_str());
        else if (a == "--out-dev") o.outputDevice = std::atoi(v.c_str());
        else if (a == "--in-rate") o.inputRate = std::atoi(v.c_str());
        else if (a == "--out-channels") o.outputChannels = std::atoi(v.c_str());
        else if (a == "--buffer-ms") o.bufferMs = std::atof(v.c_str());
        else if (a == "--latency") o.latencyMs = std::atof(v.c_str());
        else if (a == "--rds-level") { o.mpx.rds = true; o.mpx.rdsLevel = std::atof(v.c_str()); }
        else if (a == "--pi") { o.mpx.rds = true; o.mpx.rdsConfig.pi = static_cast<uint16_t>(std::strtoul(v.c_str(), nullptr, 16)); }
        else if (a == "--ps") { o.mpx.rds = true; o.mpx.rdsConfig.ps = v; }
        else if (a == "--rt") { o.mpx.rds = true; o.mpx.rdsConfig.rt = v; }
        else if (a == "--rt-file") { o.mpx.rds = true; o.rtFile = v; }
        else if (a == "--pty") { o.mpx.rds = true; o.mpx.rdsConfig.pty = std::atoi(v.c_str()) & 31; }
        else if (a == "--af") {
            o.mpx.rds = true;
            std::stringstream ss(v);
            std::string item;
            while (std::getline(ss, item, ',')) o.mpx.rdsConfig.af.push_back(std::atof(item.c_str()));
        }
        else {
            std::cerr << "Unknown option " << a << std::endl;
            return false;
        }
    }

    if (o.mpx.sampleRate < 128000 && (o.mpx.rds || o.mpx.stereo)) {
        std::cerr << "MPX rate must be at least 128000 Hz for stereo and RDS (192000 recommended)" << std::endl;
        return false;
    }
    if (o.inputFile.empty() != o.outputFile.empty()) {
        std::cerr << "File mode needs both --input and --output" << std::endl;
        return false;
    }
    if (o.outputChannels < 1 || o.inputRate <= 0 || o.bufferMs <= 0.0) {
        std::cerr << "Invalid realtime settings" << std::endl;
        return false;
    }
    return true;
}

// Converts any channel count to interleaved stereo.
void toStereo(const float* in, int channels, float* out, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
        const float l = in[i * channels];
        const float r = channels > 1 ? in[i * channels + 1] : l;
        out[i * 2] = l;
        out[i * 2 + 1] = r;
    }
}

bool readTextFile(const std::string& path, std::string& text) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::getline(f, text);
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
    return true;
}

// ---------------------------------------------------------------- file mode

int runFileMode(const Options& o) {
    std::string error;
    WavReader reader;
    if (!reader.open(o.inputFile, error)) {
        std::cerr << error << std::endl;
        return 1;
    }
    WavWriter writer;
    if (!writer.open(o.outputFile, o.mpx.sampleRate, 1, o.outputBits, error)) {
        std::cerr << error << std::endl;
        return 1;
    }

    const int inRate = reader.sampleRate();
    const int outRate = o.mpx.sampleRate;
    const bool resample = inRate != outRate;
    std::cout << "Input:  " << o.inputFile << " (" << inRate << " Hz, " << reader.channels() << " ch)\n"
              << "Output: " << o.outputFile << " (" << outRate << " Hz MPX, "
              << (o.outputBits == 32 ? "32-bit float" : std::to_string(o.outputBits) + "-bit") << ")\n"
              << "Stereo: " << (o.mpx.stereo ? "on" : "off") << ", RDS: " << (o.mpx.rds ? "on" : "off") << std::endl;

    MPXEncoder encoder(o.mpx);
    Resampler resampler(2, inRate, outRate);

    const size_t chunk = 8192;
    const uint64_t totalIn = reader.totalFrames();
    const uint64_t totalOut = static_cast<uint64_t>(std::llround(static_cast<double>(totalIn) * outRate / inRate));
    std::vector<float> raw(chunk * reader.channels());
    std::vector<float> stereo(chunk * 2);
    const size_t outChunk = chunk * (outRate / inRate + 2);
    std::vector<float> resampled(outChunk * 2);
    std::vector<float> mpx(outChunk);

    uint64_t written = 0;
    int lastPercent = -1;
    auto emit = [&](const float* frames, size_t count) -> bool {
        count = static_cast<size_t>(std::min<uint64_t>(count, totalOut - written));
        encoder.process(frames, mpx.data(), count);
        written += count;
        return writer.write(mpx.data(), count);
    };

    bool ok = true;
    bool eof = false;
    while (ok && written < totalOut) {
        size_t got = eof ? 0 : reader.read(raw.data(), chunk);
        if (got == 0) eof = true;

        if (!resample) {
            if (got == 0) break;
            toStereo(raw.data(), reader.channels(), stereo.data(), got);
            ok = emit(stereo.data(), got);
        } else {
            if (got > 0) {
                toStereo(raw.data(), reader.channels(), stereo.data(), got);
                resampler.push(stereo.data(), got);
            } else {
                resampler.pushSilence(chunk); // flush the filter tail
            }
            size_t n;
            while (ok && written < totalOut && (n = resampler.pull(resampled.data(), outChunk)) > 0) {
                ok = emit(resampled.data(), n);
            }
        }

        const int percent = totalOut ? static_cast<int>(100 * written / totalOut) : 100;
        if (percent != lastPercent) {
            std::cout << "\rEncoding... " << percent << "%" << std::flush;
            lastPercent = percent;
        }
    }
    ok = writer.close() && ok;
    std::cout << std::endl;
    if (!ok) {
        std::cerr << "Failed writing " << o.outputFile << std::endl;
        return 1;
    }
    std::cout << "Done, " << written << " samples (" << static_cast<double>(written) / outRate << " s)" << std::endl;
    return 0;
}

// ---------------------------------------------------------------- realtime mode

double steadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Realtime {
    Realtime(const Options& o, int inChannels)
        : ring(static_cast<size_t>(o.inputRate), 2),
          resampler(2, o.inputRate, o.mpx.sampleRate),
          encoder(o.mpx),
          inputChannels(inChannels),
          outputChannels(o.outputChannels),
          inputRate(o.inputRate),
          outputRate(o.mpx.sampleRate),
          targetFill(o.bufferMs * 0.001 * o.inputRate),
          stereoIn(kMaxChunk * 2),
          popped((static_cast<size_t>(kMaxChunk * 1.01 * o.inputRate / o.mpx.sampleRate) + 256) * 2),
          resampled(kMaxChunk * 2),
          mpx(kMaxChunk) {}

    static const size_t kMaxChunk = 4096;

    RingBuffer ring;
    Resampler resampler;
    MPXEncoder encoder;
    int inputChannels;
    int outputChannels;
    double inputRate;
    double outputRate;
    double targetFill;
    double (*now)() = steadySeconds;

    // Written by the input callback
    std::atomic<double> lastInputTime{ 0.0 };
    std::atomic<double> lastInputSpan{ 0.0 };

    // Output callback state
    bool buffering = true;
    double fillAverage = 0.0;
    double integral = 0.0;
    std::vector<float> stereoIn;
    std::vector<float> popped;
    std::vector<float> resampled;
    std::vector<float> mpx;

    // Status, read by the main thread
    std::atomic<double> fillMs{ 0.0 };
    std::atomic<double> correctionPpm{ 0.0 };
    std::atomic<unsigned> underruns{ 0 };
    std::atomic<unsigned> overflows{ 0 };
};

int inputCallback(const void* input, void*, unsigned long frameCount,
                  const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags flags, void* userData) {
    Realtime& rt = *static_cast<Realtime*>(userData);
    if (flags & paInputOverflow) rt.overflows++;
    const float* in = static_cast<const float*>(input);
    if (!in) return paContinue;

    for (unsigned long done = 0; done < frameCount;) {
        const size_t n = std::min<size_t>(frameCount - done, Realtime::kMaxChunk);
        toStereo(in + done * rt.inputChannels, rt.inputChannels, rt.stereoIn.data(), n);
        if (rt.ring.write(rt.stereoIn.data(), n) < n) rt.overflows++;
        done += static_cast<unsigned long>(n);
    }
    rt.lastInputSpan = static_cast<double>(frameCount) / rt.inputRate;
    rt.lastInputTime = rt.now();
    return paContinue;
}

int outputCallback(const void*, void* output, unsigned long frameCount,
                   const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData) {
    Realtime& rt = *static_cast<Realtime*>(userData);
    float* out = static_cast<float*>(output);

    // Drift control: steer the resampling ratio so the jitter buffer stays at its target.
    // This absorbs the clock difference between the two soundcards without ever
    // running dry or overflowing, which is what caused the clicks.
    // The frames the input device captured since its last callback are counted too,
    // otherwise the burst sawtooth aliases into the measurement when both callbacks
    // run at related periods.
    const double ringFill = static_cast<double>(rt.ring.availableRead());
    const double sinceInput = std::max(0.0, std::min(2.0 * rt.lastInputSpan.load(), rt.now() - rt.lastInputTime.load()));
    const double fill = ringFill + sinceInput * rt.inputRate;
    const double dt = static_cast<double>(frameCount) / rt.outputRate;
    if (rt.buffering && fill >= rt.targetFill) {
        rt.buffering = false;
        rt.fillAverage = fill;
    }
    if (!rt.buffering) {
        const double alpha = 1.0 - std::exp(-dt / 0.5);
        rt.fillAverage += alpha * (fill - rt.fillAverage);
        const double error = (rt.fillAverage - rt.targetFill) / rt.inputRate; // seconds
        rt.integral = std::max(-2.0, std::min(2.0, rt.integral + error * dt));
        const double correction = std::max(-0.002, std::min(0.002, 0.1 * error + 0.005 * rt.integral));
        rt.resampler.setRatioCorrection(correction);
        rt.correctionPpm = correction * 1e6;
    }
    rt.fillMs = (rt.buffering ? ringFill : rt.fillAverage) * 1000.0 / rt.inputRate;

    for (unsigned long done = 0; done < frameCount;) {
        const size_t n = std::min<size_t>(frameCount - done, Realtime::kMaxChunk);
        const size_t need = std::min(rt.resampler.framesNeeded(n), rt.popped.size() / 2);

        if (rt.buffering) {
            rt.resampler.pushSilence(need);
        } else {
            const size_t got = rt.ring.read(rt.popped.data(), need);
            rt.resampler.push(rt.popped.data(), got);
            if (got < need) {
                // Input stalled: keep the pilot and RDS running on silence and rebuffer.
                rt.underruns++;
                rt.buffering = true;
                rt.resampler.pushSilence(need - got);
            }
        }

        const size_t produced = rt.resampler.pull(rt.resampled.data(), n);
        for (size_t i = produced * 2; i < n * 2; ++i) rt.resampled[i] = 0.0f;
        rt.encoder.process(rt.resampled.data(), rt.mpx.data(), n);

        for (size_t i = 0; i < n; ++i) {
            for (int c = 0; c < rt.outputChannels; ++c) out[(done + i) * rt.outputChannels + c] = rt.mpx[i];
        }
        done += static_cast<unsigned long>(n);
    }
    return paContinue;
}

void listDevices() {
    const int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo* api = Pa_GetHostApiInfo(info->hostApi);
        std::cout << "[" << i << "] " << info->name << " (" << (api ? api->name : "?") << ")"
                  << "  in: " << info->maxInputChannels << "  out: " << info->maxOutputChannels
                  << "  default rate: " << info->defaultSampleRate << std::endl;
    }
}

int runRealtime(const Options& o) {
    const int inDev = o.inputDevice >= 0 ? o.inputDevice : Pa_GetDefaultInputDevice();
    const int outDev = o.outputDevice >= 0 ? o.outputDevice : Pa_GetDefaultOutputDevice();
    if (inDev < 0 || inDev >= Pa_GetDeviceCount() || outDev < 0 || outDev >= Pa_GetDeviceCount()) {
        std::cerr << "Invalid audio device, use --list-devices" << std::endl;
        return 1;
    }
    const PaDeviceInfo* inputInfo = Pa_GetDeviceInfo(inDev);
    const PaDeviceInfo* outputInfo = Pa_GetDeviceInfo(outDev);
    const int inChannels = inputInfo->maxInputChannels >= 2 ? 2 : 1;

    PaStreamParameters inputParameters;
    inputParameters.device = inDev;
    inputParameters.channelCount = inChannels;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = o.latencyMs > 0 ? o.latencyMs / 1000.0 : inputInfo->defaultHighInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    PaStreamParameters outputParameters;
    outputParameters.device = outDev;
    outputParameters.channelCount = o.outputChannels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = o.latencyMs > 0 ? o.latencyMs / 1000.0 : outputInfo->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    std::unique_ptr<Realtime> rt(new Realtime(o, inChannels));

    PaStream* inputStream = nullptr;
    PaStream* outputStream = nullptr;
    PaError err = Pa_OpenStream(&inputStream, &inputParameters, NULL, o.inputRate,
                                paFramesPerBufferUnspecified, paClipOff, inputCallback, rt.get());
    if (err != paNoError) {
        std::cerr << "PortAudio input stream error: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }
    err = Pa_OpenStream(&outputStream, NULL, &outputParameters, o.mpx.sampleRate,
                        paFramesPerBufferUnspecified, paClipOff, outputCallback, rt.get());
    if (err != paNoError) {
        std::cerr << "PortAudio output stream error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(inputStream);
        return 1;
    }

    std::string rtText;
    if (!o.rtFile.empty() && rt->encoder.rds() && readTextFile(o.rtFile, rtText)) rt->encoder.rds()->setRT(rtText);

    err = Pa_StartStream(inputStream);
    if (err == paNoError) err = Pa_StartStream(outputStream);
    if (err != paNoError) {
        std::cerr << "PortAudio stream start error: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(inputStream);
        Pa_CloseStream(outputStream);
        return 1;
    }

    std::cout << "MPX encoder is running: [" << inDev << "] " << inputInfo->name << " -> [" << outDev << "] "
              << outputInfo->name << "\nStereo: " << (o.mpx.stereo ? "on" : "off") << ", RDS: "
              << (o.mpx.rds ? "on" : "off") << ". Press Ctrl+C to stop." << std::endl;

    int tick = 0;
    while (running && Pa_IsStreamActive(outputStream) == 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (++tick % 2) continue;

        char status[160];
        std::snprintf(status, sizeof(status), "\rbuffer: %6.1f ms | drift: %+7.1f ppm | underruns: %u | overflows: %u   ",
                      rt->fillMs.load(), rt->correctionPpm.load(), rt->underruns.load(), rt->overflows.load());
        std::cout << status << std::flush;

        std::string text;
        if (!o.rtFile.empty() && rt->encoder.rds() && readTextFile(o.rtFile, text) && text != rtText) {
            rtText = text;
            rt->encoder.rds()->setRT(rtText);
        }
    }
    std::cout << "\nStopping" << std::endl;

    Pa_StopStream(inputStream);
    Pa_StopStream(outputStream);
    Pa_CloseStream(inputStream);
    Pa_CloseStream(outputStream);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseArgs(argc, argv, options)) {
        std::cerr << "Use --help for usage" << std::endl;
        return 1;
    }
    std::signal(SIGINT, onSignal);

    if (!options.inputFile.empty()) return runFileMode(options);

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }
    int result = 0;
    if (options.listDevices) listDevices();
    else result = runRealtime(options);
    Pa_Terminate();
    return result;
}
