//
// Created by opus arc on 2026/3/20.
//

#include <MyDrWav.h>

#include "Logger.h"
#include "MyPath.h"
#include "dr_wav/dr_wav.h"

M4a MyDrWav::getM4a_float(const std::string& title) {
    try {
        unsigned int channels;
        unsigned int sampleRate;
        drwav_uint64 totalFrameCount;

        const std::string wavName = title + ".wav";

        float* samples = drwav_open_file_and_read_pcm_frames_f32(
            std::filesystem::path(MyPath::getM4aFolderPath() / wavName).c_str(),
            &channels,
            &sampleRate,
            &totalFrameCount,
            nullptr
        );

        if (!samples) {
            throw std::runtime_error("Failed to load WAV");
        }

        std::cout << "title: " << title << "\n";
        std::cout << "channels: " << channels << "\n";
        std::cout << "sampleRate: " << sampleRate << "\n";
        std::cout << "frames: " << totalFrameCount << "\n";

        // 总样本数 = frameCount * channels
        std::vector<float> data(samples, samples + totalFrameCount * channels);

        drwav_free(samples, nullptr);

        M4a m4a;
        m4a.channels = channels;
        m4a.sampleRate = sampleRate;
        m4a.totalFrameCount = totalFrameCount;
        m4a.data = data;

        return m4a;
    } catch (const std::exception& e) {
        std::cerr << e.what();
        M4a m4a{};
        return m4a;
    }
}
