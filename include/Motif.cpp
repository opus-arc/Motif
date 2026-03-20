//
// Created by opus arc on 2026/3/19.
//

#define DR_WAV_IMPLEMENTATION

#include "Motif.h"
#include "CommonsInit.h"
#include "Logger.h"
#include "MyDrWav.h"
#include "MyEigen.h"
#include "MyFfmpeg.h"
#include "MyFFT.h"
#include <algorithm>
#include <unordered_set>
#include <chrono>

void Motif::audioThumbnailing(const std::string& title) {
    try {
        std::cout << "Program starts.\n" << std::endl;

        const auto programStartTime = std::chrono::steady_clock::now();
        auto stepTime = programStartTime;

        const auto printElapsedMs = [&](const char *label) {
            const auto now = std::chrono::steady_clock::now();
            const auto stepMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - stepTime).count();
            const auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - programStartTime).count();

            std::cout << "[timer] " << label
                    << " | step: " << stepMs << " ms"
                    << " | total: " << totalMs << " ms"
                    << std::endl;

            stepTime = now;
        };

        // ------------------------------------------------------------
        // 0. 初始化日志等公共组件
        // ------------------------------------------------------------
        CommonsInit::TestAllCommons();
        printElapsedMs("Step 0 完成");

        // ------------------------------------------------------------
        // 1. 读取音频文件，拿到原始浮点采样
        // ------------------------------------------------------------
        std::cout << "---------------------------------------------" << std::endl;
        testLog("Step 1: 读取 wav，解析为浮点数离散信号");

        // std::string title = "Lazarus";

        MyFfmpeg::autoConvertedToWavByFileName(title); // 自动格式转换
        const M4a wav = MyDrWav::getM4a_float(title);
        testLog("Step 1: 读取完成");
        printElapsedMs("Step 1 完成");

        // 后面几步会反复用到这两个参数，先集中定义
        constexpr size_t fftSize = 2048;
        constexpr size_t hopSize = 512;
        constexpr size_t downsampleFactor = 43;

        // ------------------------------------------------------------
        // 2. 对整首音频逐帧提取 12 维 chroma，作为后续结构分析的基础特征
        //    输出可以理解为：numFrames x 12
        // ------------------------------------------------------------
        std::cout << "---------------------------------------------" << std::endl;
        testLog("Step 2: 计算整首音频的 chroma sequence");
        const std::vector<std::vector<float> > chromaSequence = MyFFT::computeChromaSequence(
            wav,
            fftSize,
            hopSize
        );

        if (chromaSequence.empty()) {
            throw std::runtime_error("Step 2 failed: chromaSequence 为空，程序结束");
        }

        std::cout << "numFrames: " << chromaSequence.size() << std::endl;
        std::cout << "chromaDim: " << chromaSequence[0].size() << std::endl;
        testLog("Step 2: chroma sequence 计算完成");
        printElapsedMs("Step 2 完成");

        // ------------------------------------------------------------
        // 3. 对 chroma 序列做时间平滑与下采样
        //    目标：降低时间分辨率，减少后续 SSM 与扫描阶段的计算量
        // ------------------------------------------------------------
        std::cout << "---------------------------------------------" << std::endl;
        testLog("Step 3: 对 chromaMatrix 做时间平滑与下采样");

        const Eigen::MatrixXf chromaMatrix =
                MyEigen::convertChromaSequenceToMatrix(chromaSequence);

        const Eigen::MatrixXf smoothedChromaMatrix =
                MyEigen::smoothFeatureMatrix(chromaMatrix, 5);

        const Eigen::MatrixXf downsampledChromaMatrix =
                MyEigen::downsampleFeatureMatrix(smoothedChromaMatrix, downsampleFactor);

        std::cout << "downsampledFrames: " << downsampledChromaMatrix.rows() << std::endl;
        std::cout << "featureDim: " << downsampledChromaMatrix.cols() << std::endl;
        testLog("Step 3: 时间平滑与下采样完成");
        printElapsedMs("Step 3 完成");

        // ------------------------------------------------------------
        // 4. 基于下采样后的 chroma 计算自相似矩阵（SSM）
        //    这一步会把“时间-特征序列”变成“时间-时间相似关系”
        // ------------------------------------------------------------
        std::cout << "---------------------------------------------" << std::endl;
        testLog("Step 4: 计算下采样后的 SSM");

        const Eigen::MatrixXf ssm =
                MyEigen::computeSelfSimilarityMatrix(downsampledChromaMatrix, downsampledChromaMatrix.rows());

        std::cout << "ssmSize: " << ssm.rows() << " x " << ssm.cols() << std::endl;
        testLog("Step 4: SSM 计算完成");
        printElapsedMs("Step 4 完成");

        // ------------------------------------------------------------
        // 5. 对 SSM 做 threshold + penalty
        //    保留高相似区域，压低弱相似区域，增强重复结构
        // ------------------------------------------------------------
        std::cout << "---------------------------------------------" << std::endl;
        testLog("Step 5: 对 SSM 做 threshold + penalty");

        const Eigen::MatrixXf processedSSM =
                MyEigen::thresholdAndPenaltyMatrix(ssm, 0.90f, -2.0f);

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

        const size_t totalEntries =
                static_cast<size_t>(processedSSM.rows()) * static_cast<size_t>(processedSSM.cols());

        std::cout << "processedSSM size: " << processedSSM.rows() << " x " << processedSSM.cols() << std::endl;
        std::cout << "penaltyRatio: "
                << static_cast<float>(penaltyCount) / static_cast<float>(totalEntries)
                << std::endl;

        MyEigen::saveMatrixToCSV(ssm, "ssm_raw.csv");
        MyEigen::saveMatrixToCSV(processedSSM, "ssm_processed.csv");
        testLog("Step 5: threshold + penalty 完成");
        printElapsedMs("Step 5 完成");

        // ------------------------------------------------------------
        // 6. 准备时间换算工具
        //    后面所有 segment 都定义在“下采样后的时间轴”上，需要换算成秒
        // ------------------------------------------------------------
        const float secondsPerProcessedFrame =
                static_cast<float>(hopSize * downsampleFactor) /
                static_cast<float>(wav.sampleRate);

        const auto frameIndexToSeconds = [secondsPerProcessedFrame](const int frameIndex) -> float {
            return static_cast<float>(frameIndex) * secondsPerProcessedFrame;
        };

        const auto printSegmentTimeInfo = [&frameIndexToSeconds](const char *label, const Segment &segment) {
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
        // 7. 使用“粗到细”的多层采样策略扫描候选 segment
        //    论文思路：先在稀疏网格上找高分候选，再只在这些候选附近做细化。
        //    这样通常能显著减少 evaluateSegment 的调用次数。
        // ------------------------------------------------------------
        std::cout << "---------------------------------------------" << std::endl;
        testLog("Step 7: 使用多层采样扫描 candidate segments");

        const int scanLengthMin = 20; // 20
        const int scanLengthMax = 45; // 40
        const std::vector<int> gridSteps = {8, 4, 2, 1};
        constexpr int anchorCount = 64;
        constexpr int finalAnchorCount = 16;

        struct CandidateScore {
            Segment segment;
            FitnessResult result;
        };

        std::vector<CandidateScore> evaluatedCandidates;
        evaluatedCandidates.reserve(4096);

        auto segmentKey = [](const Segment &seg) -> long long {
            return (static_cast<long long>(seg.start) << 32) |
                   static_cast<unsigned int>(seg.end);
        };

        std::unordered_set<long long> visitedSegments;
        visitedSegments.reserve(8192);

        int skippedCount = 0;

        auto tryEvaluateCandidate = [&](const Segment &candidate) {
            if (candidate.start < 0 || candidate.end >= processedSSM.rows() || candidate.start > candidate.end) {
                ++skippedCount;
                return;
            }

            const long long key = segmentKey(candidate);
            if (visitedSegments.find(key) != visitedSegments.end()) {
                return;
            }
            visitedSegments.insert(key);

            const FitnessResult result = MyEigen::evaluateSegment(processedSSM, candidate);
            evaluatedCandidates.push_back(CandidateScore{candidate, result});
        };

        auto collectAnchors = [&](int maxCount) {
            std::vector<CandidateScore> sorted = evaluatedCandidates;
            std::sort(sorted.begin(), sorted.end(), [](const CandidateScore &a, const CandidateScore &b) {
                return a.result.fitness > b.result.fitness;
            });
            if (static_cast<int>(sorted.size()) > maxCount) {
                sorted.resize(maxCount);
            }
            return sorted;
        };

        // Level 1: 在最粗的二维网格上做初筛
        const int firstGridStep = gridSteps.front();
        for (int start = 0; start < processedSSM.rows(); start += firstGridStep) {
            for (int length = scanLengthMin; length <= scanLengthMax; length += firstGridStep) {
                const Segment candidate{start, start + length - 1};
                tryEvaluateCandidate(candidate);
            }
        }

        // 后续层：只在高分 anchor 的局部邻域内细化
        for (size_t levelIndex = 1; levelIndex < gridSteps.size(); ++levelIndex) {
            const int step = gridSteps[levelIndex];
            const std::vector<CandidateScore> anchors = collectAnchors(finalAnchorCount);

            for (const CandidateScore &anchor: anchors) {
                for (int deltaStart = -step; deltaStart <= step; deltaStart += step) {
                    for (int deltaLength = -step; deltaLength <= step; deltaLength += step) {
                        const int refinedStart = anchor.segment.start + deltaStart;
                        const int refinedLength = (anchor.segment.end - anchor.segment.start + 1) + deltaLength;

                        if (refinedLength < scanLengthMin || refinedLength > scanLengthMax) {
                            ++skippedCount;
                            continue;
                        }

                        const Segment refinedCandidate{refinedStart, refinedStart + refinedLength - 1};
                        tryEvaluateCandidate(refinedCandidate);
                    }
                }
            }
        }

        bool hasBest = false;
        FitnessResult bestResult{};
        for (const CandidateScore &candidate: evaluatedCandidates) {
            if (!hasBest || candidate.result.fitness > bestResult.fitness) {
                bestResult = candidate.result;
                hasBest = true;
            }
        }

        std::cout << "scanLength range: [" << scanLengthMin << ", " << scanLengthMax << "]" << std::endl;
        std::cout << "gridSteps: [";
        for (size_t i = 0; i < gridSteps.size(); ++i) {
            std::cout << gridSteps[i];
            if (i + 1 < gridSteps.size()) {
                std::cout << ", ";
            }
        }
        std::cout << "]" << std::endl;
        std::cout << "anchorCount: " << anchorCount << std::endl;
        std::cout << "evaluatedCandidates: " << evaluatedCandidates.size() << std::endl;
        std::cout << "skippedCount: " << skippedCount << std::endl;
        std::cout << "processedSSM total duration (sec): "
                << frameIndexToSeconds(processedSSM.rows() - 1) << std::endl;

        if (hasBest) {
            const AccumulatedScoreResult bestAccResult =
                    MyEigen::computeAccumulatedScoreMatrixForSegment(processedSSM, bestResult.segment);

            const PathFamily bestPathFamily =
                    MyEigen::computeOptimalPathFamily(bestAccResult.D);

            const std::vector<Segment> bestSegmentFamily =
                    MyEigen::computeInducedSegmentFamily(bestPathFamily);

            float repetitionDurationSum = 0.0f;
            for (const Segment &seg: bestSegmentFamily) {
                repetitionDurationSum += frameIndexToSeconds(seg.end) - frameIndexToSeconds(seg.start);
            }

            const float averageRepetitionDuration = bestSegmentFamily.empty()
                                                        ? 0.0f
                                                        : repetitionDurationSum / static_cast<float>(bestSegmentFamily.
                                                              size());

            std::cout << "bestResult.segment: ["
                    << bestResult.segment.start << ", "
                    << bestResult.segment.end << "]" << std::endl;
            std::cout << "bestResult.score: " << bestResult.score << std::endl;
            std::cout << "bestResult.normalizedScore: " << bestResult.normalizedScore << std::endl;
            std::cout << "bestResult.coverage: " << bestResult.coverage << std::endl;
            std::cout << "bestResult.normalizedCoverage: " << bestResult.normalizedCoverage << std::endl;
            std::cout << "bestResult.pathFamilyLength: " << bestResult.pathFamilyLength << std::endl;
            std::cout << "bestResult.fitness: " << bestResult.fitness << std::endl;
            std::cout << "bestResult average repetition duration (sec): "
                    << averageRepetitionDuration << std::endl;
            printSegmentTimeInfo("bestResult", bestResult.segment);
            std::cout << "bestSegmentFamily.size(): " << bestSegmentFamily.size() << std::endl;

            const float bestStartSec = frameIndexToSeconds(bestResult.segment.start);
            const float bestEndSec = frameIndexToSeconds(bestResult.segment.end);

            MyFfmpeg::cutTheAudio(title, bestStartSec, bestEndSec);

            // fadeOut 还是不要太长为好
            MyFfmpeg::applyFade(title + "_cut", 0.3, 1.5);
        } else {
            std::cout << "No valid candidate segment found in multi-level scan." << std::endl;
        }

        testLog("Step 7: 多层采样 candidate scan 完成");
        printElapsedMs("Step 7 完成");

        printElapsedMs("程序结束前收尾完成");
        // return 0;

    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
