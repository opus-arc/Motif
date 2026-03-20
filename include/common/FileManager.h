//
// Created by opus arc on 2026/3/6.
//

#ifndef MUSICCAT_PERSISTENTSTORAGE_H
#define MUSICCAT_PERSISTENTSTORAGE_H
#include <fstream>


class FileManager {
public:
    // 将一个文件从一个 path 复制到一个文件夹下方
    static std::string copyToWorkspace(const std::filesystem::path& targetPath);

    // 清空工作区
    static void clearWorkspace();

    // 将一个文件覆盖到一个路径上
    static void copyFileToFolder(const std::string& fileName, const std::filesystem::path& folderPath);
};


#endif //MUSICCAT_PERSISTENTSTORAGE_H