#ifdef USE_ALSA

#include "alsa_input.h"
#include "synth.h"
#include <alsa/asoundlib.h>
#include <cstdio>

struct AlsaInput::Impl {
    snd_seq_t* seq = nullptr;
    int port = -1;
    int client = -1;
};

AlsaInput::AlsaInput(Synthesizer& synth)
    : synth_(synth), impl_(new Impl) {
}

AlsaInput::~AlsaInput() {
    stop();
    delete impl_;
}

bool AlsaInput::start(const std::string& clientName, QuitCallback onQuit) {
    if (running_.load()) {
        return false;
    }

    // Open ALSA sequencer
    int err = snd_seq_open(&impl_->seq, "default", SND_SEQ_OPEN_INPUT, 0);
    if (err < 0) {
        std::fprintf(stderr, "Failed to open ALSA sequencer: %s\n", snd_strerror(err));
        return false;
    }

    // Set client name
    err = snd_seq_set_client_name(impl_->seq, clientName.c_str());
    if (err < 0) {
        std::fprintf(stderr, "Failed to set client name: %s\n", snd_strerror(err));
        snd_seq_close(impl_->seq);
        impl_->seq = nullptr;
        return false;
    }

    impl_->client = snd_seq_client_id(impl_->seq);

    // Create input port
    impl_->port = snd_seq_create_simple_port(
        impl_->seq,
        "MIDI In",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_SYNTHESIZER | SND_SEQ_PORT_TYPE_APPLICATION
    );

    if (impl_->port < 0) {
        std::fprintf(stderr, "Failed to create ALSA port: %s\n", snd_strerror(impl_->port));
        snd_seq_close(impl_->seq);
        impl_->seq = nullptr;
        return false;
    }

    std::printf("ALSA MIDI port created: %d:%d (%s)\n", impl_->client, impl_->port, clientName.c_str());
    std::printf("Connect with: aconnect <source> %d:%d\n", impl_->client, impl_->port);

    quitCallback_ = std::move(onQuit);
    running_.store(true);
    inputThread_ = std::thread(&AlsaInput::inputLoop, this);

    return true;
}

void AlsaInput::stop() {
    running_.store(false);

    if (inputThread_.joinable()) {
        // Wake up the blocking read by closing the sequencer
        if (impl_->seq) {
            snd_seq_close(impl_->seq);
            impl_->seq = nullptr;
        }
        inputThread_.join();
    }

    if (impl_->seq) {
        snd_seq_close(impl_->seq);
        impl_->seq = nullptr;
    }
}

std::string AlsaInput::getPortName() const {
    if (impl_->client >= 0 && impl_->port >= 0) {
        return std::to_string(impl_->client) + ":" + std::to_string(impl_->port);
    }
    return "";
}

void AlsaInput::inputLoop() {
    snd_seq_event_t* ev;

    while (running_.load()) {
        int err = snd_seq_event_input(impl_->seq, &ev);
        if (err < 0) {
            if (err == -EAGAIN) continue;
            break;  // Error or closed
        }

        if (!ev) continue;

        switch (ev->type) {
            case SND_SEQ_EVENT_NOTEON:
                if (ev->data.note.velocity > 0) {
                    synth_.noteOn(ev->data.note.channel, ev->data.note.note,
                                  ev->data.note.velocity / 127.0f);
                } else {
                    synth_.noteOff(ev->data.note.channel, ev->data.note.note);
                }
                break;

            case SND_SEQ_EVENT_NOTEOFF:
                synth_.noteOff(ev->data.note.channel, ev->data.note.note);
                break;

            case SND_SEQ_EVENT_CONTROLLER:
                synth_.controlChange(ev->data.control.channel,
                                     ev->data.control.param,
                                     ev->data.control.value);
                break;

            case SND_SEQ_EVENT_PGMCHANGE:
                synth_.programChange(ev->data.control.channel,
                                     ev->data.control.value);
                break;

            case SND_SEQ_EVENT_PITCHBEND:
                // ALSA pitch bend is -8192 to 8191, convert to 0-16383
                synth_.pitchBend(ev->data.control.channel,
                                 ev->data.control.value + 8192);
                break;

            case SND_SEQ_EVENT_CONTROL14:
                // 14-bit controller
                synth_.controlChange(ev->data.control.channel,
                                     ev->data.control.param,
                                     ev->data.control.value >> 7);
                break;

            default:
                // Ignore other event types
                break;
        }

        snd_seq_free_event(ev);
    }

    running_.store(false);
    if (quitCallback_) {
        quitCallback_();
    }
}

#else // !USE_ALSA

// Stub implementation when ALSA is not available
#include "alsa_input.h"
#include <cstdio>

struct AlsaInput::Impl {};

AlsaInput::AlsaInput(Synthesizer& synth) : synth_(synth), impl_(nullptr) {}
AlsaInput::~AlsaInput() {}

bool AlsaInput::start(const std::string&, QuitCallback) {
    std::fprintf(stderr, "ALSA support not compiled in\n");
    return false;
}

void AlsaInput::stop() {}
std::string AlsaInput::getPortName() const { return ""; }

#endif // USE_ALSA
