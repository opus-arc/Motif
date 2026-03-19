//
// Created by opus arc on 2026/3/20.
//

#include "../MyEigen.h"

#include "MyFFT.h"

#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <stdexcept>

Eigen::MatrixXf MyEigen::convertChromaSequenceToMatrix(
    const std::vector<std::vector<float>>& chromaSequence) {

    if (chromaSequence.empty()) {
        return Eigen::MatrixXf();
    }

    const int numFrames = static_cast<int>(chromaSequence.size());
    const int chromaDim = static_cast<int>(chromaSequence[0].size());

    Eigen::MatrixXf chromaMatrix(numFrames, chromaDim);

    for (int i = 0; i < numFrames; ++i) {
        for (int j = 0; j < chromaDim; ++j) {
            chromaMatrix(i, j) = chromaSequence[i][j];
        }
    }

    return chromaMatrix;
}

Eigen::MatrixXf MyEigen::computeSelfSimilarityMatrix(
    const Eigen::MatrixXf& chromaMatrix,
    const size_t maxFramesForTest) {

    if (chromaMatrix.rows() == 0 || chromaMatrix.cols() == 0) {
        return Eigen::MatrixXf();
    }

    const int numFrames = std::min<int>(
        static_cast<int>(maxFramesForTest),
        chromaMatrix.rows()
    );

    Eigen::MatrixXf ssm(numFrames, numFrames);
    ssm.setZero();

    for (int i = 0; i < numFrames; ++i) {
        const Eigen::VectorXf a = chromaMatrix.row(i);
        const float normA = a.norm();

        for (int j = i; j < numFrames; ++j) {
            const Eigen::VectorXf b = chromaMatrix.row(j);
            const float normB = b.norm();

            float sim = 0.0f;
            if (normA > 1e-8f && normB > 1e-8f) {
                sim = a.dot(b) / (normA * normB);
            }

            ssm(i, j) = sim;
            ssm(j, i) = sim;
        }
    }

    return ssm;
}

Eigen::MatrixXf MyEigen::smoothFeatureMatrix(
    const Eigen::MatrixXf& featureMatrix,
    const size_t windowSize) {

    if (featureMatrix.rows() == 0 || featureMatrix.cols() == 0) {
        return Eigen::MatrixXf();
    }

    if (windowSize == 0) {
        throw std::invalid_argument("windowSize must be > 0");
    }

    const int numFrames = featureMatrix.rows();
    const int featureDim = featureMatrix.cols();
    const int radius = static_cast<int>(windowSize / 2);

    Eigen::MatrixXf smoothed(numFrames, featureDim);
    smoothed.setZero();

    for (int i = 0; i < numFrames; ++i) {
        const int start = std::max(0, i - radius);
        const int end = std::min(numFrames - 1, i + radius);
        const int count = end - start + 1;

        for (int j = 0; j < featureDim; ++j) {
            float sum = 0.0f;
            for (int k = start; k <= end; ++k) {
                sum += featureMatrix(k, j);
            }
            smoothed(i, j) = sum / static_cast<float>(count);
        }
    }

    return smoothed;
}

Eigen::MatrixXf MyEigen::downsampleFeatureMatrix(
    const Eigen::MatrixXf& featureMatrix,
    const size_t factor) {

    if (featureMatrix.rows() == 0 || featureMatrix.cols() == 0) {
        return Eigen::MatrixXf();
    }

    if (factor == 0) {
        throw std::invalid_argument("factor must be > 0");
    }

    const int numFrames = featureMatrix.rows();
    const int featureDim = featureMatrix.cols();
    const int downsampledRows = (numFrames + static_cast<int>(factor) - 1) / static_cast<int>(factor);

    Eigen::MatrixXf downsampled(downsampledRows, featureDim);
    downsampled.setZero();

    for (int outRow = 0; outRow < downsampledRows; ++outRow) {
        const int start = outRow * static_cast<int>(factor);
        const int endExclusive = std::min(numFrames, start + static_cast<int>(factor));
        const int count = endExclusive - start;

        for (int j = 0; j < featureDim; ++j) {
            float sum = 0.0f;
            for (int k = start; k < endExclusive; ++k) {
                sum += featureMatrix(k, j);
            }
            downsampled(outRow, j) = sum / static_cast<float>(count);
        }
    }

    return downsampled;
}

