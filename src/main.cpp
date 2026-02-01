#include "audio.h"
#include "synth.h"
#include "midi_file.h"
#include "input.h"
#include "alsa_input.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>

#ifndef VERSION
#define VERSION "1.0.0"
#endif

// Global flag for signal handling
static std::atomic<bool> g_running{true};

void signalHandler(int /*sig*/) {
    g_running.store(false);
}

void printVersion() {
    std::printf("termux-midi %s\n", VERSION);
}

void printUsage(const char* program) {
    std::printf("Usage: %s <command> [options]\n\n", program);
    std::printf("Commands:\n");
    std::printf("  play <file.mid>        Play a MIDI file\n");
    std::printf("  serve                  Run as MIDI service (ALSA sequencer)\n");
    std::printf("  listen                 Real-time mode (text commands from stdin)\n");
    std::printf("  list-instruments       List instruments in soundfont\n");
    std::printf("\nOptions:\n");
    std::printf("  --sf2 <path>           Path to SoundFont file (.sf2 or .sf3)\n");
    std::printf("  --socket <path>        Listen on Unix socket instead of stdin\n");
    std::printf("  --name <name>          ALSA client name (default: termux-midi)\n");
    std::printf("\nReal-time text commands (for 'listen' mode):\n");
    std::printf("  noteon <ch> <note> <vel>   Note on\n");
    std::printf("  noteoff <ch> <note>        Note off\n");
    std::printf("  cc <ch> <ctrl> <val>       Control change\n");
    std::printf("  pc <ch> <prog>             Program change\n");
    std::printf("  pitch <ch> <val>           Pitch bend\n");
    std::printf("  panic                      All notes off\n");
    std::printf("  quit                       Exit\n");
#ifdef USE_ALSA
    std::printf("\nALSA support: enabled\n");
    std::printf("  Use 'aconnect -l' to list MIDI ports\n");
    std::printf("  Use 'aconnect <source> termux-midi' to connect\n");
#else
    std::printf("\nALSA support: disabled (compile with USE_ALSA=1)\n");
#endif
}

std::string findSoundFont() {
    // Check environment variable first
    const char* envSf2 = std::getenv("TERMUX_MIDI_SF2");
    if (envSf2 && std::strlen(envSf2) > 0) {
        return envSf2;
    }

    // Check common locations (SF2 and SF3 formats)
    const char* locations[] = {
        "./soundfont.sf2",
        "./soundfont.sf3",
        "./default.sf2",
        "./default.sf3",
        "soundfonts/default.sf2",
        "soundfonts/default.sf3",
        "/data/data/com.termux/files/home/soundfonts/default.sf2",
        "/data/data/com.termux/files/home/soundfonts/default.sf3",
        "/data/data/com.termux/files/usr/share/soundfonts/default.sf2",
        "/data/data/com.termux/files/usr/share/soundfonts/default.sf3",
        "/usr/share/sounds/sf2/FluidR3_GM.sf2",
        "/usr/share/soundfonts/default.sf2",
        "/usr/share/soundfonts/default.sf3",
        nullptr
    };

    for (int i = 0; locations[i]; ++i) {
        FILE* f = std::fopen(locations[i], "rb");
        if (f) {
            std::fclose(f);
            return locations[i];
        }
    }

    return "";
}

