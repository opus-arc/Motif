//
// Created by opus arc on 2026/3/7.
//

#ifndef MUSICCAT_PATHMANAGER_H
#define MUSICCAT_PATHMANAGER_H

#include <filesystem>

class MyPath {
    static bool isTestingInClion;
public:
    static std::filesystem::path getProjectPath();

    static std::filesystem::path getHomePath();

    static std::filesystem::path getCachePath();

    static std::filesystem::path getCacheTxtPath();

    static std::filesystem::path getLogPath();

    static std::filesystem::path getLogFilePath();

    static std::filesystem::path getLogoPath();

    static std::filesystem::path getWorkspaceFolderPath();
};


#endif //MUSICCAT_PATHMANAGER_H
