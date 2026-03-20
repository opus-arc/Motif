//
// Created by opus arc on 2026/3/7.
//

#include "../MyFfmpeg.h"


#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Cmd.h"
#include "Logger.h"
#include "MyPath.h"
#include "Public.h"
#include <utility>

namespace {
    std::string shellQuote(const std::string &s) {
        std::string out = "'";
        for (char ch: s) {
            if (ch == '\'') {
                out += "'\\''";
            } else {
                out += ch;
            }
        }
        out += "'";
        return out;
    }

    std::string trimCopy(const std::string &s) {
        const std::string whitespace = " \t\r\n";
        const auto begin = s.find_first_not_of(whitespace);
        if (begin == std::string::npos) {
            return "";
        }
        const auto end = s.find_last_not_of(whitespace);
        return s.substr(begin, end - begin + 1);
    }

    std::string sanitizeFolderName(const std::string &name) {
        std::string out;
        out.reserve(name.size());

        for (char ch: name) {
            if (ch == '/' || ch == ':' || ch == '\\') {
                out += '_';
            } else {
                out += ch;
            }
        }

        out = trimCopy(out);
        return out.empty() ? "Unknown Album" : out;
    }

    std::string getM4aAlbumName(const std::filesystem::path &m4aPath) {
        std::ostringstream cmd;
        cmd << "ffprobe -v error "
                << "-show_entries format_tags=album "
                << "-of default=noprint_wrappers=1:nokey=1 "
                << shellQuote(m4aPath.string());

        const std::string output = trimCopy(Cmd::runCmdCapture(cmd.str()));
        return output.empty() ? "Unknown Album" : output;
    }

    int getAudioBitDepth(const std::filesystem::path &audioPath) {
        std::ostringstream cmd;
        cmd << "ffprobe -v error "
                << "-select_streams a:0 "
                << "-show_entries stream=bits_per_raw_sample,bits_per_sample "
                << "-of default=noprint_wrappers=1:nokey=1 "
                << shellQuote(audioPath.string());

        const std::string output = trimCopy(Cmd::runCmdCapture(cmd.str()));
        if (output.empty()) {
            throw std::runtime_error("ffprobe returned empty bit depth for: " + audioPath.string());
        }

        std::istringstream iss(output);
        std::string line;
        while (std::getline(iss, line)) {
            line = trimCopy(line);
            if (line.empty() || line == "N/A") {
                continue;
            }

            try {
                return std::stoi(line);
            } catch (const std::exception &) {
            }
        }

        throw std::runtime_error("Failed to parse bit depth from ffprobe output: " + output);
    }

    int getFlacBitDepth(const std::filesystem::path &flacPath) {
        return getAudioBitDepth(flacPath);
    }

    bool extractCoverFromM4a(const std::filesystem::path &m4aPath, const std::filesystem::path &coverPath) {
        if (std::filesystem::exists(coverPath)) {
            std::filesystem::remove(coverPath);
        }

        std::ostringstream cmd;
        cmd << "ffmpeg -y -i " << shellQuote(m4aPath.string())
                << " -an -map 0:v:0 -c:v copy "
                << shellQuote(coverPath.string());

        try {
            Cmd::runCmdCapture(cmd.str());
            return std::filesystem::exists(coverPath);
        } catch (const std::exception &) {
            return false;
        }
    }

    void applyMacFolderIcon(const std::filesystem::path &folderPath, const std::filesystem::path &imagePath) {
        // fileicon set
        // "/Users/opusarc/CLionProjects/MusicCat/output/Mcat Library/A Moment in Time"
        // "/Users/opusarc/CLionProjects/MusicCat/output/Mcat Library/A Moment in Time/Folder.jpg"
        const std::string cmd = "fileicon set \"" + folderPath.string() + "\" \"" + imagePath.string() + "\"";
        Cmd::runCmdCapture(cmd);
    }

    std::filesystem::path makeUniqueDestination(const std::filesystem::path &targetPath) {
        if (!std::filesystem::exists(targetPath)) {
            return targetPath;
        }

        const std::filesystem::path parent = targetPath.parent_path();
        const std::string stem = targetPath.stem().string();
        const std::string ext = targetPath.extension().string();

        for (int i = 1; i <= 9999; ++i) {
            const std::filesystem::path candidate = parent / (stem + " (" + std::to_string(i) + ")" + ext);
            if (!std::filesystem::exists(candidate)) {
                return candidate;
            }
        }

        throw std::runtime_error("Failed to allocate unique destination path for: " + targetPath.string());
    }

