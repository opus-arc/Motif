//
// Created by opus arc on 2026/3/20.
//

#ifndef MOTIF_MYEIGEN_H
#define MOTIF_MYEIGEN_H
#include <vector>

#include "Public.h"

class MyEigen {
public:
    static Eigen::MatrixXf convertChromaSequenceToMatrix(
        const std::vector<std::vector<float> > &chromaSequence);

    static Eigen::MatrixXf computeSelfSimilarityMatrix(
        const Eigen::MatrixXf &chromaMatrix,
        size_t maxFramesForTest = 300);

    static Eigen::MatrixXf smoothFeatureMatrix(
        const Eigen::MatrixXf &featureMatrix,
        size_t windowSize);

    static Eigen::MatrixXf downsampleFeatureMatrix(
        const Eigen::MatrixXf &featureMatrix,
        size_t factor);

    static Eigen::MatrixXf thresholdAndPenaltyMatrix(
        const Eigen::MatrixXf &ssm,
        float threshold,
        float penalty);

    static void saveMatrixToCSV(const Eigen::MatrixXf &matrix, const std::string &filePath);

    static AccumulatedScoreResult computeAccumulatedScoreMatrixForSegment(
        const Eigen::MatrixXf &ssm,
        const Segment &segment);

    static PathFamily computeOptimalPathFamily(const Eigen::MatrixXf &D);

    static std::vector<Segment> computeInducedSegmentFamily(const PathFamily &pathFamily);

    static int computeCoverage(const std::vector<Segment> &segmentFamily);

    static FitnessResult evaluateSegment(
        const Eigen::MatrixXf &processedSSM,
        const Segment &segment);

};


#endif //MOTIF_MYEIGEN_H
