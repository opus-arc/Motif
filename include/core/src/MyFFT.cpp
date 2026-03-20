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

    void computeMagnitudeSpectrumInPlaceInternal(const std::vector<float>& frame,
                                                 kiss_fftr_cfg cfg,
                                                 std::vector<kiss_fft_cpx>& spectrum,
                                                 std::vector<float>& magnitude) {
        if (frame.empty() || cfg == nullptr) {
            magnitude.clear();
            return;
        }

        kiss_fftr(cfg, frame.data(), spectrum.data());

        const size_t binCount = magnitude.size();
        for (size_t k = 0; k < binCount; ++k) {
            const float re = spectrum[k].r;
            const float im = spectrum[k].i;
            magnitude[k] = std::sqrt(re * re + im * im);
        }
    }

    void computeChromaFromMagnitudeSpectrumInPlaceInternal(const std::vector<float>& magnitude,
                                                           const unsigned int sampleRate,
                                                           const size_t fftSize,
                                                           std::vector<float>& chroma) {
        if (magnitude.empty() || sampleRate == 0 || fftSize < 2) {
            chroma.clear();
            return;
        }

        chroma.assign(12, 0.0f);

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
            for (float& value : chroma) {
                value /= sum;
            }
        }
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
    const size_t spectrumSize = fftSize / 2 + 1;
    const size_t estimatedFrames = (mono.size() + hopSize - 1) / hopSize;

    kiss_fftr_cfg cfg = kiss_fftr_alloc(static_cast<int>(fftSize), 0, nullptr, nullptr);
    if (cfg == nullptr) {
        return {};
    }

    std::vector<std::vector<float>> chromaSequence;
    chromaSequence.reserve(estimatedFrames);

    std::vector<float> frame(fftSize, 0.0f);
    std::vector<kiss_fft_cpx> spectrum(spectrumSize);
    std::vector<float> magnitude(spectrumSize, 0.0f);
    std::vector<float> chroma(12, 0.0f);

    for (size_t frameStart = 0; frameStart < mono.size(); frameStart += hopSize) {
        for (size_t i = 0; i < fftSize; ++i) {
            const size_t index = frameStart + i;
            const float sample = index < mono.size() ? mono[index] : 0.0f;
            frame[i] = sample * hannWindow[i];
        }

        computeMagnitudeSpectrumInPlaceInternal(frame, cfg, spectrum, magnitude);
        if (magnitude.empty()) {
            continue;
        }

        computeChromaFromMagnitudeSpectrumInPlaceInternal(magnitude,
                                                          audio.sampleRate,
                                                          fftSize,
                                                          chroma);
        if (!chroma.empty()) {
            chromaSequence.push_back(chroma);
        }

        if (frameStart + hopSize >= mono.size()) {
            break;
        }
    }

    free(cfg);
    return chromaSequence;
}
