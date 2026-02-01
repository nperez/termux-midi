#ifndef ALSA_INPUT_H
#define ALSA_INPUT_H

#include <string>
#include <atomic>
#include <thread>
#include <functional>

class Synthesizer;

class AlsaInput {
public:
    using QuitCallback = std::function<void()>;

    AlsaInput(Synthesizer& synth);
    ~AlsaInput();

    // Start ALSA sequencer input
    // Creates a virtual MIDI port that other apps can connect to
    bool start(const std::string& clientName = "termux-midi", QuitCallback onQuit = nullptr);

    // Stop input handling
    void stop();

    // Check if running
    bool isRunning() const { return running_.load(); }

    // Get the ALSA client:port string for connection info
    std::string getPortName() const;

private:
    Synthesizer& synth_;
    std::atomic<bool> running_{false};
    std::thread inputThread_;
    QuitCallback quitCallback_;

    struct Impl;
    Impl* impl_ = nullptr;

    void inputLoop();
};

#endif // ALSA_INPUT_H
