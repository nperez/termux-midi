#define TML_IMPLEMENTATION
#include "../vendor/tml.h"
#include "midi_file.h"
#include "synth.h"
#include <cstdio>

MidiPlayer::MidiPlayer(Synthesizer& synth)
    : synth_(synth) {
}

MidiPlayer::~MidiPlayer() {
    if (midi_) {
        tml_free(midi_);
    }
}

bool MidiPlayer::load(const std::string& path) {
    if (midi_) {
        tml_free(midi_);
        midi_ = nullptr;
    }

    midi_ = tml_load_filename(path.c_str());
    if (!midi_) {
        std::fprintf(stderr, "Failed to load MIDI file: %s\n", path.c_str());
        return false;
    }

    current_ = midi_;
    currentTime_ = 0.0;
    finished_.store(false);

    return true;
}

void MidiPlayer::play() {
    if (midi_ && !finished_.load()) {
        playing_.store(true);
    }
}

void MidiPlayer::stop() {
    playing_.store(false);
    synth_.allNotesOff();
}

void MidiPlayer::reset() {
    stop();
    current_ = midi_;
    currentTime_ = 0.0;
    finished_.store(false);
}

void MidiPlayer::process(int samples) {
    if (!playing_.load() || !current_) {
        return;
    }

    // Convert samples to milliseconds
    double msPerSample = 1000.0 / sampleRate_;
    double targetTime = currentTime_ + (samples * msPerSample);

    // Process all MIDI events up to the target time
    while (current_ && current_->time <= targetTime) {
        switch (current_->type) {
            case TML_NOTE_ON:
                if (current_->velocity > 0) {
                    synth_.noteOn(current_->channel, current_->key, current_->velocity / 127.0f);
                } else {
                    synth_.noteOff(current_->channel, current_->key);
                }
                break;

            case TML_NOTE_OFF:
                synth_.noteOff(current_->channel, current_->key);
                break;

            case TML_CONTROL_CHANGE:
                synth_.controlChange(current_->channel, current_->control, current_->control_value);
                break;

            case TML_PROGRAM_CHANGE:
                synth_.programChange(current_->channel, current_->program);
                break;

            case TML_PITCH_BEND:
                synth_.pitchBend(current_->channel, current_->pitch_bend);
                break;

            default:
                // Ignore other message types (sysex, meta, etc.)
                break;
        }

        current_ = current_->next;
    }

    currentTime_ = targetTime;

    // Check if we've reached the end
    if (!current_) {
        finished_.store(true);
        playing_.store(false);
    }
}
