//
// Created by opus arc on 2026/3/20.
//

#ifndef MOTIF_MYFFT_H
#define MOTIF_MYFFT_H
#include <vector>

#include "Public.h"


class MyFFT {
public:
    static std::vector<float> prepareMonoFrameForFFT(const M4a &audio,
                                                     size_t fftSize,
                                                     size_t frameStart);

    static std::vector<float> computeMagnitudeSpectrum(const std::vector<float> &frame);

    static std::vector<float> computeChromaFromMagnitudeSpectrum(
        const std::vector<float> &magnitude,
        unsigned int sampleRate,
        size_t fftSize);

    static std::vector<std::vector<float>> computeChromaSequence(const M4a &audio,
                                                             const size_t fftSize,
                                                             const size_t hopSize);
};


#endif //MOTIF_MYFFT_H