    std::filesystem::path makeTempF32Path(const std::filesystem::path &outputFolder,
                                          const std::string &title) {
        const std::filesystem::path basePath = outputFolder / (title + ".decode.tmp.f32");
        if (!std::filesystem::exists(basePath)) {
            return basePath;
        }

        for (int i = 1; i <= 9999; ++i) {
            const std::filesystem::path candidate =
                outputFolder / (title + ".decode.tmp." + std::to_string(i) + ".f32");
            if (!std::filesystem::exists(candidate)) {
                return candidate;
            }
        }

        throw std::runtime_error("Failed to allocate temp f32 path for: " + title);
    }
    double getM4aSampleRate(const std::filesystem::path &m4aPath) {
        std::ostringstream cmd;
        cmd << "ffprobe -v error "
            << "-select_streams a:0 "
            << "-show_entries stream=sample_rate "
            << "-of default=noprint_wrappers=1:nokey=1 "
            << shellQuote(m4aPath.string());

        const std::string output = trimCopy(Cmd::runCmdCapture(cmd.str()));
        if (output.empty()) {
            throw std::runtime_error("ffprobe returned empty sample rate for: " + m4aPath.string());
        }

        try {
            return std::stod(output);
        } catch (const std::exception &) {
            throw std::runtime_error("Failed to parse sample rate from ffprobe output: " + output);
        }
    }

    int getM4aChannels(const std::filesystem::path &m4aPath) {
        std::ostringstream cmd;
        cmd << "ffprobe -v error "
            << "-select_streams a:0 "
            << "-show_entries stream=channels "
            << "-of default=noprint_wrappers=1:nokey=1 "
            << shellQuote(m4aPath.string());

        const std::string output = trimCopy(Cmd::runCmdCapture(cmd.str()));
        if (output.empty()) {
            throw std::runtime_error("ffprobe returned empty channels for: " + m4aPath.string());
        }

        try {
            return std::stoi(output);
        } catch (const std::exception &) {
            throw std::runtime_error("Failed to parse channels from ffprobe output: " + output);
        }
    }

    double getAudioDurationSeconds(const std::filesystem::path &audioPath) {
        std::ostringstream cmd;
        cmd << "ffprobe -v error "
            << "-show_entries format=duration "
            << "-of default=noprint_wrappers=1:nokey=1 "
            << shellQuote(audioPath.string());

        const std::string output = trimCopy(Cmd::runCmdCapture(cmd.str()));
        if (output.empty()) {
            throw std::runtime_error("ffprobe returned empty duration for: " + audioPath.string());
        }

        try {
            return std::stod(output);
        } catch (const std::exception &) {
            throw std::runtime_error("Failed to parse duration from ffprobe output: " + output);
        }
    }

    std::filesystem::path findAudioFileByTitleInFolder(const std::filesystem::path &folderPath,
                                                       const std::string &title) {
        if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath)) {
            return {};
        }

        static const std::vector<std::string> extensions = {
            ".m4a", ".mp4", ".flac", ".wav", ".mp3", ".aac", ".aiff", ".aif", ".caf", ".ogg", ".opus"
        };

        for (const auto &ext: extensions) {
            const std::filesystem::path candidate = folderPath / (title + ext);
            if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
                return candidate;
            }
        }

        for (const auto &entry: std::filesystem::directory_iterator(folderPath)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const std::filesystem::path candidate = entry.path();
            if (candidate.stem() == title) {
                return candidate;
            }
        }

        return {};
    }

    std::filesystem::path findAudioFileByTitle(const std::string &title) {
        const std::filesystem::path m4aFolderPath = MyPath::getWorkspaceFolderPath();

        std::filesystem::path candidate = findAudioFileByTitleInFolder(m4aFolderPath, title);
        if (!candidate.empty()) {
            return candidate;
        }

        if (!candidate.empty()) {
            return candidate;
        }

        throw std::runtime_error("Audio file does not exist for title: " + title);
    }

    std::filesystem::path makeTempSiblingPath(const std::filesystem::path &sourcePath,
                                              const std::string &suffix) {
        const std::filesystem::path parent = sourcePath.parent_path();
        const std::string stem = sourcePath.stem().string();
        const std::string ext = sourcePath.extension().string();
        const std::filesystem::path basePath = parent / (stem + suffix + ext);

        if (!std::filesystem::exists(basePath)) {
            return basePath;
        }

        for (int i = 1; i <= 9999; ++i) {
            const std::filesystem::path candidate =
                parent / (stem + suffix + "." + std::to_string(i) + ext);
            if (!std::filesystem::exists(candidate)) {
                return candidate;
            }
        }

        throw std::runtime_error("Failed to allocate temp sibling path for: " + sourcePath.string());
    }

    std::filesystem::path makeDerivedSiblingPath(const std::filesystem::path &sourcePath,
                                                 const std::string &suffix,
                                                 const std::string &newExtension = "") {
        const std::filesystem::path parent = sourcePath.parent_path();
        const std::string stem = sourcePath.stem().string();
        const std::string ext = newExtension.empty() ? sourcePath.extension().string() : newExtension;
        const std::filesystem::path basePath = parent / (stem + suffix + ext);

        if (!std::filesystem::exists(basePath)) {
            return basePath;
        }

        for (int i = 1; i <= 9999; ++i) {
            const std::filesystem::path candidate =
                parent / (stem + suffix + "." + std::to_string(i) + ext);
            if (!std::filesystem::exists(candidate)) {
                return candidate;
            }
        }

        throw std::runtime_error("Failed to allocate derived sibling path for: " + sourcePath.string());
    }
}



