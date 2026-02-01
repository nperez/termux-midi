#ifndef SYNTH_H
#define SYNTH_H

#include <string>
#include <mutex>
#include <vector>

// Forward declare TSF
struct tsf;

class Synthesizer {
public:
    Synthesizer();
    ~Synthesizer();

    // Load a SoundFont file
    bool loadSoundFont(const std::string& path);

    // Check if loaded
    bool isLoaded() const { return tsf_ != nullptr; }

    // Set output mode (stereo interleaved, 44100Hz)
    void setOutput(int sampleRate, int channels);

    // MIDI events (thread-safe)
    void noteOn(int channel, int note, float velocity);
    void noteOff(int channel, int note);
    void controlChange(int channel, int controller, int value);
    void programChange(int channel, int program);
    void pitchBend(int channel, int value);
    void allNotesOff();

    // Render audio (called from audio thread)
    void render(int16_t* buffer, int frames);

    // Get instrument list
    std::vector<std::string> getInstruments() const;

    // Get number of presets
    int getPresetCount() const;
    std::string getPresetName(int index) const;

private:
    tsf* tsf_ = nullptr;
    mutable std::mutex mutex_;
    int sampleRate_ = 44100;
};

#endif // SYNTH_H
