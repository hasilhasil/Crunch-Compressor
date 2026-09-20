#include "Compressor.h"

void Compressor::prepare (double sampleRate, int numChannels)
{
    juce::ignoreUnused (numChannels);

    sampleRate_ = sampleRate;

    // delay lines are sized once for the maximum lookahead (3 ms, see Param::lookahead)
    const int maxLookaheadSamples = (int) std::ceil (3.0 * 0.001 * sampleRate_) + 1;
    delayL_.assign ((size_t) maxLookaheadSamples, 0.0f);
    delayR_.assign ((size_t) maxLookaheadSamples, 0.0f);

    reset();
}

void Compressor::reset()
{
    smoothedGainDb_ = 0.0f;
    gainReductionDb_ = 0.0f;
    rmsEnvelope_     = 0.0f;
    avgGainReductionDb_ = 0.0f;
    makeupDb_.store (0.0f, std::memory_order_relaxed);
    lookaheadSamples_ = 0;
    clearDelayLines();
}

void Compressor::clearDelayLines()
{
    std::fill (delayL_.begin(), delayL_.end(), 0.0f);
    std::fill (delayR_.begin(), delayR_.end(), 0.0f);
    writePos_ = 0;
}

void Compressor::setParameters (float thresholdDb, float ratio, float attackMs, float releaseMs,
                                float kneeDb, bool useRmsDetector, float lookaheadMs, bool autoGain)
{
    thresholdDb_ = thresholdDb;
    ratio_       = juce::jmax (1.0f, ratio);
    kneeDb_      = juce::jmax (0.0f, kneeDb);
    useRms_      = useRmsDetector;

    const float attackSec  = juce::jmax (0.0001f, attackMs  * 0.001f);
    const float releaseSec = juce::jmax (0.0001f, releaseMs * 0.001f);

    // one-pole coefficients: ~63% of the way to the target per time constant
    attackCoeff_  = 1.0f - std::exp (-1.0f / (sampleRate_ * attackSec));
    releaseCoeff_ = 1.0f - std::exp (-1.0f / (sampleRate_ * releaseSec));

    // 10 ms RMS integration window
    rmsCoeff_ = 1.0f - std::exp (-1.0f / (sampleRate_ * 0.010f));

    // Auto gain: the target is the original dry input level, so the makeup is
    // measured (mean gain reduction) rather than derived from threshold/ratio.
    autoGain_          = autoGain;
    autoGainCoeff_     = 1.0f - std::exp (-1.0f / (sampleRate_ * (double) kAutoGainAverageSec));
    makeupSmoothCoeff_ = 1.0f - std::exp (-1.0f / (sampleRate_ * (double) kMakeupSmoothSec));

    const int wantSamples = (int) std::ceil (lookaheadMs * 0.001f * sampleRate_);
    const int clamped     = juce::jlimit (0, (int) delayL_.size() - 1, wantSamples);
    if (clamped != lookaheadSamples_)
    {
        lookaheadSamples_ = clamped;
        clearDelayLines();
    }
}

float Compressor::computeGainDb (float levelDb) const noexcept
{
    const float slope    = 1.0f / ratio_ - 1.0f;   // negative for ratio > 1
    const float halfKnee = kneeDb_ * 0.5f;

    if (kneeDb_ <= 0.0f)
        return levelDb > thresholdDb_ ? slope * (levelDb - thresholdDb_) : 0.0f;

    if (levelDb <= thresholdDb_ - halfKnee)
        return 0.0f;

    if (levelDb >= thresholdDb_ + halfKnee)
        return slope * (levelDb - thresholdDb_);

    // quadratic interpolation inside the knee
    const float over = levelDb - thresholdDb_ + halfKnee;   // 0 .. kneeDb_
    return slope * over * over / (2.0f * kneeDb_);
}

void Compressor::processSample (float& left, float& right) noexcept
{
    // linked detector: the loudest channel drives the gain
    float level = std::max (std::abs (left), std::abs (right));
    if (useRms_)
    {
        const float meanSquare = 0.5f * (left * left + right * right);
        rmsEnvelope_ += (meanSquare - rmsEnvelope_) * rmsCoeff_;
        level = std::sqrt (rmsEnvelope_);
    }

    const float levelDb         = juce::Decibels::gainToDecibels (level + 1.0e-9f);
    lastDetectorDb_             = levelDb;
    const float desiredGainDb   = computeGainDb (levelDb);
    const float coeff           = (desiredGainDb < smoothedGainDb_) ? attackCoeff_ : releaseCoeff_;
    smoothedGainDb_ += (desiredGainDb - smoothedGainDb_) * coeff;
    gainReductionDb_ = smoothedGainDb_;

    // Auto gain: align the (average) output level to the dry input level. The
    // makeup is a FAST (~100 ms) average of the gain reduction the compressor
    // actually applied, so on average in + GR + makeup == in. The average
    // always tracks (even when auto gain is off) so enabling it is seamless,
    // and it is open loop: the detector reads the INPUT, so the measured
    // reduction does not depend on the makeup and the loop cannot run away.
    // A short smoothing on the applied makeup removes any click when the
    // switch is toggled.
    // NOTE: keep the average window short. A multi-second window made the
    // output gradually swell for seconds after auto gain was enabled (the
    // makeup had to ramp from 0 to the reduction), which reads as a fade-in
    // rather than instant level compensation.
    avgGainReductionDb_ += (smoothedGainDb_ - avgGainReductionDb_) * autoGainCoeff_;
    const float makeupTarget  = autoGain_ ? -avgGainReductionDb_ : 0.0f;
    const float currentMakeup = makeupDb_.load (std::memory_order_relaxed);
    const float makeupDb      = currentMakeup + (makeupTarget - currentMakeup) * makeupSmoothCoeff_;
    makeupDb_.store (makeupDb, std::memory_order_relaxed);

    // lookahead: the detector has already seen these samples; delay the audio
    // so the gain reduction lands before the transient instead of after it.
    if (lookaheadSamples_ > 0)
    {
        const size_t size = delayL_.size();
        delayL_[(size_t) writePos_] = left;
        delayR_[(size_t) writePos_] = right;

        int readPos = writePos_ - lookaheadSamples_;
        if (readPos < 0)
            readPos += (int) size;

        left  = delayL_[(size_t) readPos];
        right = delayR_[(size_t) readPos];

        writePos_ = (writePos_ + 1 == (int) size) ? 0 : writePos_ + 1;
    }

    const float gain = juce::Decibels::decibelsToGain (smoothedGainDb_ + makeupDb);
    left  *= gain;
    right *= gain;
}
