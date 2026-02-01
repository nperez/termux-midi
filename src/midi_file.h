#ifndef MIDI_FILE_H
#define MIDI_FILE_H

#include <string>
#include <atomic>

class Synthesizer;

// Forward declare TML
struct tml_message;

class MidiPlayer {
public:
    MidiPlayer(Synthesizer& synth);
    ~MidiPlayer();

    // Load a MIDI file
    bool load(const std::string& path);

    // Start playback
    void play();

    // Stop playback
    void stop();

    // Check if playing
    bool isPlaying() const { return playing_.load(); }

    // Check if finished (reached end of file)
    bool isFinished() const { return finished_.load(); }

    // Process MIDI events up to current time
    // Call this from audio callback with number of samples rendered
    void process(int samples);

    // Reset to beginning
    void reset();

private:
    Synthesizer& synth_;
    tml_message* midi_ = nullptr;
    tml_message* current_ = nullptr;
    double currentTime_ = 0.0;  // Current playback time in milliseconds
    int sampleRate_ = 44100;
    std::atomic<bool> playing_{false};
    std::atomic<bool> finished_{false};
};

#endif // MIDI_FILE_H
