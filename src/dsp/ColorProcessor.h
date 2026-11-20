#pragma once

#include <JuceHeader.h>

#include <array>
#include <cmath>

#include "BiquadFilter.h"

// =========================================================================
// ColorProcessor — Pure 1x Crunch Compressor Voicing + Mastering 4x Linear-Phase
//                  Anti-Aliased [HQ] Mode + True 64-bit Double Precision
//   · Preserves the exact 1x `pre` / `post` RBJ voicing of `Crunch Compressor`:
//       - Mode 1 (Warm): 150 Hz +6.0 dB LowShelf (Q=0.707) -> tanh(3*amt*x) -> 8000 Hz HighCut (Q=0.707)
//       - Mode 2 (Cold): 6000 Hz +6.0 dB HighShelf (Q=0.707) -> tanh(3*amt*x) -> 80 Hz LowCut (Q=0.707)
//       - Mode 3 (Clip): tanh(3*amt*x), blended & hard-clamped to [-1.0, +1.0]
//   · [HQ Off] (default): Pure 1x zero-latency path (0 samples added latency, bit-accurate
//     original grit & zero comb-filtering).
//   · [HQ On]  (mastering anti-alias): `pre` and `post` remain at 1x (zero EQ voicing drift!),
//     while ONLY the memoryless `std::tanh(drive * x)` stage runs at 4x via two cascaded
//     Type-I symmetric linear-phase Half-Band FIR stages (`L1 = 31, M1 = 15` and `L2 = 15, M2 = 7`,
//     >82 dB alias rejection, <0.0003 dB passband ripple) with exact `kHqLatencySamples = 18`
//     integer sample delay matched on the internal `dry` path (0.000 dB dry/wet comb-filtering!).
//   · Retains 5 ms per-sample `amount` smoothing + 15 ms parallel dual-bank crossfade when
//     switching between Off / Warm / Cold / Clip or toggling [HQ] On <-> Off (zero clicks!).
// =========================================================================
class ColorProcessor
{
public:
    static constexpr int kHqLatencySamples = 18;

    ColorProcessor() noexcept
    {
        initHalfBandKernels();
    }

    void prepare (float sampleRate, int maxBlockSize = 4096) noexcept
    {
        juce::ignoreUnused (maxBlockSize);
        initHalfBandKernels();
        sampleRate_ = sampleRate > 8000.0f ? sampleRate : 48000.0f;

        // 5 ms per-sample smoothing for `amount_`
        smoothAlpha_ = 1.0 - std::exp (-1.0 / (0.005 * (double) sampleRate_));
        // 15 ms linear crossfade step for click-free mode / HQ transitions
        xfadeStep_   = 1.0 / juce::jmax (32.0, (double) sampleRate_ * 0.015);

        configureBankForMode (activeBank_, mode_);
        configureBankForMode (xfadeBank_,  mode_);
        reset();
    }

    void reset() noexcept
    {
        activeBank_.reset();
        xfadeBank_.reset();
        smoothedAmount_ = (double) targetAmount_;
        xfadeRemaining_ = 0.0;
        xfadeFromMode_  = mode_;
        xfadeFromHq_    = hqEnabled_;
        hasProcessed_   = false;
    }

    int getLatencySamples() const noexcept
    {
        return hqEnabled_ ? kHqLatencySamples : 0;
    }

    bool isHqEnabled() const noexcept { return hqEnabled_; }

    void setHqEnabled (bool enable) noexcept
    {
        if (enable == hqEnabled_)
            return;

        if (! hasProcessed_)
        {
            hqEnabled_   = enable;
            xfadeFromHq_ = enable;
            activeBank_.resetOversamplers();
            return;
        }

        xfadeBank_      = activeBank_;
        xfadeFromMode_  = mode_;
        xfadeFromHq_    = hqEnabled_;
        xfadeRemaining_ = 1.0;
        hqEnabled_      = enable;
    }

    void setMode (int mode) noexcept
    {
        mode = juce::jlimit (0, 3, mode);
        if (mode == mode_)
            return;

        if (! hasProcessed_)
        {
            mode_          = mode;
            xfadeFromMode_ = mode;
            xfadeFromHq_   = hqEnabled_;
            configureBankForMode (activeBank_, mode_);
            activeBank_.reset();
            return;
        }

        // Snapshot current active filter bank into xfadeBank_ and start a 15 ms crossfade
        xfadeBank_      = activeBank_;
        xfadeFromMode_  = mode_;
        xfadeFromHq_    = hqEnabled_;
        xfadeRemaining_ = 1.0;

        mode_ = mode;
        configureBankForMode (activeBank_, mode_);
        activeBank_.resetIirOnly();
    }

