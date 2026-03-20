//
// Created by opus arc on 2026/3/12.
//

#ifndef MUSICCAT_PUBLIC_H
#define MUSICCAT_PUBLIC_H
#include <iomanip>
#include <regex>
#include <string>
#include <iostream>
#include <sstream>

#include <eigen/Dense>

#include "dr_wav.h"


inline std::string sanitizeFileName(const std::string &title, const std::size_t maxLen = 180) {
    std::string s = title;

    // 1) 把最危险、最常见的文件名问题字符替换掉
    // mac/Linux 不能含 '/'
    // Windows 也不喜欢 \ : * ? " < > |
    static const std::regex badChars(R"([\/\\:\*\?"<>\|])");
    s = std::regex_replace(s, badChars, " - ");

    // 2) 去掉控制字符
    static const std::regex ctrlChars(R"([\x00-\x1F\x7F])");
    s = std::regex_replace(s, ctrlChars, "");

    // 3) 把连续空白压成一个空格
    static const std::regex multiSpace(R"(\s+)");
    s = std::regex_replace(s, multiSpace, " ");

    // 4) 去掉首尾空格
    auto trim = [](std::string &str) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), notSpace));
        str.erase(std::find_if(str.rbegin(), str.rend(), notSpace).base(), str.end());
    };
    trim(s);

    // 5) 避免文件名结尾是 '.' 或空格
    while (!s.empty() && (s.back() == '.' || s.back() == ' ')) {
        s.pop_back();
    }

    // 6) 空字符串兜底
    if (s.empty()) {
        s = "Untitled";
    }

    // 7) 控制长度，给扩展名留空间
    if (s.size() > maxLen) {
        s = s.substr(0, maxLen);
        while (!s.empty() && (s.back() == ' ' || s.back() == '.')) {
            s.pop_back();
        }
        if (s.empty()) {
            s = "Untitled";
        }
    }

    return s;
}

namespace TermFix {
    static inline std::string trim(const std::string &s) {
        const auto begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return "";
        const auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(begin, end - begin + 1);
    }

    static inline bool startsWith(const std::string &s, const std::string &prefix) {
        return s.rfind(prefix, 0) == 0;
    }

    struct SoxField {
        std::string key;
        std::string value;
    };

    static std::vector<SoxField> parseSoxHeader(const std::string &headerText) {
        std::vector<SoxField> fields;
        std::istringstream iss(headerText);
        std::string line;
        while (std::getline(iss, line)) {
            line = trim(line);
            if (line.empty()) continue;

            auto pos = line.find(':');
            if (pos == std::string::npos) continue;

            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));

            // 去掉 Input File
            if (key == "Input File") continue;

            fields.push_back({key, value});
        }

        return fields;
    }

    static void eraseCurrentLine() {
        std::cout << "\r\033[2K";
    }

    static void moveCursorUp(int n) {
        if (n > 0) {
            std::cout << "\033[" << n << "A";
        }
    }

    static void rewriteSoxHeaderAligned(const std::string &headerText) {
        auto fields = parseSoxHeader(headerText);
        if (fields.empty()) return;

        // 计算 key 最大宽度，让冒号对齐
        std::size_t maxKeyLen = 0;
        for (const auto &f: fields) {
            maxKeyLen = std::max(maxKeyLen, f.key.size());
        }

        // sox 原先通常打印 5 行头部 + 1 个空行，然后下面是实时状态行
        // 我们要删掉上面的 6 行，再按新格式重写 4 行 + 1 空行
        moveCursorUp(6);

        for (int i = 0; i < 6; ++i) {
            eraseCurrentLine();
            if (i != 5) std::cout << '\n';
        }

        moveCursorUp(5);

        for (const auto &f: fields) {
            std::cout << std::setw(static_cast<int>(maxKeyLen))
                    << std::right
                    << f.key
                    << ": "
                    << f.value
                    << '\n';
        }

        std::cout << '\n';
        std::cout.flush();
    }
} // namespace TermFix

struct M4a {
    unsigned int sampleRate;
    unsigned int channels;
    std::vector<float> data;
    drwav_uint64 totalFrameCount;
};

struct Segment {
    int start;
    int end;
};

struct AccumulatedScoreResult {
    Eigen::MatrixXf D;
    float score;
};

struct PathPoint {
    int row;
    int col;
};

using Path = std::vector<PathPoint>;
using PathFamily = std::vector<Path>;

struct FitnessResult {
    Segment segment;
    float score;
    float normalizedScore;
    int coverage;
    float normalizedCoverage;
    float fitness;
    int pathFamilyLength;
};

struct FastFitnessResult {
    Segment segment;
    float score;
    float estimatedFitness;
};

#endif //MUSICCAT_PUBLIC_H
