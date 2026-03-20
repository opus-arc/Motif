cmake_minimum_required(VERSION 4.0)
project(Motif)

set(CMAKE_CXX_STANDARD 20)

add_executable(Motif main.cpp
        include/Motif.cpp
        include/Motif.h
        include/common/Logger.h
        include/common/Cmd.h
        include/common/FileManager.h
        include/common/CommonsInit.h
        include/common/MyPath.h
        include/common/Public.h
        include/common/TypeParser.h
        include/common/src/Cmd.cpp
        include/common/src/MyPath.cpp
        include/common/src/Logger.cpp
        include/common/src/TypeParser.cpp
        include/common/src/FileManager.cpp
        include/common/src/CommonsInit.cpp
        external/dr_wav/dr_wav.h
        include/core/src/MyFfmpeg.cpp
        include/core/MyFfmpeg.h
        external/FFT/_kiss_fft_guts.h
        external/FFT/kiss_fftnd.h
        external/FFT/kfc.c
        external/FFT/kiss_fft.h
        external/FFT/kiss_fftndr.h
        external/FFT/kiss_fft_log.h
        external/FFT/kiss_fftnd.c
        external/FFT/kfc.h
        external/FFT/kissfft.hh
        external/FFT/kiss_fftndr.c
        external/FFT/kissfft_i32.hh
        external/FFT/kiss_fftr.h
        external/FFT/kiss_fft.c
        external/FFT/kiss_fftr.c
        include/core/src/MyDrWav.cpp
        include/core/MyDrWav.h
        include/core/src/MyFFT.cpp
        include/core/MyFFT.h
        include/core/src/MyEigen.cpp
        include/core/MyEigen.h
)

include(FetchContent)

# 使用 spdlog 作为 logger
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.3
)

FetchContent_MakeAvailable(spdlog)

target_include_directories(
        Motif PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/
        ${CMAKE_CURRENT_SOURCE_DIR}/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/include/common/
        ${CMAKE_CURRENT_SOURCE_DIR}/include/core/
        ${CMAKE_CURRENT_SOURCE_DIR}/external/
        ${CMAKE_CURRENT_SOURCE_DIR}/external/dr_wav
        ${CMAKE_CURRENT_SOURCE_DIR}/external/eigen
        ${CMAKE_CURRENT_SOURCE_DIR}/external/fft
)

target_link_libraries(Motif PRIVATE spdlog::spdlog)


set_target_properties(Motif PROPERTIES
        OUTPUT_NAME "motif"
)