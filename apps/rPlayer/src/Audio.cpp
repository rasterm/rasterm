/* SPDX-License-Identifier: Apache-2.0 */

#include <rPlayer/Audio.hpp>
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <iostream>

namespace rasterm::rPlayer {

    AudioPlayer::AudioPlayer() {}
    AudioPlayer::~AudioPlayer() { stop(); }

    bool AudioPlayer::openDemux(const std::string& path) {
        avformat_network_init();
        if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0) return false;
        if (avformat_find_stream_info(fmt, nullptr) < 0) return false;

        /* find best audio stream */

        audioStreamIndex = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audioStreamIndex < 0) return false;
        AVStream* st = fmt->streams[audioStreamIndex];
        const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!codec) return false;
        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx || avcodec_parameters_to_context(codecCtx, st->codecpar) < 0) return false;
        if (avcodec_open2(codecCtx, codec, nullptr) < 0) return false;

        pkt = av_packet_alloc();
        frame = av_frame_alloc();
        if (!pkt || !frame) return false;

        return initResampler();
    }

    bool AudioPlayer::initResampler() {

        /* output: float32, outSampleRate, stereo */

        swr = swr_alloc();
        if (!swr) return false;

        AVChannelLayout outLayout; av_channel_layout_default(&outLayout, outChannels);
        AVChannelLayout inLayout = codecCtx->ch_layout;

        av_opt_set_chlayout(swr, "in_chlayout", &inLayout, 0);
        av_opt_set_int(swr, "in_sample_rate", codecCtx->sample_rate, 0);
        av_opt_set_sample_fmt(swr, "in_sample_fmt", codecCtx->sample_fmt, 0);

        av_opt_set_chlayout(swr, "out_chlayout", &outLayout, 0);
        av_opt_set_int(swr, "out_sample_rate", outSampleRate, 0);
        av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

        if (swr_init(swr) < 0) return false;
        return true;
    }

    void AudioPlayer::closeDemux() {
        if (pkt) { av_packet_free(&pkt); pkt = nullptr; }
        if (frame) { av_frame_free(&frame); frame = nullptr; }
        if (swr) { swr_free(&swr); swr = nullptr; }
        if (codecCtx) { avcodec_free_context(&codecCtx); codecCtx = nullptr; }
        if (fmt) { avformat_close_input(&fmt); fmt = nullptr; }
        audioStreamIndex = -1;
    }

    bool AudioPlayer::queueDecodedFrame() {
        const int inSamples = frame->nb_samples;
        const int64_t required = av_rescale_rnd(swr_get_delay(swr, codecCtx->sample_rate) + inSamples,
            outSampleRate, codecCtx->sample_rate, AV_ROUND_UP);
        if (required <= 0 || required > INT_MAX) {
            return false;
        }

        decodeBuffer.resize(static_cast<size_t>(required) * outChannels);
        uint8_t* output[1] = { reinterpret_cast<uint8_t*>(decodeBuffer.data()) };
        const uint8_t* input[AV_NUM_DATA_POINTERS]{};
        const int inputPlanes = std::min<int>(frame->ch_layout.nb_channels, AV_NUM_DATA_POINTERS);
        for (int plane = 0; plane < inputPlanes; ++plane) {
            input[plane] = frame->extended_data[plane];
        }
        const int outputFrames = swr_convert(swr, output, static_cast<int>(required),
            input, inSamples);
        if (outputFrames <= 0) {
            return outputFrames == 0;
        }

        size_t queued = 0;
        while (queued < static_cast<size_t>(outputFrames) && running.load(std::memory_order_acquire)) {
            const size_t pushed = ring.push(decodeBuffer.data() + queued * outChannels, static_cast<size_t>(outputFrames) - queued);
            queued += pushed;
            if (pushed == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        return true;
    }

    void AudioPlayer::decodeThreadRun() {
        while (running.load(std::memory_order_acquire)) {
            const int readResult = av_read_frame(fmt, pkt);
            if (readResult < 0) {
                avcodec_send_packet(codecCtx, nullptr);
            }
            else if (pkt->stream_index == audioStreamIndex) {
                avcodec_send_packet(codecCtx, pkt);
            }
            av_packet_unref(pkt);

            while (running.load(std::memory_order_acquire)) {
                const int receiveResult = avcodec_receive_frame(codecCtx, frame);
                if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF) {
                    break;
                }
                if (receiveResult < 0 || !queueDecodedFrame()) {
                    decodeFinished.store(true, std::memory_order_release);
                    return;
                }
                av_frame_unref(frame);
            }

            if (readResult < 0) {
                decodeFinished.store(true, std::memory_order_release);
                return;
            }
        }
        decodeFinished.store(true, std::memory_order_release);
    }

    static void onAudioFramesShim(ma_device* device, void* output, const void* input, ma_uint32 frameCount) {
        (void)input;
        AudioPlayer* self = reinterpret_cast<AudioPlayer*>(device->pUserData);
        self->mixCallback(reinterpret_cast<float*>(output), (uint32_t)frameCount);
    }

    void AudioPlayer::mixCallback(float* out, uint32_t frames) {
        const size_t read = ring.pop(out, frames);
        if (read < frames) {
            std::fill(out + read * outChannels, out + static_cast<size_t>(frames) * outChannels, 0.0f);
            underruns.fetch_add(1, std::memory_order_relaxed);
        }
        playedFrames.fetch_add(frames, std::memory_order_release);
    }

    double AudioPlayer::playbackClockSeconds() const noexcept {
        return static_cast<double>(playedFrames.load(std::memory_order_acquire)) / outSampleRate;
    }

    void AudioPlayer::waitForInitialBuffer() {
        const size_t targetFrames = static_cast<size_t>(outSampleRate) / 4;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (running.load(std::memory_order_acquire) && !decodeFinished.load(std::memory_order_acquire) &&
               ring.readAvailable() < targetFrames && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    bool AudioPlayer::start(const std::string& path, const Params& params) {
        if (running.load()) return true;
        outSampleRate = params.outSampleRate;
        outChannels = params.outChannels;

        /* prepare ring buffer (~3 seconds) */

        ring.reset(static_cast<size_t>(outSampleRate) * 3);
        playedFrames.store(0, std::memory_order_release);
        underruns.store(0, std::memory_order_release);
        decodeFinished.store(false, std::memory_order_release);

        if (!openDemux(path)) {
            std::cerr << "AudioPlayer: failed to open demux for " << path << "\n";
            closeDemux();
            return false;
        }

        /* init miniaudio device */

        ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
        cfg.playback.format = ma_format_f32;
        cfg.playback.channels = (ma_uint32)outChannels;
        cfg.sampleRate = (ma_uint32)outSampleRate;
        cfg.dataCallback = onAudioFramesShim;
        cfg.pUserData = this;

        device = new ma_device{};
        if (ma_device_init(nullptr, &cfg, device) != MA_SUCCESS) {
            std::cerr << "AudioPlayer: failed to init audio device\n";
            closeDemux();
            delete device; device = nullptr;
            return false;
        }
        deviceInitialized = true;

        running.store(true, std::memory_order_release);
        decodeThread = std::thread([this]() { decodeThreadRun(); });
        waitForInitialBuffer();

        if (ma_device_start(device) != MA_SUCCESS) {
            std::cerr << "AudioPlayer: failed to start audio device\n";
            running.store(false, std::memory_order_release);
            if (decodeThread.joinable()) decodeThread.join();
            ma_device_uninit(device);
            deviceInitialized = false;
            delete device; device = nullptr;
            closeDemux();
            return false;
        }

        return true;
    }

    void AudioPlayer::stop() {
        bool wasRunning = running.exchange(false);
        if (wasRunning) {
            if (decodeThread.joinable()) decodeThread.join();
        }
        if (deviceInitialized && device) {
            ma_device_uninit(device);
            deviceInitialized = false;
            delete device; device = nullptr;
        }
        closeDemux();
    }

}