# EasyMPX
[![Windows](https://badgen.net/badge/icon/windows?icon=windows&label)](https://microsoft.com/windows/)

A Stereo encoder for FM transmitters now with RDS support.

!! Warning !! If you use this software please use with audio processor like stereo tool or hellfire processor. This software haven't composite clipper or composite limiter for protect.

This program can generate mpx signal for FM transmitters to produce stereo transmissions. 
This can you to transmit your own FM stereo signal and can be received with any standard FM stereo radio. 

This software can use only with soundcard supported 192Khz+ samples rates 

# Features
- Stereo MPX generation (mono + 19 kHz pilot + 38 kHz L-R) with phase-locked, click-free oscillators
- RDS encoder: PI, PS, RadioText (static or live from a text file), PTY, TP/TA, M/S, AF list and clock time (CT)
- File to file mode: turn any WAV (PCM or float, any sample rate, mono or stereo) into an MPX WAV
- Realtime mode: soundcard to soundcard with a drift-compensating jitter buffer, so two devices with different clocks never underrun or click
- 15 kHz brickwall lowpass that keeps audio out of the pilot and RDS bands
- Optional 50/75 us pre-emphasis and composite safety clipper

# Download
Grab `FMPXgen.exe` (Windows x64, no DLLs required) from the **Actions** tab (artifact `EasyMPX-windows-x64`) or build it yourself (see below).

# Usage
List audio devices:
```
FMPXgen --list-devices
```

Realtime (soundcard input to soundcard output, output must support 192 kHz):
```
FMPXgen --in-dev 4 --out-dev 39 --rds --pi 1234 --ps "EASYMPX" --rt "Hello world" --pty 10
```
The status line shows the jitter buffer fill, the measured clock drift between the two soundcards and any underruns or overflows. Use `--in-rate` if your input runs at 44100/48000 Hz and `--out-channels 2` if the output device does not accept mono. Use `--rt-file nowplaying.txt` to update RadioText live from a text file.

File to file:
```
FMPXgen --input music.wav --output mpx.wav --rds --ps "EASYMPX" --rt "Now playing: ..."
```
The output is a mono 192 kHz WAV (32-bit float by default, `--bits 16` or `--bits 24` for PCM).

Levels (fraction of full deviation, defaults follow common practice): audio 0.88 (`--audio-level`), pilot 0.09 (`--pilot-level`), RDS 0.03 (`--rds-level`), plus `--gain` for the overall output. Note that the old version effectively ran the whole composite at half level, so recheck your transmitter deviation and use `--gain 0.5` to match the old level.

Run `FMPXgen --help` for all options.

# Building
- Windows (Visual Studio): open `FMPXGEN.sln`, restore NuGet packages and build **Release** (Debug builds may be too slow for realtime).
- Windows (MinGW cross compile) / Linux / macOS: see `.github/workflows/build.yml`, or on Linux/macOS install PortAudio and run
```
cmake -S . -B build && cmake --build build
```

# Transmitter
The transmitter mustn't have lowpass filter or builtin mpx generator inside. But if transmitter have "MPX IN" or "Composite In" please turn off or settings your transmitter to disable builtin mpx generator for using this software

For testing it is possible to use a simple one transistor FM transmitter design but signal quality and frequency stability are likely to be problems.

# Previews
### Processing with Stereo Tool
![Processing with Stereo Tool](https://github.com/damp11113/FM-MPX-Processor/assets/64675096/f4fe9d1e-4908-4ddb-9038-80327d59091a)
### Processing with HellFire 8.0.1 (Composite Clipper enable)
![Processing with HellFire 8.0.1](https://github.com/damp11113/EasyMPX/assets/64675096/e90275b7-42e0-4eb6-82d3-1f84164747bd)
