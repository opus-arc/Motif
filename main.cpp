#define DR_WAV_IMPLEMENTATION

#include "CommonsInit.h"
#include "Logger.h"
#include "MyDrWav.h"
#include "MyEigen.h"
#include "MyFfmpeg.h"
#include "MyFFT.h"

int main() {
    std::cout << "Program starts.\n" << std::endl;

    // ------------------------------------------------------------
    // 0. 初始化日志等公共组件
    // ------------------------------------------------------------
    CommonsInit::TestAllCommons();

    // ------------------------------------------------------------
    // 1. 读取音频文件，拿到原始浮点采样
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 1: 读取 wav，解析为浮点数离散信号");
    const M4a wav = MyDrWav::getM4a_float("test");
    testLog("Step 1: 读取完成");

    // 后面几步会反复用到这两个参数，先集中定义
    constexpr size_t fftSize = 2048;
    constexpr size_t hopSize = 512;
    constexpr size_t firstFrameStart = 0;
    constexpr size_t downsampleFactor = 43;

    // ------------------------------------------------------------
    // 2. 准备一帧 FFT 输入：
    //    多声道 -> 单声道 -> 取一帧 -> 乘 Hann window
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 2: 准备第一帧 FFT 输入");
    const std::vector<float> frame = MyFFT::prepareMonoFrameForFFT(wav, fftSize, firstFrameStart);
    std::cout << "frame.size(): " << frame.size() << std::endl;
    testLog("Step 2: 第一帧准备完成");

    if (frame.empty()) {
        testLog("Step 2 failed: frame 为空，程序结束");
        return 1;
    }

    // ------------------------------------------------------------
    // 3. 对这一帧做实数 FFT，得到幅度频谱
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 3: 计算第一帧的幅度频谱");
    const std::vector<float> magnitude = MyFFT::computeMagnitudeSpectrum(frame);
    std::cout << "magnitude.size(): " << magnitude.size() << std::endl;
    for (size_t i = 0; i < 10 && i < magnitude.size(); ++i) {
        std::cout << "magnitude[" << i << "]: " << magnitude[i] << std::endl;
    }
    testLog("Step 3: 幅度频谱计算完成");

    if (magnitude.empty()) {
        testLog("Step 3 failed: magnitude 为空，程序结束");
        return 1;
    }

    // ------------------------------------------------------------
    // 4. 把这一帧的频谱映射成 12 维 chroma 向量
    //    这 12 个槽位对应 12 个 pitch class（C 到 B）
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 4: 计算第一帧的 12 维 chroma");
    const std::vector<float> chroma = MyFFT::computeChromaFromMagnitudeSpectrum(
        magnitude,
        wav.sampleRate,
        fftSize
    );
    std::cout << "chroma.size(): " << chroma.size() << std::endl;
    for (size_t i = 0; i < chroma.size(); ++i) {
        std::cout << "chroma[" << i << "]: " << chroma[i] << std::endl;
    }
    testLog("Step 4: chroma 计算完成");

    if (chroma.empty()) {
        testLog("Step 4 failed: chroma 为空，程序结束");
        return 1;
    }

    // ------------------------------------------------------------
    // 5. 对整首音频逐帧重复以上流程，得到 chroma 序列
    //    输出形状可以理解为：numFrames x 12
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 5: 计算整首音频的 chroma sequence");
    const std::vector<std::vector<float> > chromaSequence = MyFFT::computeChromaSequence(
        wav,
        fftSize,
        hopSize
    );
    std::cout << "numFrames: " << chromaSequence.size() << std::endl;

    if (!chromaSequence.empty()) {
        std::cout << "chromaDim: " << chromaSequence[0].size() << std::endl;
        std::cout << "first frame chroma:" << std::endl;
        for (size_t i = 0; i < chromaSequence[0].size(); ++i) {
            std::cout << "  chromaSequence[0][" << i << "]: " << chromaSequence[0][i] << std::endl;
        }
    }
    testLog("Step 5: chroma sequence 计算完成");

    // ------------------------------------------------------------
    // 当前结果是否正常：
    // - frame.size() 应该等于 fftSize（这里是 2048）
    // - magnitude.size() 应该等于 fftSize / 2 + 1（这里是 1025）
    // - chroma.size() 应该等于 12
    // - chromaSequence.size() 应该是一个较大的帧数
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << "Summary:" << std::endl;
    std::cout << "  frame.size() = " << frame.size() << "  (expected: " << fftSize << ")" << std::endl;
    std::cout << "  magnitude.size() = " << magnitude.size() << "  (expected: " << (fftSize / 2 + 1) << ")" <<
            std::endl;
    std::cout << "  chroma.size() = " << chroma.size() << "  (expected: 12)" << std::endl;
    std::cout << "  chromaSequence.size() = " << chromaSequence.size() << std::endl;


    // ------------------------------------------------------------
    // 6. 先对 chromaMatrix 做时间平滑，再做时间下采样
    //    目标：把当前很高的帧率，压到更接近论文里适合做结构分析的尺度
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 6: 对 chromaMatrix 做时间平滑与下采样");

    const Eigen::MatrixXf chromaMatrix =
        MyEigen::convertChromaSequenceToMatrix(chromaSequence);

    const Eigen::MatrixXf smoothedChromaMatrix =
        MyEigen::smoothFeatureMatrix(chromaMatrix, 5);

    const Eigen::MatrixXf downsampledChromaMatrix =
        MyEigen::downsampleFeatureMatrix(smoothedChromaMatrix, downsampleFactor);

    std::cout << "smoothedChromaMatrix.rows(): " << smoothedChromaMatrix.rows() << std::endl;
    std::cout << "smoothedChromaMatrix.cols(): " << smoothedChromaMatrix.cols() << std::endl;
    std::cout << "downsampledChromaMatrix.rows(): " << downsampledChromaMatrix.rows() << std::endl;
    std::cout << "downsampledChromaMatrix.cols(): " << downsampledChromaMatrix.cols() << std::endl;

    testLog("Step 6: 时间平滑与下采样完成");

    // ------------------------------------------------------------
    // 7. 对下采样后的特征矩阵计算一个小规模 SSM 进行验证
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 7: 计算下采样后的 SSM");

    const Eigen::MatrixXf ssm =
        MyEigen::computeSelfSimilarityMatrix(downsampledChromaMatrix, 772);

    std::cout << "ssm.rows(): " << ssm.rows() << std::endl;
    std::cout << "ssm.cols(): " << ssm.cols() << std::endl;
    std::cout << "ssm(0,0): " << ssm(0,0) << std::endl;
    std::cout << "ssm(0,1): " << ssm(0,1) << std::endl;
    std::cout << "ssm(1,0): " << ssm(1,0) << std::endl;
    std::cout << "ssm(1,1): " << ssm(1,1) << std::endl;
    std::cout << "ssm(0,50): " << ssm(0,50) << std::endl;
    std::cout << "ssm(0,100): " << ssm(0,100) << std::endl;

    testLog("Step 7: SSM 计算完成");

    // ------------------------------------------------------------
    // 8. 对 SSM 做 threshold + penalty
    //    小于 threshold 的位置直接压成 penalty
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 8: 对 SSM 做 threshold + penalty");

    const Eigen::MatrixXf processedSSM =
        MyEigen::thresholdAndPenaltyMatrix(ssm, 0.90f, -2.0f);

    std::cout << "processedSSM.rows(): " << processedSSM.rows() << std::endl;
    std::cout << "processedSSM.cols(): " << processedSSM.cols() << std::endl;
    std::cout << "processedSSM(0,0): " << processedSSM(0,0) << std::endl;
    std::cout << "processedSSM(0,1): " << processedSSM(0,1) << std::endl;
    std::cout << "processedSSM(1,0): " << processedSSM(1,0) << std::endl;
    std::cout << "processedSSM(1,1): " << processedSSM(1,1) << std::endl;
    std::cout << "processedSSM(0,50): " << processedSSM(0,50) << std::endl;
    std::cout << "processedSSM(0,100): " << processedSSM(0,100) << std::endl;
    std::cout << "processedSSM(0,200): " << processedSSM(0,200) << std::endl;

    testLog("Step 8: threshold + penalty 完成");
    size_t penaltyCount = 0;
    size_t positiveCount = 0;
    size_t diagonalCount = 0;

    for (int i = 0; i < processedSSM.rows(); ++i) {
        for (int j = 0; j < processedSSM.cols(); ++j) {
            if (i == j) {
                ++diagonalCount;
                continue;
            }

            if (processedSSM(i, j) == -2.0f) {
                ++penaltyCount;
            } else {
                ++positiveCount;
            }
        }
    }

    std::cout << "penaltyCount: " << penaltyCount << std::endl;
    std::cout << "positiveCount: " << positiveCount << std::endl;
    std::cout << "diagonalCount: " << diagonalCount << std::endl;

    const size_t totalEntries =
        static_cast<size_t>(processedSSM.rows()) * static_cast<size_t>(processedSSM.cols());

    std::cout << "totalEntries: " << totalEntries << std::endl;
    std::cout << "penaltyRatio: "
              << static_cast<float>(penaltyCount) / static_cast<float>(totalEntries)
              << std::endl;

    std::cout << "Saving matrices to CSV..." << std::endl;
    MyEigen::saveMatrixToCSV(ssm, "ssm_raw.csv");
    MyEigen::saveMatrixToCSV(processedSSM, "ssm_processed.csv");
    std::cout << "CSV saved." << std::endl;

    // ------------------------------------------------------------
    // 后面所有 segment 都是在“下采样后的时间轴”上定义的。
    // 这里先准备一个统一的换算工具，把 frame index 转成秒。
    // 注意：这是近似时间，已经足够拿去试听定位。
    // ------------------------------------------------------------
    const float secondsPerProcessedFrame =
        static_cast<float>(hopSize * downsampleFactor) /
        static_cast<float>(wav.sampleRate);

    const auto frameIndexToSeconds = [secondsPerProcessedFrame](const int frameIndex) -> float {
        return static_cast<float>(frameIndex) * secondsPerProcessedFrame;
    };

    const auto printSegmentTimeInfo = [&frameIndexToSeconds](const char* label, const Segment& segment) {
        const float startSec = frameIndexToSeconds(segment.start);
        const float endSec = frameIndexToSeconds(segment.end);
        const float durationSec = endSec - startSec;

        std::cout << label << " frame range: ["
                  << segment.start << ", " << segment.end << "]" << std::endl;
        std::cout << label << " time range (sec): ["
                  << startSec << ", " << endSec << "]" << std::endl;
        std::cout << label << " duration (sec): " << durationSec << std::endl;
    };

    // ------------------------------------------------------------
    // 9. 用一个手动挑选的测试 segment，验证 evaluateSegment
    //    这里不再把所有中间矩阵都打印出来，main 保持精简。
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 9: 用 evaluateSegment 验证一个手动测试 segment");

    const Segment testSegment{40, 70};
    const FitnessResult evalResult =
        MyEigen::evaluateSegment(processedSSM, testSegment);

    std::cout << "evalResult.segment: ["
              << evalResult.segment.start << ", "
              << evalResult.segment.end << "]" << std::endl;
    std::cout << "evalResult.score: " << evalResult.score << std::endl;
    std::cout << "evalResult.normalizedScore: " << evalResult.normalizedScore << std::endl;
    std::cout << "evalResult.coverage: " << evalResult.coverage << std::endl;
    std::cout << "evalResult.normalizedCoverage: " << evalResult.normalizedCoverage << std::endl;
    std::cout << "evalResult.pathFamilyLength: " << evalResult.pathFamilyLength << std::endl;
    std::cout << "evalResult.fitness: " << evalResult.fitness << std::endl;
    printSegmentTimeInfo("testSegment", evalResult.segment);

    testLog("Step 9: evaluateSegment 验证完成");

    // ------------------------------------------------------------
    // 12. 小范围扫描 candidate segments，找当前 fitness 最大的片段
    //    先不要全局暴力搜索，只在一个可控范围内验证流程
    // ------------------------------------------------------------
    std::cout << "---------------------------------------------" << std::endl;
    testLog("Step 12: 小范围扫描 candidate segments");

    const int scanStartMin = 0;
    const int scanStartMax = processedSSM.rows() - 1;
    const int scanLengthMin = 20;
    const int scanLengthMax = 40;

    bool hasBest = false;
    FitnessResult bestResult{};
    int scannedCount = 0;
    int skippedCount = 0;

    for (int start = scanStartMin; start <= scanStartMax; ++start) {
        for (int length = scanLengthMin; length <= scanLengthMax; ++length) {
            const int end = start + length - 1;

            if (end >= processedSSM.rows()) {
                ++skippedCount;
                continue;
            }

            const Segment candidate{start, end};
            const FitnessResult result = MyEigen::evaluateSegment(processedSSM, candidate);
            ++scannedCount;

            if (!hasBest || result.fitness > bestResult.fitness) {
                bestResult = result;
                hasBest = true;
            }
        }
    }

    std::cout << "scanStart range: [" << scanStartMin << ", " << scanStartMax << "]" << std::endl;
    std::cout << "scanLength range: [" << scanLengthMin << ", " << scanLengthMax << "]" << std::endl;
    std::cout << "scannedCount: " << scannedCount << std::endl;
    std::cout << "skippedCount: " << skippedCount << std::endl;
    std::cout << "processedSSM total duration (sec): "
              << frameIndexToSeconds(processedSSM.rows() - 1) << std::endl;
    std::cout << "bestResult frame count: "
              << (bestResult.segment.end - bestResult.segment.start + 1) << std::endl;

    if (hasBest) {
        std::cout << "bestResult.segment: ["
                  << bestResult.segment.start << ", "
                  << bestResult.segment.end << "]" << std::endl;
        std::cout << "bestResult.score: " << bestResult.score << std::endl;
        std::cout << "bestResult.normalizedScore: " << bestResult.normalizedScore << std::endl;
        std::cout << "bestResult.coverage: " << bestResult.coverage << std::endl;
        std::cout << "bestResult.normalizedCoverage: " << bestResult.normalizedCoverage << std::endl;
        std::cout << "bestResult.pathFamilyLength: " << bestResult.pathFamilyLength << std::endl;
        std::cout << "bestResult.fitness: " << bestResult.fitness << std::endl;
        std::cout << "bestResult average repetition duration (sec): ";

        const AccumulatedScoreResult bestAccResult =
            MyEigen::computeAccumulatedScoreMatrixForSegment(processedSSM, bestResult.segment);

        const PathFamily bestPathFamily =
            MyEigen::computeOptimalPathFamily(bestAccResult.D);

        const std::vector<Segment> bestSegmentFamily =
            MyEigen::computeInducedSegmentFamily(bestPathFamily);

        printSegmentTimeInfo("bestResult", bestResult.segment);

        float repetitionDurationSum = 0.0f;
        std::cout << "bestSegmentFamily.size(): " << bestSegmentFamily.size() << std::endl;
        for (size_t i = 0; i < bestSegmentFamily.size(); ++i) {
            const Segment& seg = bestSegmentFamily[i];
            const float startSec = frameIndexToSeconds(seg.start);
            const float endSec = frameIndexToSeconds(seg.end);
            const float durationSec = endSec - startSec;
            repetitionDurationSum += durationSec;

            std::cout << "best repetition " << i
                      << " frame range: [" << seg.start << ", " << seg.end << "]" << std::endl;
            std::cout << "best repetition " << i
                      << " time range (sec): [" << startSec << ", " << endSec << "]" << std::endl;
            std::cout << "best repetition " << i
                      << " duration (sec): " << durationSec << std::endl;
        }
        if (!bestSegmentFamily.empty()) {
            std::cout << (repetitionDurationSum / static_cast<float>(bestSegmentFamily.size())) << std::endl;
        } else {
            std::cout << 0.0f << std::endl;
        }

        const float bestStartSec = frameIndexToSeconds(bestResult.segment.start);
        const float bestEndSec = frameIndexToSeconds(bestResult.segment.end);
        const float processedTimelineEndSec = frameIndexToSeconds(processedSSM.rows() - 1);
        std::cout << "bestResult relative position in processed timeline: "
                  << (bestStartSec / processedTimelineEndSec) << " -> "
                  << (bestEndSec / processedTimelineEndSec) << std::endl;
    } else {
        std::cout << "No valid candidate segment found in scan range." << std::endl;
    }

    testLog("Step 12: candidate scan 完成");

    return 0;
}
