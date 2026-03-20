//
// Created by opus arc on 2026/3/6.
//

#include "../FileManager.h"

#include "MyPath.h"


std::string FileManager::copyToWorkspace(const std::filesystem::path& targetPath) {
    const auto workspace = MyPath::getWorkspaceFolderPath();
    std::filesystem::create_directories(workspace);

    const auto destination = workspace / targetPath.filename();
    std::filesystem::copy_file(
            targetPath,
            destination,
            std::filesystem::copy_options::overwrite_existing
    );

    // 这个title不含后缀名
    return targetPath.stem().string();
}

void FileManager::clearWorkspace() {
    std::filesystem::remove_all(MyPath::getWorkspaceFolderPath());
}

void FileManager::copyFileToFolder(const std::string& fileName, const std::filesystem::path &opf) {
    // 把名为 fileName（包含后缀名）的文件复制到 opf，也就是目标文件夹中
    const auto workspace = MyPath::getWorkspaceFolderPath();

    if (std::filesystem::exists(opf) && !std::filesystem::is_directory(opf)) {
        throw std::runtime_error("Target path is not a directory: " + opf.string());
    }

    std::filesystem::create_directories(opf);

    if (!std::filesystem::exists(workspace) || !std::filesystem::is_directory(workspace)) {
        return;
    }

    const std::filesystem::path sourceName(fileName);
    const auto expectedFilename = sourceName.filename();

    for (const auto& entry : std::filesystem::directory_iterator(workspace)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto& path = entry.path();
        if (path.filename() != expectedFilename) {
            continue;
        }

        const auto target = opf / path.filename();
        std::filesystem::copy_file(
                path,
                target,
                std::filesystem::copy_options::overwrite_existing
        );

        return;
    }

    throw std::runtime_error("Source file not found in workspace: " + expectedFilename.string());
}

