#ifndef AUDIO_H
#define AUDIO_H

#include <cstdint>
#include <functional>
#include <atomic>

// Audio callback type: fills buffer with samples, returns number of frames written
using AudioCallback = std::function<void(int16_t* buffer, int frames)>;

class AudioOutput {
public:
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int CHANNELS = 2;
    static constexpr int BUFFER_FRAMES = 1024;
    static constexpr int NUM_BUFFERS = 2;

    AudioOutput();
    ~AudioOutput();

    // Initialize OpenSL ES audio output
    bool init(AudioCallback callback);

    // Start/stop playback
    bool start();
    void stop();

    // Check if running
    bool isRunning() const { return running_.load(); }

    // Called from OpenSL ES callback (public for callback access)
    void onBufferComplete();

private:
    struct Impl;
    Impl* impl_ = nullptr;
    AudioCallback callback_;
    std::atomic<bool> running_{false};
    int currentBuffer_ = 0;

    void fillBuffer(int bufferIndex);
};

#endif // AUDIO_H