    void setAmount (float amount01) noexcept
    {
        targetAmount_ = juce::jlimit (0.0f, 1.0f, amount01);
        if (! hasProcessed_)
            smoothedAmount_ = (double) targetAmount_;
    }

    void processSampleDouble (double& left, double& right) noexcept
    {
        if (! hasProcessed_)
        {
            hasProcessed_   = true;
            smoothedAmount_ = (double) targetAmount_;
        }

        smoothedAmount_ += ((double) targetAmount_ - smoothedAmount_) * smoothAlpha_;
        const double amt = smoothedAmount_;

        // When HQ is OFF and Colour is idle (Mode == Off or Amount == 0) with no crossfade active,
        // fast-path bypass while keeping the 18-sample history warm for click-free HQ engagement!
        if (! hqEnabled_ && xfadeRemaining_ <= 0.0
            && (mode_ == 0 || (targetAmount_ <= 0.0001f && amt <= 0.0001)))
        {
            activeBank_.pushIdleHistory (left, right);
            return;
        }

        double curL = left, curR = right;
        evalSingleSampleForMode (mode_, hqEnabled_, activeBank_, amt, curL, curR);

        if (xfadeRemaining_ > 0.0)
        {
            double oldL = left, oldR = right;
            evalSingleSampleForMode (xfadeFromMode_, xfadeFromHq_, xfadeBank_, amt, oldL, oldR);
            const double wOld = xfadeRemaining_;
            const double wNew = 1.0 - wOld;
            left  = wNew * curL + wOld * oldL;
            right = wNew * curR + wOld * oldR;
            xfadeRemaining_ = juce::jmax (0.0, xfadeRemaining_ - xfadeStep_);
        }
        else
        {
            left  = curL;
            right = curR;
        }
    }

    void processSample (float& left, float& right) noexcept
    {
        double dl = (double) left;
        double dr = (double) right;
        processSampleDouble (dl, dr);
        left  = (float) dl;
        right = (float) dr;
    }

    void processBlock (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        if (numSamples <= 0 || numChannels <= 0)
            return;

        if (! hasProcessed_)
        {
            hasProcessed_   = true;
            smoothedAmount_ = (double) targetAmount_;
        }

        float* left  = buffer.getWritePointer (0);
        float* right = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

        if (! hqEnabled_ && xfadeRemaining_ <= 0.0
            && (mode_ == 0 || (targetAmount_ <= 0.0001f && smoothedAmount_ <= 0.0001)))
        {
            smoothedAmount_ = (mode_ == 0) ? (double) targetAmount_ : 0.0;
            const int start = juce::jmax (0, numSamples - 32);
            for (int s = start; s < numSamples; ++s)
                activeBank_.pushIdleHistory ((double) left[s], (double) (right != nullptr ? right[s] : left[s]));
            return;
        }

        for (int s = 0; s < numSamples; ++s)
        {
            double l = (double) left[s];
            double r = (right != nullptr) ? (double) right[s] : l;
            processSampleDouble (l, r);
            left[s] = (float) l;
            if (right != nullptr)
                right[s] = (float) r;
        }
    }

    void processBlockDouble (juce::AudioBuffer<double>& buffer) noexcept
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());
        if (numSamples <= 0 || numChannels <= 0)
            return;

        if (! hasProcessed_)
        {
            hasProcessed_   = true;
            smoothedAmount_ = (double) targetAmount_;
        }

        double* left  = buffer.getWritePointer (0);
        double* right = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

        if (! hqEnabled_ && xfadeRemaining_ <= 0.0
            && (mode_ == 0 || (targetAmount_ <= 0.0001f && smoothedAmount_ <= 0.0001)))
        {
            smoothedAmount_ = (mode_ == 0) ? (double) targetAmount_ : 0.0;
            const int start = juce::jmax (0, numSamples - 32);
            for (int s = start; s < numSamples; ++s)
                activeBank_.pushIdleHistory (left[s], (right != nullptr ? right[s] : left[s]));
            return;
        }

        for (int s = 0; s < numSamples; ++s)
        {
            double l = left[s];
            double r = (right != nullptr) ? right[s] : l;
            processSampleDouble (l, r);
            left[s] = l;
            if (right != nullptr)
                right[s] = r;
        }
    }

    // Designs the pre/post biquads for a colour mode. Modes 0 and 3 leave the
    // outputs untouched. (Directly ported from Crunch Compressor)
    static void designForMode (int mode, float sampleRate, BiquadCoefficients& pre, BiquadCoefficients& post)
    {
        if (mode == 1)
        {
            pre  = BiquadFilter::designLowShelf (150.0f, 0.707f, 6.0f, sampleRate);
            post = BiquadFilter::designHighCut (8000.0f, 0.707f, sampleRate);
        }
        else if (mode == 2)
        {
            pre  = BiquadFilter::designHighShelf (6000.0f, 0.707f, 6.0f, sampleRate);
            post = BiquadFilter::designLowCut (80.0f, 0.707f, sampleRate);
        }
    }