void MyFfmpeg::cutTheAudio(const std::string &title, const double startTime, const double endTime) {
    if (startTime < 0.0) {
        throw std::runtime_error("startTime must be >= 0 for title: " + title);
    }

    if (endTime <= startTime) {
        throw std::runtime_error("endTime must be greater than startTime for title: " + title);
    }

    const std::filesystem::path sourcePath = findAudioFileByTitle(title);
    const std::filesystem::path outputPath = makeDerivedSiblingPath(sourcePath, "_cut");
    const std::filesystem::path tempPath = makeTempSiblingPath(outputPath, ".tmp");

    if (std::filesystem::exists(tempPath)) {
        std::filesystem::remove(tempPath);
    }

    if (std::filesystem::exists(outputPath)) {
        std::filesystem::remove(outputPath);
    }

    std::ostringstream cmd;

    cmd << "ffmpeg -y -i " << shellQuote(sourcePath.string())
        << " -ss " << startTime
        << " -to " << endTime
        << " -map 0"
        << " -map_metadata 0"
        << " -c copy"
        << " -movflags use_metadata_tags "
        << shellQuote(tempPath.string());

    Cmd::runCmdCapture(cmd.str());

    std::filesystem::rename(tempPath, outputPath);
}

void MyFfmpeg::autoConvertedToWavByFileName(const std::string &title) {
    const std::filesystem::path sourcePath = findAudioFileByTitle(title);

    if (sourcePath.extension() == ".wav") {
        return;
    }

    const std::filesystem::path wavPath = sourcePath.parent_path() / (title + ".wav");
    const std::filesystem::path tempWavPath = makeTempSiblingPath(wavPath, ".tmp");

    if (std::filesystem::exists(tempWavPath)) {
        std::filesystem::remove(tempWavPath);
    }

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i " << shellQuote(sourcePath.string())
        << " -vn -acodec pcm_s16le "
        << shellQuote(tempWavPath.string());

    Cmd::runCmdCapture(cmd.str());

    std::filesystem::rename(tempWavPath, wavPath);
}

void MyFfmpeg::autoConvertedToM4aByFileName(const std::string &title, const std::string& newTitle) {
    const std::filesystem::path sourcePath = findAudioFileByTitle(title);

    if (sourcePath.extension() == ".m4a") {
        return;
    }

    const std::filesystem::path m4aPath = sourcePath.parent_path() / (newTitle + ".m4a");
    const std::filesystem::path tempM4aPath = makeTempSiblingPath(m4aPath, ".tmp");

    if (std::filesystem::exists(tempM4aPath)) {
        std::filesystem::remove(tempM4aPath);
    }

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i " << shellQuote(sourcePath.string())
        << " -vn -c:a aac -b:a 256k "
        << shellQuote(tempM4aPath.string());

    Cmd::runCmdCapture(cmd.str());

    std::filesystem::rename(tempM4aPath, m4aPath);
}

void MyFfmpeg::applyFade(const std::string &title, double fadeInSeconds, double fadeOutSeconds) {
    if (fadeInSeconds < 0.0 || fadeOutSeconds < 0.0) {
        throw std::runtime_error("Fade seconds must be >= 0 for title: " + title);
    }

    const std::filesystem::path sourcePath = findAudioFileByTitle(title);
    const double duration = getAudioDurationSeconds(sourcePath);

    if (fadeInSeconds + fadeOutSeconds > duration) {
        throw std::runtime_error("Fade duration exceeds audio duration for title: " + title);
    }

    const double fadeOutStart = duration - fadeOutSeconds;
    const std::filesystem::path outputPath = makeDerivedSiblingPath(sourcePath, "_fade");
    const std::filesystem::path tempPath = makeTempSiblingPath(outputPath, ".tmp");

    if (std::filesystem::exists(tempPath)) {
        std::filesystem::remove(tempPath);
    }

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i " << shellQuote(sourcePath.string())
        << " -af "
        << shellQuote(
            "afade=t=in:st=0:d=" + std::to_string(fadeInSeconds) +
            ",afade=t=out:st=" + std::to_string(fadeOutStart) +
            ":d=" + std::to_string(fadeOutSeconds)
        )
        << " -vn -c:a aac -b:a 256k "
        << shellQuote(tempPath.string());

    Cmd::runCmdCapture(cmd.str());
    std::filesystem::rename(tempPath, outputPath);
}