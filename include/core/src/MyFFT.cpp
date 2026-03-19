//
// Created by opus arc on 2026/3/20.
//

#include <MyFFT.h>

#include "Public.h"
#include <vector>

#include "fft/kiss_fftr.h"

namespace {
    std::vector<float> downmixToMonoInternal(const M4a &audio) {
        if (audio.channels == 0 || audio.data.empty()) {
            return {};
        }

        if (audio.channels == 1) {
            return audio.data;
        }

        const size_t frameCount = static_cast<size_t>(audio.totalFrameCount);
        std::vector<float> mono(frameCount, 0.0f);

        for (size_t i = 0; i < frameCount; ++i) {
            float sum = 0.0f;
            for (unsigned int ch = 0; ch < audio.channels; ++ch) {
                sum += audio.data[i * audio.channels + ch];
            }
            mono[i] = sum / static_cast<float>(audio.channels);
        }

        return mono;
    }

    std::vector<float> makeHannWindowInternal(const size_t fftSize) {
        std::vector<float> window(fftSize, 0.0f);
        if (fftSize <= 1) {
            return window;
        }

        constexpr float twoPi = 6.28318530717958647692f;
        for (size_t n = 0; n < fftSize; ++n) {
            window[n] = 0.5f - 0.5f * std::cos(twoPi * static_cast<float>(n) /
                                               static_cast<float>(fftSize - 1));
        }
        return window;
    }
}

std::vector<float> MyFFT::prepareMonoFrameForFFT(const M4a &audio,
                                                 const size_t fftSize,
                                                 const size_t frameStart) {
    const std::vector<float> mono = downmixToMonoInternal(audio);
    if (mono.empty() || fftSize == 0) {
        return {};
    }

    const std::vector<float> hannWindow = makeHannWindowInternal(fftSize);
    std::vector<float> frame(fftSize, 0.0f);

    for (size_t i = 0; i < fftSize; ++i) {
        const size_t index = frameStart + i;
        const float sample = index < mono.size() ? mono[index] : 0.0f;
        frame[i] = sample * hannWindow[i];
    }

    return frame;
}

std::vector<float> MyFFT::computeMagnitudeSpectrum(const std::vector<float> &frame) {
    if (frame.empty()) {
        return {};
    }

    const int fftSize = static_cast<int>(frame.size());
    if (fftSize < 2) {
        return {};
    }

    kiss_fftr_cfg cfg = kiss_fftr_alloc(fftSize, 0, nullptr, nullptr);
    if (cfg == nullptr) {
        return {};
    }

    std::vector<kiss_fft_cpx> spectrum(static_cast<size_t>(fftSize / 2 + 1));
    kiss_fftr(cfg, frame.data(), spectrum.data());

    std::vector<float> magnitude(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
    for (size_t k = 0; k < magnitude.size(); ++k) {
        const float re = spectrum[k].r;
        const float im = spectrum[k].i;
        magnitude[k] = std::sqrt(re * re + im * im);
    }

    free(cfg);
    return magnitude;
}

std::vector<float> MyFFT::computeChromaFromMagnitudeSpectrum(
    const std::vector<float>& magnitude,
    const unsigned int sampleRate,
    const size_t fftSize) {

    if (magnitude.empty() || sampleRate == 0 || fftSize < 2) {
        return {};
    }

    std::vector<float> chroma(12, 0.0f);

    for (size_t k = 1; k < magnitude.size(); ++k) {
        const float freq = static_cast<float>(k) * static_cast<float>(sampleRate) /
                           static_cast<float>(fftSize);

        if (freq < 27.5f) {
            continue;
        }

        const float midi = 69.0f + 12.0f * std::log2(freq / 440.0f);
        const int roundedMidi = static_cast<int>(std::lround(midi));
        const int pitchClass = ((roundedMidi % 12) + 12) % 12;

        chroma[static_cast<size_t>(pitchClass)] += magnitude[k];
    }

    float sum = 0.0f;
    for (const float value : chroma) {
        sum += value;
    }

    if (sum > 0.0f) {
        for (float &value : chroma) {
            value /= sum;
        }
    }

    return chroma;
}

std::vector<std::vector<float>> MyFFT::computeChromaSequence(const M4a &audio,
                                                             const size_t fftSize,
                                                             const size_t hopSize) {
    if (fftSize == 0 || hopSize == 0 || audio.sampleRate == 0 || audio.data.empty()) {
        return {};
    }

    const std::vector<float> mono = downmixToMonoInternal(audio);
    if (mono.empty()) {
        return {};
    }

    const std::vector<float> hannWindow = makeHannWindowInternal(fftSize);

    std::vector<std::vector<float>> chromaSequence;

    for (size_t frameStart = 0; frameStart < mono.size(); frameStart += hopSize) {
        std::vector<float> frame(fftSize, 0.0f);

        for (size_t i = 0; i < fftSize; ++i) {
            const size_t index = frameStart + i;
            const float sample = index < mono.size() ? mono[index] : 0.0f;
            frame[i] = sample * hannWindow[i];
        }

        const std::vector<float> magnitude = computeMagnitudeSpectrum(frame);
        if (magnitude.empty()) {
            continue;
        }

        const std::vector<float> chroma = computeChromaFromMagnitudeSpectrum(magnitude,
                                                                             audio.sampleRate,
                                                                             fftSize);
        if (!chroma.empty()) {
            chromaSequence.push_back(chroma);
        }

        if (frameStart + hopSize >= mono.size()) {
            break;
        }
    }

    return chromaSequence;
}