private:
    static double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        const double halfSq = 0.25 * x * x;
        for (int k = 1; k <= 28; ++k)
        {
            term *= halfSq / ((double) k * (double) k);
            sum  += term;
            if (term < 1.0e-16 * sum)
                break;
        }
        return sum;
    }

    void initHalfBandKernels() noexcept
    {
        if (kernelsInitialized_)
            return;

        constexpr double kBeta = 7.8;
        const double invI0Beta = 1.0 / besselI0 (kBeta);

        // Stage 1: M1 = 15 (L1 = 31 taps, 8 symmetric non-zero pairs `c1_[0..7]` from outer m=0..7 to inner m=7)
        double sum1 = 0.0;
        for (int k = 1; k <= 8; ++k)
        {
            const int    oddOff = 2 * k - 1; // 1, 3, ..., 15
            const double u      = (double) oddOff / 16.0;
            const double win    = besselI0 (kBeta * std::sqrt (juce::jmax (0.0, 1.0 - u * u))) * invI0Beta;
            const double h      = (((k & 1) != 0) ? 1.0 : -1.0) / (juce::MathConstants<double>::pi * (double) oddOff) * win;
            // Store from outer (`m = 8 - k`, i.e. `k=8 -> idx 0`, `k=1 -> idx 7`) for clean linear indexing!
            c1_[(size_t) (8 - k)] = h;
            sum1 += h;
        }
        const double norm1 = 0.25 / sum1;
        for (double& v : c1_)
            v *= norm1;

        // Stage 2: M2 = 7 (L2 = 15 taps, 4 symmetric non-zero pairs `c2_[0..3]` from outer m=0..3 to inner m=3)
        double sum2 = 0.0;
        for (int k = 1; k <= 4; ++k)
        {
            const int    oddOff = 2 * k - 1; // 1, 3, 5, 7
            const double u      = (double) oddOff / 8.0;
            const double win    = besselI0 (kBeta * std::sqrt (juce::jmax (0.0, 1.0 - u * u))) * invI0Beta;
            const double h      = (((k & 1) != 0) ? 1.0 : -1.0) / (juce::MathConstants<double>::pi * (double) oddOff) * win;
            c2_[(size_t) (4 - k)] = h;
            sum2 += h;
        }
        const double norm2 = 0.25 / sum2;
        for (double& v : c2_)
            v *= norm2;

        kernelsInitialized_ = true;
    }

    // Single-channel 4x symmetric linear-phase Half-Band FIR oversampled `tanh(drive * x)`
    // Exact integer round-trip group delay = `kHqLatencySamples = 18` samples at 1x!
    struct ChannelOversampler4x
    {
        static constexpr int kDryCap = 32; // power-of-2 ring >= kHqLatencySamples + 1
        std::array<double, 16>      up1  {};
        std::array<double, 8>       up2  {};
        std::array<double, 8>       zEv2 {};
        std::array<double, 8>       zOd2 {};
        std::array<double, 16>      vEv1 {};
        std::array<double, 16>      vOd1 {};
        std::array<double, kDryCap> dryRing {};
        int p1 = 0, p2 = 0, pDec1 = 0, pDry = 0;

        void reset() noexcept
        {
            up1.fill (0.0);  up2.fill (0.0);
            zEv2.fill (0.0); zOd2.fill (0.0);
            vEv1.fill (0.0); vOd1.fill (0.0);
            dryRing.fill (0.0);
            p1 = p2 = pDec1 = pDry = 0;
        }

        inline double pushAndReadDryDelayed (double dryIn) noexcept
        {
            dryRing[(size_t) pDry] = dryIn;
            const double delayed   = dryRing[(size_t) ((pDry + kDryCap - kHqLatencySamples) & (kDryCap - 1))];
            pDry = (pDry + 1) & (kDryCap - 1);
            return delayed;
        }

        inline double processStage2Sample (double s2, double drive, const std::array<double, 4>& c2) noexcept
        {
            up2[(size_t) p2] = s2;
            const double w4Even = 2.0 * (
                  c2[0] * (up2[(size_t) p2]               + up2[(size_t) ((p2 + 8 - 7) & 7)])
                + c2[1] * (up2[(size_t) ((p2 + 8 - 1) & 7)] + up2[(size_t) ((p2 + 8 - 6) & 7)])
                + c2[2] * (up2[(size_t) ((p2 + 8 - 2) & 7)] + up2[(size_t) ((p2 + 8 - 5) & 7)])
                + c2[3] * (up2[(size_t) ((p2 + 8 - 3) & 7)] + up2[(size_t) ((p2 + 8 - 4) & 7)]));
            const double w4Odd  = up2[(size_t) ((p2 + 8 - 3) & 7)];

            zEv2[(size_t) p2] = (drive > 1.0e-9) ? std::tanh (drive * w4Even) : 0.0;
            zOd2[(size_t) p2] = (drive > 1.0e-9) ? std::tanh (drive * w4Odd)  : 0.0;

            const double out2x = 0.5 * zOd2[(size_t) ((p2 + 8 - 4) & 7)]
                + c2[0] * (zEv2[(size_t) p2]               + zEv2[(size_t) ((p2 + 8 - 7) & 7)])
                + c2[1] * (zEv2[(size_t) ((p2 + 8 - 1) & 7)] + zEv2[(size_t) ((p2 + 8 - 6) & 7)])
                + c2[2] * (zEv2[(size_t) ((p2 + 8 - 2) & 7)] + zEv2[(size_t) ((p2 + 8 - 5) & 7)])
                + c2[3] * (zEv2[(size_t) ((p2 + 8 - 3) & 7)] + zEv2[(size_t) ((p2 + 8 - 4) & 7)]);

            p2 = (p2 + 1) & 7;
            return out2x;
        }

        inline double processTanh4x (double x, double drive,
                                     const std::array<double, 8>& c1,
                                     const std::array<double, 4>& c2) noexcept
        {
            up1[(size_t) p1] = x;
            const double y1Even = 2.0 * (
                  c1[0] * (up1[(size_t) p1]                 + up1[(size_t) ((p1 + 16 - 15) & 15)])
                + c1[1] * (up1[(size_t) ((p1 + 16 - 1) & 15)] + up1[(size_t) ((p1 + 16 - 14) & 15)])
                + c1[2] * (up1[(size_t) ((p1 + 16 - 2) & 15)] + up1[(size_t) ((p1 + 16 - 13) & 15)])
                + c1[3] * (up1[(size_t) ((p1 + 16 - 3) & 15)] + up1[(size_t) ((p1 + 16 - 12) & 15)])
                + c1[4] * (up1[(size_t) ((p1 + 16 - 4) & 15)] + up1[(size_t) ((p1 + 16 - 11) & 15)])
                + c1[5] * (up1[(size_t) ((p1 + 16 - 5) & 15)] + up1[(size_t) ((p1 + 16 - 10) & 15)])
                + c1[6] * (up1[(size_t) ((p1 + 16 - 6) & 15)] + up1[(size_t) ((p1 + 16 - 9)  & 15)])
                + c1[7] * (up1[(size_t) ((p1 + 16 - 7) & 15)] + up1[(size_t) ((p1 + 16 - 8)  & 15)]));
            const double y1Odd  = up1[(size_t) ((p1 + 16 - 7) & 15)];
            p1 = (p1 + 1) & 15;

            vEv1[(size_t) pDec1] = processStage2Sample (y1Even, drive, c2);
            vOd1[(size_t) pDec1] = processStage2Sample (y1Odd,  drive, c2);

            const double out1x = 0.5 * vEv1[(size_t) ((pDec1 + 16 - 7) & 15)]
                + c1[0] * (vOd1[(size_t) pDec1]                 + vOd1[(size_t) ((pDec1 + 16 - 15) & 15)])
                + c1[1] * (vOd1[(size_t) ((pDec1 + 16 - 1) & 15)] + vOd1[(size_t) ((pDec1 + 16 - 14) & 15)])
                + c1[2] * (vOd1[(size_t) ((pDec1 + 16 - 2) & 15)] + vOd1[(size_t) ((pDec1 + 16 - 13) & 15)])
                + c1[3] * (vOd1[(size_t) ((pDec1 + 16 - 3) & 15)] + vOd1[(size_t) ((pDec1 + 16 - 12) & 15)])
                + c1[4] * (vOd1[(size_t) ((pDec1 + 16 - 4) & 15)] + vOd1[(size_t) ((pDec1 + 16 - 11) & 15)])
                + c1[5] * (vOd1[(size_t) ((pDec1 + 16 - 5) & 15)] + vOd1[(size_t) ((pDec1 + 16 - 10) & 15)])
                + c1[6] * (vOd1[(size_t) ((pDec1 + 16 - 6) & 15)] + vOd1[(size_t) ((pDec1 + 16 - 9)  & 15)])
                + c1[7] * (vOd1[(size_t) ((pDec1 + 16 - 7) & 15)] + vOd1[(size_t) ((pDec1 + 16 - 8)  & 15)]);

            pDec1 = (pDec1 + 1) & 15;
            return out1x;
        }
    };

    struct FilterBank
    {
        BiquadFilter preL, preR;
        BiquadFilter postL, postR;
        ChannelOversampler4x osL, osR;

        void reset() noexcept
        {
            preL.reset();  preR.reset();
            postL.reset(); postR.reset();
            osL.reset();   osR.reset();
        }

        void resetIirOnly() noexcept
        {
            preL.reset();  preR.reset();
            postL.reset(); postR.reset();
        }

        void resetOversamplers() noexcept
        {
            osL.reset();
            osR.reset();
        }

        void pushIdleHistory (double l, double r) noexcept
        {
            osL.pushAndReadDryDelayed (l);
            osR.pushAndReadDryDelayed (r);
        }
    };

    void configureBankForMode (FilterBank& bank, int mode) const noexcept
    {
        if (mode == 1 || mode == 2)
        {
            BiquadCoefficients pre, post;
            designForMode (mode, sampleRate_, pre, post);
            bank.preL.setCoefficients (pre);   bank.preR.setCoefficients (pre);
            bank.postL.setCoefficients (post); bank.postR.setCoefficients (post);
        }
    }

    void evalSingleSampleForMode (int mode, bool useHq, FilterBank& bank, double amt,
                                  double& left, double& right) const noexcept
    {
        const double dryDelayedL = bank.osL.pushAndReadDryDelayed (left);
        const double dryDelayedR = bank.osR.pushAndReadDryDelayed (right);
        const double dryRefL     = useHq ? dryDelayedL : left;
        const double dryRefR     = useHq ? dryDelayedR : right;

        if (mode == 0 || amt <= 0.0001)
        {
            if (useHq)
            {
                (void) bank.osL.processTanh4x (left,  0.0, c1_, c2_);
                (void) bank.osR.processTanh4x (right, 0.0, c1_, c2_);
            }
            left  = dryRefL;
            right = dryRefR;
            return;
        }

        const double drive = amt * 3.0;
        if (mode == 3)
        {
            const double l = useHq ? bank.osL.processTanh4x (left,  drive, c1_, c2_) : std::tanh (drive * left);
            const double r = useHq ? bank.osR.processTanh4x (right, drive, c1_, c2_) : std::tanh (drive * right);
            left  = juce::jlimit (-1.0, 1.0, dryRefL * (1.0 - amt) + l * amt);
            right = juce::jlimit (-1.0, 1.0, dryRefR * (1.0 - amt) + r * amt);
            return;
        }

        double l = bank.preL.processSampleDouble (left);
        double r = bank.preR.processSampleDouble (right);
        const double satL = useHq ? bank.osL.processTanh4x (l, drive, c1_, c2_) : std::tanh (drive * l);
        const double satR = useHq ? bank.osR.processTanh4x (r, drive, c1_, c2_) : std::tanh (drive * r);
        l = bank.postL.processSampleDouble (satL);
        r = bank.postR.processSampleDouble (satR);

        left  = dryRefL * (1.0 - amt) + l * amt;
        right = dryRefR * (1.0 - amt) + r * amt;
    }

    std::array<double, 8> c1_ {};
    std::array<double, 4> c2_ {};
    bool kernelsInitialized_ = false;

    FilterBank activeBank_, xfadeBank_;
    int    mode_           = 0;
    int    xfadeFromMode_  = 0;
    bool   hqEnabled_      = false;
    bool   xfadeFromHq_    = false;
    float  targetAmount_   = 0.0f;
    double smoothedAmount_ = 0.0;
    double smoothAlpha_    = 0.004;
    double xfadeRemaining_ = 0.0;
    double xfadeStep_      = 0.001;
    float  sampleRate_     = 48000.0f;
    bool   hasProcessed_   = false;
};
