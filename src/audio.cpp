#include "audio.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <cstring>
#include <cstdio>

struct AudioOutput::Impl {
    SLObjectItf engineObject = nullptr;
    SLEngineItf engine = nullptr;
    SLObjectItf outputMixObject = nullptr;
    SLObjectItf playerObject = nullptr;
    SLPlayItf player = nullptr;
    SLAndroidSimpleBufferQueueItf bufferQueue = nullptr;

    // Double buffer
    int16_t buffers[NUM_BUFFERS][BUFFER_FRAMES * CHANNELS];
};

// File-local callback function for OpenSL ES
static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf /*bq*/, void* context) {
    auto* audio = static_cast<AudioOutput*>(context);
    audio->onBufferComplete();
}

AudioOutput::AudioOutput() : impl_(new Impl) {
    std::memset(impl_->buffers, 0, sizeof(impl_->buffers));
}

AudioOutput::~AudioOutput() {
    stop();

    if (impl_->playerObject) {
        (*impl_->playerObject)->Destroy(impl_->playerObject);
    }
    if (impl_->outputMixObject) {
        (*impl_->outputMixObject)->Destroy(impl_->outputMixObject);
    }
    if (impl_->engineObject) {
        (*impl_->engineObject)->Destroy(impl_->engineObject);
    }

    delete impl_;
}

void AudioOutput::onBufferComplete() {
    fillBuffer(currentBuffer_);
    currentBuffer_ = (currentBuffer_ + 1) % NUM_BUFFERS;
}

void AudioOutput::fillBuffer(int bufferIndex) {
    if (callback_) {
        callback_(impl_->buffers[bufferIndex], BUFFER_FRAMES);
    } else {
        std::memset(impl_->buffers[bufferIndex], 0, BUFFER_FRAMES * CHANNELS * sizeof(int16_t));
    }

    (*impl_->bufferQueue)->Enqueue(
        impl_->bufferQueue,
        impl_->buffers[bufferIndex],
        BUFFER_FRAMES * CHANNELS * sizeof(int16_t)
    );
}

bool AudioOutput::init(AudioCallback callback) {
    callback_ = std::move(callback);

    SLresult result;

    // Create engine
    result = slCreateEngine(&impl_->engineObject, 0, nullptr, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to create OpenSL ES engine\n");
        return false;
    }

    result = (*impl_->engineObject)->Realize(impl_->engineObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to realize OpenSL ES engine\n");
        return false;
    }

    result = (*impl_->engineObject)->GetInterface(impl_->engineObject, SL_IID_ENGINE, &impl_->engine);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to get engine interface\n");
        return false;
    }

    // Create output mix
    result = (*impl_->engine)->CreateOutputMix(impl_->engine, &impl_->outputMixObject, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to create output mix\n");
        return false;
    }

    result = (*impl_->outputMixObject)->Realize(impl_->outputMixObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to realize output mix\n");
        return false;
    }

    // Configure audio source (buffer queue)
    SLDataLocator_AndroidSimpleBufferQueue locBufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
        NUM_BUFFERS
    };

    SLDataFormat_PCM formatPcm = {
        SL_DATAFORMAT_PCM,
        CHANNELS,
        SL_SAMPLINGRATE_44_1,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        CHANNELS == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT) : SL_SPEAKER_FRONT_CENTER,
        SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataSource audioSrc = {&locBufq, &formatPcm};

    // Configure audio sink
    SLDataLocator_OutputMix locOutmix = {
        SL_DATALOCATOR_OUTPUTMIX,
        impl_->outputMixObject
    };

    SLDataSink audioSnk = {&locOutmix, nullptr};

    // Create audio player
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};

    result = (*impl_->engine)->CreateAudioPlayer(
        impl_->engine,
        &impl_->playerObject,
        &audioSrc,
        &audioSnk,
        1,
        ids,
        req
    );
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to create audio player\n");
        return false;
    }

    result = (*impl_->playerObject)->Realize(impl_->playerObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to realize audio player\n");
        return false;
    }

    result = (*impl_->playerObject)->GetInterface(impl_->playerObject, SL_IID_PLAY, &impl_->player);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to get play interface\n");
        return false;
    }

    result = (*impl_->playerObject)->GetInterface(impl_->playerObject, SL_IID_BUFFERQUEUE, &impl_->bufferQueue);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to get buffer queue interface\n");
        return false;
    }

    // Register callback
    result = (*impl_->bufferQueue)->RegisterCallback(impl_->bufferQueue, bufferQueueCallback, this);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to register buffer callback\n");
        return false;
    }

    return true;
}

bool AudioOutput::start() {
    if (!impl_->player) {
        return false;
    }

    running_.store(true);
    currentBuffer_ = 0;

    // Enqueue initial buffers
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        fillBuffer(i);
    }

    // Start playback
    SLresult result = (*impl_->player)->SetPlayState(impl_->player, SL_PLAYSTATE_PLAYING);
    if (result != SL_RESULT_SUCCESS) {
        std::fprintf(stderr, "Failed to start playback\n");
        running_.store(false);
        return false;
    }

    return true;
}

void AudioOutput::stop() {
    running_.store(false);

    if (impl_->player) {
        (*impl_->player)->SetPlayState(impl_->player, SL_PLAYSTATE_STOPPED);
    }

    if (impl_->bufferQueue) {
        (*impl_->bufferQueue)->Clear(impl_->bufferQueue);
    }
}
