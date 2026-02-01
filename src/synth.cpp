// SF3 (compressed soundfont) support via stb_vorbis
#define STB_VORBIS_HEADER_ONLY
#include "../vendor/stb_vorbis.c"

#define TSF_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-pointer-subtraction"
#include "../vendor/tsf.h"
#pragma GCC diagnostic pop

// stb_vorbis implementation (after tsf.h)
#undef STB_VORBIS_HEADER_ONLY
#include "../vendor/stb_vorbis.c"

#include "synth.h"
#include <cstdio>
#include <cstring>

Synthesizer::Synthesizer() = default;

Synthesizer::~Synthesizer() {
    if (tsf_) {
        tsf_close(tsf_);
    }
}

bool Synthesizer::loadSoundFont(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (tsf_) {
        tsf_close(tsf_);
        tsf_ = nullptr;
    }

    tsf_ = tsf_load_filename(path.c_str());
    if (!tsf_) {
        std::fprintf(stderr, "Failed to load soundfont: %s\n", path.c_str());
        return false;
    }

    // Set output mode: stereo interleaved
    tsf_set_output(tsf_, TSF_STEREO_INTERLEAVED, sampleRate_, 0.0f);

    return true;
}

void Synthesizer::setOutput(int sampleRate, int /*channels*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    sampleRate_ = sampleRate;

    if (tsf_) {
        tsf_set_output(tsf_, TSF_STEREO_INTERLEAVED, sampleRate, 0.0f);
    }
}

void Synthesizer::noteOn(int channel, int note, float velocity) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tsf_) {
        tsf_channel_note_on(tsf_, channel, note, velocity);
    }
}

void Synthesizer::noteOff(int channel, int note) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tsf_) {
        tsf_channel_note_off(tsf_, channel, note);
    }
}

void Synthesizer::controlChange(int channel, int controller, int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tsf_) {
        tsf_channel_midi_control(tsf_, channel, controller, value);
    }
}

void Synthesizer::programChange(int channel, int program) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tsf_) {
        tsf_channel_set_presetnumber(tsf_, channel, program, channel == 9);
    }
}

void Synthesizer::pitchBend(int channel, int value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tsf_) {
        // Convert 0-16383 to -1.0 to 1.0 (semitones based on pitch bend range)
        float bend = (value - 8192) / 8192.0f;
        tsf_channel_set_pitchwheel(tsf_, channel, value);
        (void)bend; // pitch wheel handles it internally
    }
}

void Synthesizer::allNotesOff() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tsf_) {
        tsf_note_off_all(tsf_);
    }
}

void Synthesizer::render(int16_t* buffer, int frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tsf_) {
        tsf_render_short(tsf_, buffer, frames, 0);
    } else {
        std::memset(buffer, 0, frames * 2 * sizeof(int16_t));
    }
}

std::vector<std::string> Synthesizer::getInstruments() const {
    std::vector<std::string> result;
    std::lock_guard<std::mutex> lock(mutex_);

    if (tsf_) {
        int count = tsf_get_presetcount(tsf_);
        for (int i = 0; i < count; ++i) {
            const char* name = tsf_get_presetname(tsf_, i);
            if (name) {
                result.push_back(name);
            }
        }
    }

    return result;
}

int Synthesizer::getPresetCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tsf_ ? tsf_get_presetcount(tsf_) : 0;
}

std::string Synthesizer::getPresetName(int index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tsf_) {
        const char* name = tsf_get_presetname(tsf_, index);
        if (name) {
            return name;
        }
    }
    return "";
}