Eigen::MatrixXf MyEigen::thresholdAndPenaltyMatrix(
    const Eigen::MatrixXf& ssm,
    const float threshold,
    const float penalty) {

    if (ssm.rows() == 0 || ssm.cols() == 0) {
        return Eigen::MatrixXf();
    }

    if (ssm.rows() != ssm.cols()) {
        throw std::invalid_argument("SSM must be square");
    }

    Eigen::MatrixXf processed = ssm;
    const int n = processed.rows();

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                processed(i, j) = 1.0f;
                continue;
            }

            if (processed(i, j) < threshold) {
                processed(i, j) = penalty;
            }
        }
    }

    return processed;
}

void MyEigen::saveMatrixToCSV(const Eigen::MatrixXf& matrix, const std::string& filePath) {
    if (matrix.rows() == 0 || matrix.cols() == 0) {
        throw std::invalid_argument("matrix is empty");
    }

    std::ofstream out(filePath);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open file: " + filePath);
    }

    for (int i = 0; i < matrix.rows(); ++i) {
        for (int j = 0; j < matrix.cols(); ++j) {
            out << matrix(i, j);
            if (j + 1 < matrix.cols()) {
                out << ",";
            }
        }
        out << "\n";
    }

    out.close();
}

AccumulatedScoreResult MyEigen::computeAccumulatedScoreMatrixForSegment(
    const Eigen::MatrixXf& ssm,
    const Segment& segment) {

    if (ssm.rows() == 0 || ssm.cols() == 0) {
        throw std::invalid_argument("SSM is empty");
    }

    if (ssm.rows() != ssm.cols()) {
        throw std::invalid_argument("SSM must be square");
    }

    if (segment.start < 0 || segment.end < segment.start || segment.end >= ssm.cols()) {
        throw std::invalid_argument("Invalid segment range");
    }

    const int N = ssm.rows();
    const int M = segment.end - segment.start + 1;

    Eigen::MatrixXf S_seg = ssm.block(0, segment.start, N, M);

    const float negInf = -1e9f;
    Eigen::MatrixXf D = Eigen::MatrixXf::Constant(N, M + 1, negInf);

    D(0, 0) = 0.0f;
    D(0, 1) = S_seg(0, 0);

    for (int n = 1; n < N; ++n) {
        D(n, 0) = std::max(D(n - 1, 0), D(n - 1, M));

        D(n, 1) = D(n, 0) + S_seg(n, 0);

        for (int m = 2; m <= M; ++m) {
            float bestPrev = D(n - 1, m - 1);

            if (n - 1 >= 0 && m - 2 >= 0) {
                bestPrev = std::max(bestPrev, D(n - 1, m - 2));
            }

            if (n - 2 >= 0 && m - 1 >= 0) {
                bestPrev = std::max(bestPrev, D(n - 2, m - 1));
            }

            D(n, m) = S_seg(n, m - 1) + bestPrev;
        }
    }

    const float score = std::max(D(N - 1, 0), D(N - 1, M));

    return {D, score};
}

