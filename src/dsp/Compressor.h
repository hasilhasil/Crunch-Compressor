#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <cmath>
#include <vector>

#include "../Parameters.h"

// Feed-forward compressor: linked peak/RMS detector -> soft/hard knee gain
// computer -> attack/release smoothed gain -> optional lookahead delay line.
// Optional auto gain: a slowly averaged makeup equal to the mean gain
// reduction, so the output level stays aligned with the dry input level.
class Compressor
{
public:
    Compressor() = default;

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setParameters (float thresholdDb, float ratio, float attackMs, float releaseMs,
                        float kneeDb, bool useRmsDetector, float lookaheadMs, bool autoGain);

    void processSample (float& left, float& right) noexcept;

    float getGainReductionDb() const noexcept { return gainReductionDb_; }
    int   getLookaheadSamples() const noexcept { return lookaheadSamples_; }

    // Static transfer function: gain (dB) applied to a signal at levelDb,
    // including the current auto-gain makeup. Used by the UI to draw the knee
    // curve. The makeup is atomically mirrored for the UI thread.
    float getAppliedGainDb (float levelDb) const noexcept
    {
        return computeGainDb (levelDb) + makeupDb_.load (std::memory_order_relaxed);
    }

    // Detector level (dB) of the last processed sample; used by the host to
    // record the input level history for the display.
    float getLastDetectorDb() const noexcept { return lastDetectorDb_; }

private:
    float computeGainDb (float levelDb) const noexcept;
    void  clearDelayLines();

    // Auto-gain measurement window (fast average of the gain reduction) and
    // de-click smoothing for the makeup actually applied. The window is kept
    // SHORT: with a ~1 s average the makeup audibly ramped up for seconds
    // after auto gain was switched on (and after every transport restart,
    // where reset() clears the average), reading as a slow fade-in.
    static constexpr float kAutoGainAverageSec = 0.1f;
    static constexpr float kMakeupSmoothSec    = 0.02f;

    double sampleRate_  = 48000.0;

    float thresholdDb_ = Param::kThresholdDefault;
    float ratio_       = Param::kRatioDefault;
    float kneeDb_      = 0.0f;
    bool  useRms_      = false;

    // Auto gain (target = dry input level). avgGainReductionDb_ follows the
    // applied gain reduction with a short (~100 ms) time constant; the makeup
    // is its negation, so on average the compressed output returns to the
    // level the detector saw. Written on the audio thread, mirrored atomically
    // for the UI transfer curve.
    bool  autoGain_           = false;
    float avgGainReductionDb_ = 0.0f;
    float autoGainCoeff_      = 0.0f;
    float makeupSmoothCoeff_  = 0.0f;
    std::atomic<float> makeupDb_ { 0.0f };

    float attackCoeff_  = 0.0f;
    float releaseCoeff_ = 0.0f;
    float rmsCoeff_     = 0.0f;

    float smoothedGainDb_ = 0.0f;
    float gainReductionDb_ = 0.0f;
    float rmsEnvelope_     = 0.0f;
    float lastDetectorDb_  = -100.0f;

    std::vector<float> delayL_;
    std::vector<float> delayR_;
    int writePos_         = 0;
    int lookaheadSamples_ = 0;
};
