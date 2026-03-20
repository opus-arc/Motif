//
// Created by opus arc on 2026/3/7.
//

#ifndef MUSICCAT_MYFFMPEG_H
#define MUSICCAT_MYFFMPEG_H
#include <string>
#include <vector>

#include "Public.h"


class MyFfmpeg {
public:

    static void autoConvertedToWavByFileName(const std::string &title);

    static void autoConvertedToM4aByFileName(const std::string &title, const std::string& newTitle);

    static void cutTheAudio(const std::string &title, double startTime, double endTime);

    static void applyFade(const std::string& title, double fadeInSeconds, double fadeOutSeconds);


    // static M4a decodeM4aToFloatByFilename(const std::string &title);
};


#endif //MUSICCAT_MYFFMPEG_H