PathFamily MyEigen::computeOptimalPathFamily(const Eigen::MatrixXf& D) {
    if (D.rows() == 0 || D.cols() == 0) {
        return {};
    }

    const int N = static_cast<int>(D.rows());
    const int M = static_cast<int>(D.cols());   // 注意：这里包含 elevator column

    PathFamily pathFamily;
    Path path;

    int n = N - 1;
    int m = (D(n, M - 1) < D(n, 0)) ? 0 : (M - 1);

    if (m == M - 1) {
        path.push_back({N - 1, M - 2});
    }

    while (n > 0 || m > 0) {
        int nextN = n;
        int nextM = m;

        if (n == 0) {
            nextN = 0;
            nextM = m - 1;
        }
        else if (m == 0) {
            if (D(n - 1, M - 1) > D(n - 1, 0)) {
                nextN = n - 1;
                nextM = M - 1;

                if (!path.empty()) {
                    std::reverse(path.begin(), path.end());
                    pathFamily.push_back(path);
                }

                path.clear();
                path.push_back({nextN, M - 2});
            } else {
                nextN = n - 1;
                nextM = 0;
            }
        }
        else if (m == 1) {
            nextN = n;
            nextM = 0;
        }
        else {
            float bestVal = -1e30f;
            int bestN = -1;
            int bestM = -1;

            // predecessor: (n-1, m-1)
            if (n - 1 >= 0 && m - 1 >= 0) {
                if (D(n - 1, m - 1) > bestVal) {
                    bestVal = D(n - 1, m - 1);
                    bestN = n - 1;
                    bestM = m - 1;
                }
            }

            // predecessor: (n-1, m-2)
            if (n - 1 >= 0 && m - 2 >= 0) {
                if (D(n - 1, m - 2) > bestVal) {
                    bestVal = D(n - 1, m - 2);
                    bestN = n - 1;
                    bestM = m - 2;
                }
            }

            // predecessor: (n-2, m-1)
            if (n - 2 >= 0 && m - 1 >= 0) {
                if (D(n - 2, m - 1) > bestVal) {
                    bestVal = D(n - 2, m - 1);
                    bestN = n - 2;
                    bestM = m - 1;
                }
            }

            nextN = bestN;
            nextM = bestM;

            if (nextM >= 1) {
                path.push_back({nextN, nextM - 1});
            }
        }

        n = nextN;
        m = nextM;
    }

    if (!path.empty()) {
        std::reverse(path.begin(), path.end());
        pathFamily.push_back(path);
    }

    std::reverse(pathFamily.begin(), pathFamily.end());
    return pathFamily;
}


std::vector<Segment> MyEigen::computeInducedSegmentFamily(const PathFamily& pathFamily) {
    std::vector<Segment> segmentFamily;

    for (const Path& path : pathFamily) {
        if (path.empty()) {
            continue;
        }

        const int startRow = path.front().row;
        const int endRow = path.back().row;

        if (endRow < startRow) {
            throw std::invalid_argument("Invalid path: endRow < startRow");
        }

        segmentFamily.push_back({startRow, endRow});
    }

    return segmentFamily;
}

int MyEigen::computeCoverage(const std::vector<Segment>& segmentFamily) {
    int coverage = 0;

    for (const Segment& segment : segmentFamily) {
        if (segment.end < segment.start) {
            throw std::invalid_argument("Invalid segment: end < start");
        }

        coverage += (segment.end - segment.start + 1);
    }

    return coverage;
}

FitnessResult MyEigen::evaluateSegment(const Eigen::MatrixXf& processedSSM,
                                       const Segment& segment) {
    const AccumulatedScoreResult accResult =
        MyEigen::computeAccumulatedScoreMatrixForSegment(processedSSM, segment);

    const PathFamily pathFamily =
        MyEigen::computeOptimalPathFamily(accResult.D);

    const std::vector<Segment> segmentFamily =
        MyEigen::computeInducedSegmentFamily(pathFamily);

    const int coverage =
        MyEigen::computeCoverage(segmentFamily);

    int pathFamilyLength = 0;
    for (const Path& path : pathFamily) {
        pathFamilyLength += static_cast<int>(path.size());
    }

    const int M = segment.end - segment.start + 1;
    const int N = processedSSM.rows();

    const float normalizedScore =
        (accResult.score - static_cast<float>(M)) /
        static_cast<float>(pathFamilyLength);

    const float normalizedCoverage =
        static_cast<float>(coverage - M) /
        static_cast<float>(N);

    const float eps = 1e-8f;
    const float fitness =
        2.0f * normalizedScore * normalizedCoverage /
        (normalizedScore + normalizedCoverage + eps);

    return {
        segment,
        accResult.score,
        normalizedScore,
        coverage,
        normalizedCoverage,
        fitness,
        pathFamilyLength
    };
}
