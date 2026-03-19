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

#include "Entity.h"
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
}

void MyFfmpeg::flacConvertedToM4aByFilename(const std::string &title) {
    const std::filesystem::path flacPath = Entity::getOutputFolderPath() / (title + ".flac");
    const std::filesystem::path m4aPath = Entity::getOutputFolderPath() / (title + ".m4a");

    if (!std::filesystem::exists(flacPath)) {
        throw std::runtime_error("FLAC file does not exist: " + flacPath.string());
    }

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i " << shellQuote(flacPath.string())
            << " -vn -c:a aac -b:a 256k "
            << shellQuote(m4aPath.string());

    Cmd::runCmdCapture(cmd.str());

    testLog("flacConvertedToM4aByFilename: 开始转换音频格式");
    testLog("flacConvertedToM4aByFilename: 线程: " + title);
    testLog("flacConvertedToM4aByFilename: flacPath: " + flacPath.string());
    testLog("flacConvertedToM4aByFilename: m4aPath: " + m4aPath.string());

    // std::cout << "转换 flac 为 m4a： " << std::endl;
    // std::cout << "flacPath: " << flacPath.string() << "  m4aPath: " << m4aPath.string() << std::endl;
}

int MyFfmpeg::getFlacDurationSecondsByFilename(const std::string &title) {
    const std::filesystem::path flacPath = Entity::getOutputFolderPath() / (title + ".flac");

    if (!std::filesystem::exists(flacPath)) {
        throw std::runtime_error("FLAC file does not exist: " + flacPath.string());
    }

    std::ostringstream cmd;
    cmd << "ffprobe -v error "
            << "-show_entries format=duration "
            << "-of default=noprint_wrappers=1:nokey=1 "
            << shellQuote(flacPath.string());

    const std::string output = Cmd::runCmdCapture(cmd.str());

    if (output.empty()) {
        throw std::runtime_error("ffprobe returned empty duration for: " + flacPath.string());
    }

    try {
        const double seconds = std::stod(output);
        return static_cast<int>(seconds + 0.5);
    } catch (const std::exception &) {
        throw std::runtime_error("Failed to parse duration from ffprobe output: " + output);
    }
}

int MyFfmpeg::getFlacBitDepthByFilename(const std::string &title) {
    const std::filesystem::path flacPath = Entity::getOutputFolderPath() / (title + ".flac");

    if (!std::filesystem::exists(flacPath)) {
        throw std::runtime_error("FLAC file does not exist: " + flacPath.string());
    }

    return getFlacBitDepth(flacPath);
}

// M4a MyFfmpeg::decodeM4aToFloatByFilename(const std::string &title) {
//     const std::filesystem::path m4aFolder = Path::getM4aFolderPath();
//     const std::filesystem::path m4aPath = m4aFolder / (title + ".m4a");
//
//     if (!std::filesystem::exists(m4aPath)) {
//         throw std::runtime_error("M4A file does not exist: " + m4aPath.string());
//     }
//
//     const double sampleRate = getM4aSampleRate(m4aPath);
//     const int channels = getM4aChannels(m4aPath);
//
//     const std::filesystem::path tempF32Path = makeTempF32Path(m4aFolder, title);
//
//     std::ostringstream cmd;
//     cmd << "ffmpeg -v error -y -i " << shellQuote(m4aPath.string())
//         << " -vn -f f32le -acodec pcm_f32le "
//         << shellQuote(tempF32Path.string());
//
//     Cmd::runCmdCapture(cmd.str());
//
//     std::ifstream in(tempF32Path, std::ios::binary | std::ios::ate);
//     if (!in) {
//         throw std::runtime_error("Failed to open decoded float file: " + tempF32Path.string());
//     }
//
//     const std::streamsize byteSize = in.tellg();
//     if (byteSize < 0) {
//         in.close();
//         std::filesystem::remove(tempF32Path);
//         throw std::runtime_error("Failed to determine decoded float file size: " + tempF32Path.string());
//     }
//
//     if ((byteSize % static_cast<std::streamsize>(sizeof(float))) != 0) {
//         in.close();
//         std::filesystem::remove(tempF32Path);
//         throw std::runtime_error("Decoded float file size is not aligned to float width: " + tempF32Path.string());
//     }
//
//     std::vector<float> samples(
//         static_cast<std::size_t>(byteSize / static_cast<std::streamsize>(sizeof(float)))
//     );
//
//     in.seekg(0, std::ios::beg);
//     if (!samples.empty()) {
//         in.read(reinterpret_cast<char *>(samples.data()), byteSize);
//         if (!in) {
//             in.close();
//             std::filesystem::remove(tempF32Path);
//             throw std::runtime_error("Failed to read decoded float data from: " + tempF32Path.string());
//         }
//     }
//
//     in.close();
//     std::filesystem::remove(tempF32Path);
//
//     return M4a{sampleRate, channels, std::move(samples)};
// }

