//
// Created by opus arc on 2026/3/7.
//

#include <Logger.h>
#include <MyPath.h>
#include <FileManager.h>

#include <filesystem>
#include <vector>
#include <fstream>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>

std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;

void Logger::init() {
    if (logger_) {
        return;
    }

    // Ensure log directory exists
    const auto logDir = MyPath::getLogPath();
    FileManager::ensure_path(logDir);

    auto logFile = (logDir / "motif.log").string();

    // Console output
    const auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    class ConsoleFormatterNoInfo : public spdlog::formatter {
    public:
        void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override {
            std::string payload(msg.payload.begin(), msg.payload.end());

            if (msg.level == spdlog::level::info) {
                fmt::format_to(std::back_inserter(dest), "{}\n", payload);
            } else {
                auto level_sv = spdlog::level::to_string_view(msg.level);
                std::string level(level_sv.data(), level_sv.size());
                fmt::format_to(std::back_inserter(dest), "[{}] {}\n", level, payload);
            }
        }

        std::unique_ptr<spdlog::formatter> clone() const override {
            return std::make_unique<ConsoleFormatterNoInfo>();
        }
    };

    console_sink->set_formatter(std::make_unique<ConsoleFormatterNoInfo>());

    // File output with rotation
    const auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFile,
        5 * 1024 * 1024,   // 5MB per file
        3                  // keep 3 old log files
    );

    std::vector<spdlog::sink_ptr> sinks { console_sink, file_sink };

    logger_ = std::make_shared<spdlog::logger>("motif", sinks.begin(), sinks.end());

    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
    logger_->set_level(spdlog::level::debug);

    spdlog::set_default_logger(logger_);
}

std::shared_ptr<spdlog::logger> Logger::get() {
    return logger_;
}

void Logger::debug(const std::string_view message) {
    if (logger_) {
        logger_->debug(message);
    }
}

void Logger::info(const std::string_view message) {
    if (logger_) {
        logger_->info(message);
    }
}

void Logger::warn(const std::string_view message) {
    if (logger_) {
        logger_->warn(message);
    }
}

void Logger::error(const std::string_view message) {
    if (logger_) {
        logger_->error(message);
    }
}

void Logger::fatal(const std::string_view message) {
    if (logger_) {
        logger_->critical(message);
    }
}


void Logger::printLog() {
    const auto logDir = MyPath::getLogPath();
    const auto logFile = logDir / "motif.log";

    std::ifstream file(logFile);
    if (!file.is_open()) {
        warn("Failed to open log file");
        return;
    }

    std::vector<std::string> lines;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    if (lines.empty()) {
        info("Log file is empty");
        return;
    }

    constexpr std::size_t maxLines = 20;
    const std::size_t start = lines.size() > maxLines ? lines.size() - maxLines : 0;

    for (std::size_t i = start; i < lines.size(); ++i) {
        std::cout << lines[i] << '\n';
    }
}