int cmdPlay(const std::string& midiFile, const std::string& sf2Path) {
    Synthesizer synth;

    std::string soundfont = sf2Path.empty() ? findSoundFont() : sf2Path;
    if (soundfont.empty()) {
        std::fprintf(stderr, "No soundfont found. Use --sf2 or set TERMUX_MIDI_SF2\n");
        return 1;
    }

    std::printf("Loading soundfont: %s\n", soundfont.c_str());
    if (!synth.loadSoundFont(soundfont)) {
        return 1;
    }

    synth.setOutput(AudioOutput::SAMPLE_RATE, AudioOutput::CHANNELS);

    MidiPlayer player(synth);
    std::printf("Loading MIDI file: %s\n", midiFile.c_str());
    if (!player.load(midiFile)) {
        return 1;
    }

    AudioOutput audio;
    if (!audio.init([&synth, &player](int16_t* buffer, int frames) {
        player.process(frames);
        synth.render(buffer, frames);
    })) {
        std::fprintf(stderr, "Failed to initialize audio\n");
        return 1;
    }

    player.play();
    if (!audio.start()) {
        std::fprintf(stderr, "Failed to start audio\n");
        return 1;
    }

    std::printf("Playing... (Ctrl+C to stop)\n");

    // Wait for playback to finish or signal
    while (g_running.load() && !player.isFinished()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    audio.stop();
    std::printf("Playback finished\n");

    return 0;
}

int cmdListen(const std::string& sf2Path, const std::string& socketPath) {
    Synthesizer synth;

    std::string soundfont = sf2Path.empty() ? findSoundFont() : sf2Path;
    if (soundfont.empty()) {
        std::fprintf(stderr, "No soundfont found. Use --sf2 or set TERMUX_MIDI_SF2\n");
        return 1;
    }

    std::printf("Loading soundfont: %s\n", soundfont.c_str());
    if (!synth.loadSoundFont(soundfont)) {
        return 1;
    }

    synth.setOutput(AudioOutput::SAMPLE_RATE, AudioOutput::CHANNELS);

    AudioOutput audio;
    if (!audio.init([&synth](int16_t* buffer, int frames) {
        synth.render(buffer, frames);
    })) {
        std::fprintf(stderr, "Failed to initialize audio\n");
        return 1;
    }

    if (!audio.start()) {
        std::fprintf(stderr, "Failed to start audio\n");
        return 1;
    }

    InputHandler input(synth);
    auto onQuit = [&]() {
        g_running.store(false);
    };

    if (!socketPath.empty()) {
        if (!input.startSocket(socketPath, onQuit)) {
            audio.stop();
            return 1;
        }
    } else {
        std::printf("Ready for commands (type 'quit' to exit):\n");
        input.startStdin(onQuit);
    }

    // Wait for quit or signal
    while (g_running.load() && input.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    input.stop();
    audio.stop();

    return 0;
}

int cmdListInstruments(const std::string& sf2Path) {
    Synthesizer synth;

    std::string soundfont = sf2Path.empty() ? findSoundFont() : sf2Path;
    if (soundfont.empty()) {
        std::fprintf(stderr, "No soundfont found. Use --sf2 or set TERMUX_MIDI_SF2\n");
        return 1;
    }

    if (!synth.loadSoundFont(soundfont)) {
        return 1;
    }

    int count = synth.getPresetCount();
    std::printf("Instruments in %s (%d presets):\n\n", soundfont.c_str(), count);

    for (int i = 0; i < count; ++i) {
        std::string name = synth.getPresetName(i);
        std::printf("  %3d: %s\n", i, name.c_str());
    }

    return 0;
}

int cmdServe(const std::string& sf2Path, const std::string& clientName) {
    Synthesizer synth;

    std::string soundfont = sf2Path.empty() ? findSoundFont() : sf2Path;
    if (soundfont.empty()) {
        std::fprintf(stderr, "No soundfont found. Use --sf2 or set TERMUX_MIDI_SF2\n");
        return 1;
    }

    std::printf("Loading soundfont: %s\n", soundfont.c_str());
    if (!synth.loadSoundFont(soundfont)) {
        return 1;
    }

    synth.setOutput(AudioOutput::SAMPLE_RATE, AudioOutput::CHANNELS);

    AudioOutput audio;
    if (!audio.init([&synth](int16_t* buffer, int frames) {
        synth.render(buffer, frames);
    })) {
        std::fprintf(stderr, "Failed to initialize audio\n");
        return 1;
    }

    if (!audio.start()) {
        std::fprintf(stderr, "Failed to start audio\n");
        return 1;
    }

    AlsaInput alsaInput(synth);
    auto onQuit = [&]() {
        g_running.store(false);
    };

    std::string name = clientName.empty() ? "termux-midi" : clientName;
    if (!alsaInput.start(name, onQuit)) {
        audio.stop();
        return 1;
    }

    std::printf("MIDI service running (Ctrl+C to stop)\n");

    // Wait for quit or signal
    while (g_running.load() && alsaInput.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    alsaInput.stop();
    audio.stop();

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Set up signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string command = argv[1];
    std::string sf2Path;
    std::string socketPath;
    std::string midiFile;
    std::string clientName;

    // Parse arguments
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--sf2") == 0 && i + 1 < argc) {
            sf2Path = argv[++i];
        }
        else if (std::strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            socketPath = argv[++i];
        }
        else if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc) {
            clientName = argv[++i];
        }
        else if (argv[i][0] != '-' && midiFile.empty()) {
            midiFile = argv[i];
        }
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
    }

    if (command == "play") {
        if (midiFile.empty()) {
            std::fprintf(stderr, "Error: No MIDI file specified\n");
            printUsage(argv[0]);
            return 1;
        }
        return cmdPlay(midiFile, sf2Path);
    }
    else if (command == "serve") {
        return cmdServe(sf2Path, clientName);
    }
    else if (command == "listen") {
        return cmdListen(sf2Path, socketPath);
    }
    else if (command == "list-instruments") {
        return cmdListInstruments(sf2Path);
    }
    else if (command == "--help" || command == "-h") {
        printUsage(argv[0]);
        return 0;
    }
    else if (command == "--version" || command == "-v") {
        printVersion();
        return 0;
    }
    else {
        std::fprintf(stderr, "Unknown command: %s\n", command.c_str());
        printUsage(argv[0]);
        return 1;
    }
}