void MyFfmpeg::applyCover(const std::string &title) {
    testLog("applyCover: 开始为 m4a 添加封面");
    testLog("applyCover: 线程: " + title);

    const std::filesystem::path opf = Entity::getOutputFolderPath();
    const std::filesystem::path m4aPath = opf / (title + ".m4a");
    const std::filesystem::path coverPath = opf / (title + ".jpg");
    const std::filesystem::path tempPath = opf / (title + ".cover.tmp.m4a");

    if (!std::filesystem::exists(m4aPath)) {
        Logger::info(
            std::string("[MyFfmpeg]: Skip applying cover because M4A file does not exist: ") + m4aPath.string());
        return;
    }

    if (!std::filesystem::exists(coverPath)) {
        Logger::info(
            std::string("[MyFfmpeg]: Skip applying cover because cover image does not exist: ") + coverPath.string());
        return;
    }

    if (std::filesystem::exists(tempPath)) {
        std::filesystem::remove(tempPath);
    }

    std::ostringstream cmd;
    cmd << "ffmpeg -y "
            << "-i " << shellQuote(m4aPath.string()) << ' '
            << "-i " << shellQuote(coverPath.string()) << ' '
            << "-map 0:a:0 -map 1:v:0 "
            << "-c:a copy "
            << "-c:v mjpeg "
            << "-disposition:v:0 attached_pic "
            << "-metadata:s:v:0 title=" << shellQuote("Cover") << ' '
            << "-metadata:s:v:0 comment=" << shellQuote("Cover (front)") << ' '
            << shellQuote(tempPath.string());

    Cmd::runCmdCapture(cmd.str());

    testLog("applyCover: m4aPath: " + m4aPath.string());

    std::filesystem::rename(tempPath, m4aPath);
    // Logger::info(std::string("[MyFfmpeg]: Attached front cover artwork to ") + m4aPath.string());
}

void MyFfmpeg::organizeAlbums(const std::string &title) {
    testLog("organizeAlbums: 开始整理指定的文件");
    testLog("organizeAlbums: 线程" + title);

    // std::cout << "开始整理指定文件: " << title << std::endl;

    const std::filesystem::path opf = Entity::getOutputFolderPath();
    const std::filesystem::path libraryRoot = opf / "Mcat Library";

    if (!std::filesystem::exists(opf) || !std::filesystem::is_directory(opf)) {
        throw std::runtime_error("Output folder does not exist or is not a directory: " + opf.string());
    }

    if (!std::filesystem::exists(libraryRoot)) {
        std::filesystem::create_directory(libraryRoot);
    }

    // 构造目标文件路径
    const std::filesystem::path m4aPath = opf / (title + ".m4a");
    const std::filesystem::path flacPath = opf / (title + ".flac");

    // 如果 m4a 不存在，直接退出（核心文件）
    if (!std::filesystem::exists(m4aPath) || !std::filesystem::is_regular_file(m4aPath)) {
        throw std::runtime_error("m4a file not found: " + m4aPath.string());
    }

    // ===== 读取专辑信息 =====
    const std::string rawAlbumName = getM4aAlbumName(m4aPath);
    const std::string albumFolderName = sanitizeFolderName(rawAlbumName);
    const std::filesystem::path albumFolderPath = libraryRoot / albumFolderName;

    const bool albumFolderAlreadyExists = std::filesystem::exists(albumFolderPath);

    if (!albumFolderAlreadyExists) {
        std::filesystem::create_directory(albumFolderPath);

        // 仅首次创建时提取封面
        const std::filesystem::path folderCoverPath = albumFolderPath / "Cover.jpg";
        if (extractCoverFromM4a(m4aPath, folderCoverPath)) {
            applyMacFolderIcon(albumFolderPath, folderCoverPath);
        }
        // std::filesystem::remove(folderCoverPath);
    }

    // ===== 移动 m4a =====
    const std::filesystem::path m4aDestinationPath =
        makeUniqueDestination(albumFolderPath / m4aPath.filename());
    std::filesystem::rename(m4aPath, m4aDestinationPath);

    // ===== 处理 flac（如果存在）=====
    if (std::filesystem::exists(flacPath) && std::filesystem::is_regular_file(flacPath)) {

        const std::filesystem::path flacFolderPath = albumFolderPath / "flac";

        if (!std::filesystem::exists(flacFolderPath)) {
            std::filesystem::create_directory(flacFolderPath);
        }

        const std::filesystem::path flacDestinationPath =
            makeUniqueDestination(flacFolderPath / flacPath.filename());

        std::filesystem::rename(flacPath, flacDestinationPath);
    }

}
