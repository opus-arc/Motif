/**
 *  MusicCat CLI Entry
 */

#include <iostream>
#include <string>
#include <string_view>
#include <Motif.h>
#include <CommonsInit.h>
#include <csignal>

#include "FileManager.h"
#include "Logger.h"
#include "MyFfmpeg.h"
#include "MyPath.h"
#define MOTIF_VERSION "0.1.0"

std::atomic<bool> g_shouldExit = false;

void signalHandler(int) {
    g_shouldExit.store(true, std::memory_order_relaxed);
}

void help();
void runMotifCommand(const std::string& audioPath, const std::string& opf);

namespace {
    constexpr std::string_view kAppName = "motif";


    void printUnknownCommand(const std::string_view command) {
        std::cerr << "Unknown command: " << command << "\n\n";
        help();
    }

    void versionCommand() {
        CommonsInit::TestAllCommons();
        const std::string version = MOTIF_VERSION;
        std::cout << kAppName << " " << version << std::endl;
    }

    void logCommand() {
        CommonsInit::TestAllCommons();
        Logger::printLog();
    }

} // namespace


void help() {
    std::cout << "Motif - Audio thumbnailing CLI\n\n";

    std::cout << "Usage:\n";
    std::cout << "  motif <audio.xxx> <opf>\n";
    std::cout << "  motif <command>\n\n";

    std::cout << "Commands:\n";
    std::cout << "  log                  Print runtime logs\n";
    std::cout << "  help                 Show this help message\n";
    std::cout << "  zh                   Show Chinese help\n";
    std::cout << "  ja                   Show Japanese help\n\n";

    std::cout << "Notes:\n";
    std::cout << "  When two file paths are provided, motif will run audio thumbnailing.\n";
    std::cout << "  Example: motif input.wav output.wav\n";
    std::cout << "  -h, --help           Show help message\n";
    std::cout << "  -v, --version        Show program version\n";
}

void helpZh() {
    std::cout << "Motif - 音频缩略图命令行工具\n\n";

    std::cout << "用法:\n";
    std::cout << "  motif <audio.xxx> <opf>\n";
    std::cout << "  motif <命令>\n\n";

    std::cout << "命令:\n";
    std::cout << "  log                  查看运行日志\n";
    std::cout << "  help                 显示英文帮助\n";
    std::cout << "  zh                   显示中文帮助\n";
    std::cout << "  ja                   显示日文帮助\n\n";

    std::cout << "说明:\n";
    std::cout << "  当传入两个文件路径时，motif 会执行音频缩略图分析。\n";
    std::cout << "  示例: motif input.wav output.wav\n";
    std::cout << "  -h, --help           显示帮助信息\n";
    std::cout << "  -v, --version        显示版本号\n";
}

void helpJa() {
    std::cout << "Motif - オーディオ縮約サムネイル CLI\n\n";

    std::cout << "使い方:\n";
    std::cout << "  motif <audio.xxx> <opf>\n";
    std::cout << "  motif <command>\n\n";

    std::cout << "コマンド:\n";
    std::cout << "  log                  実行ログを表示\n";
    std::cout << "  help                 英語ヘルプを表示\n";
    std::cout << "  zh                   中国語ヘルプを表示\n";
    std::cout << "  ja                   日本語ヘルプを表示\n\n";

    std::cout << "説明:\n";
    std::cout << "  2つのファイルパスを渡すと、motif は音声サムネイル解析を実行します。\n";
    std::cout << "  例: motif input.wav output.wav\n";
    std::cout << "  -h, --help           ヘルプを表示\n";
    std::cout << "  -v, --version        バージョンを表示\n";
}

void runMotifCommand(const std::string& audioPath, const std::string& opf) {
    CommonsInit::TestAllCommons();

    std::cout << "audioPath:  " << audioPath << std::endl;
    std::cout << "opf: " << opf << std::endl;

    // 为了避免在 mcat 遇到的那种线程问题 还是先清理再说吧
    // 不过 cpp 的似乎都是自动检查清楚的，只要返回了就是已经确认执行完了
    FileManager::clearWorkspace();

    // 把目标文件拷到工作区，顺便获取 title
    const std::string title = FileManager::copyToWorkspace(audioPath);

    // 执行核心逻辑
    Motif::audioThumbnailing(title);

    // 拼凑完整的修改好的文件名字，下列涉及到几种不同阶段的处理阶段，都可以选择性地迁移到用户要求的位置
    const std::string prepped = title + "_cut.m4a"; // 纯粹的音乐缩略图与动机
    const std::string prepared = title + "_cut_fade.m4a"; // 加上淡入淡出的音乐缩略图与动机
    const std::string motif = title + "_motif.m4a";

    // 改个名
    std::filesystem::copy_file(
        MyPath::getWorkspaceFolderPath() / prepared,
        MyPath::getWorkspaceFolderPath()/ motif
        );


    // 把制作好的文件移动到用户要求的目标文件夹下方
    // 根据需要选择需要的菜品
    FileManager::copyFileToFolder(motif, opf);
}

int main(const int argc, char *argv[]) {

    std::signal(SIGINT, signalHandler); // Ctrl + C
    std::signal(SIGTERM, signalHandler); // kill
    // std::signal(SIGHUP, signalHandler);

    if (argc < 2) {
        help();
        return 0;
    }

    const std::string cmd = argv[1];

    if (cmd == "help"
        || cmd == "-h"
        || cmd == "--help"
        || cmd == "--zh"
        || cmd == "zh"
        || cmd == "ja"
        || cmd == "--ja"
    ) {
        if (cmd == "zh" || cmd == "--zh")
            helpZh();
        else if (cmd == "ja" || cmd == "--ja" || cmd == "japan")
            helpJa();
        else
            help();
        return 0;
    }

    if (cmd == "-ver" || cmd == "-version" || cmd == "--version" || cmd == "-v") {
        versionCommand();
        return 0;
    }

    if (cmd == "log") {
        logCommand();
        return 0;
    }

    if (argc == 3) {
        const std::string audioPath = argv[1];
        const std::string opf = argv[2];
        runMotifCommand(audioPath, opf);
        return 0;
    }

    printUnknownCommand(cmd);

    return 1;
}
