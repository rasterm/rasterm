/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

/* FFmpeg forward declarations */

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

/* forward declare miniaudio device to avoid including implementation in header */

struct ma_device;

namespace rasterm::rPlayer {

    /* single producer / single consumer PCM ring buffer. the decode thread is
       the sole writer and the audio callback is the sole reader. */

    class PcmRingBuffer {
    public:
        PcmRingBuffer() = default;
        explicit PcmRingBuffer(size_t framesCapacity) { reset(framesCapacity); }

        void reset(size_t framesCapacity) {
            data.resize(framesCapacity * channels);
            capacityFrames = framesCapacity;
            readFrame.store(0, std::memory_order_relaxed);
            writeFrame.store(0, std::memory_order_relaxed);
        }

        [[nodiscard]] size_t writeAvailable() const {
            const auto write = writeFrame.load(std::memory_order_relaxed);
            const auto read = readFrame.load(std::memory_order_acquire);
            return capacityFrames - static_cast<size_t>(write - read);
        }

        [[nodiscard]] size_t readAvailable() const {
            const auto read = readFrame.load(std::memory_order_relaxed);
            const auto write = writeFrame.load(std::memory_order_acquire);
            return static_cast<size_t>(write - read);
        }

        size_t push(const float* src, size_t frames) {
            const auto write = writeFrame.load(std::memory_order_relaxed);
            const auto read = readFrame.load(std::memory_order_acquire);
            const size_t framesToWrite = (std::min)(frames, capacityFrames - static_cast<size_t>(write - read));
            for (size_t i = 0; i < framesToWrite; ++i) {
                const size_t idx = ((write + i) % capacityFrames) * channels;
                data[idx + 0] = src[i * channels + 0];
                data[idx + 1] = src[i * channels + 1];
            }
            writeFrame.store(write + framesToWrite, std::memory_order_release);
            return framesToWrite;
        }

        size_t pop(float* dst, size_t frames) {
            const auto read = readFrame.load(std::memory_order_relaxed);
            const auto write = writeFrame.load(std::memory_order_acquire);
            const size_t framesToRead = (std::min)(frames, static_cast<size_t>(write - read));
            for (size_t i = 0; i < framesToRead; ++i) {
                const size_t idx = ((read + i) % capacityFrames) * channels;
                dst[i * channels + 0] = data[idx + 0];
                dst[i * channels + 1] = data[idx + 1];
            }
            readFrame.store(read + framesToRead, std::memory_order_release);
            return framesToRead;
        }

    private:
        static constexpr size_t channels = 2; /* stereo */
        std::vector<float> data;
        size_t capacityFrames = 0;
        std::atomic<uint64_t> readFrame{ 0 };
        std::atomic<uint64_t> writeFrame{ 0 };
    };

    class AudioPlayer {
    public:
        struct Params {
            int outSampleRate = 48000;
            int outChannels = 2; /* stereo */
        };

        AudioPlayer();
        ~AudioPlayer();

        bool start(const std::string& path, const Params& params = {});
        void stop();

        [[nodiscard]] double playbackClockSeconds() const noexcept;
        [[nodiscard]] uint64_t underrunCount() const noexcept { return underruns.load(std::memory_order_acquire); }
        [[nodiscard]] bool isActive() const noexcept { return running.load(std::memory_order_acquire); }

        /* expose for device callback shim */

        void mixCallback(float* out, uint32_t frames);

    private:


        /* FFmpeg pipeline */

        bool openDemux(const std::string& path);
        void decodeThreadRun();
        bool initResampler();
        void closeDemux();
        bool queueDecodedFrame();
        void waitForInitialBuffer();

        /* state */

        std::atomic<bool> running{ false };
        std::thread decodeThread;
        std::atomic<uint64_t> playedFrames{ 0 };
        std::atomic<uint64_t> underruns{ 0 };
        std::atomic<bool> decodeFinished{ false };

        /* output format */

        int outSampleRate = 48000;
        int outChannels = 2;

        /* ring buffer (store ~3 seconds by default) */

        PcmRingBuffer ring;

        /* miniaudio device */

        ma_device* device = nullptr;
        bool deviceInitialized = false;

        /* FFmpeg objects */

        AVFormatContext* fmt = nullptr;
        AVCodecContext* codecCtx = nullptr;
        SwrContext* swr = nullptr;
        int audioStreamIndex = -1;
        AVPacket* pkt = nullptr;
        AVFrame* frame = nullptr;
        std::vector<float> decodeBuffer;
    };

}