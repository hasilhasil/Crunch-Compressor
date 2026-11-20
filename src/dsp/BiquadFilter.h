#pragma once

#include <JuceHeader.h>

#include <cmath>
#include <complex>

struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;   // a0 is normalised to 1.0

    inline void clampToStableTriangle() noexcept
    {
        if (! (std::isfinite (b0) && std::isfinite (b1) && std::isfinite (b2)
               && std::isfinite (a1) && std::isfinite (a2)))
        {
            b0 = 1.0; b1 = 0.0; b2 = 0.0; a1 = 0.0; a2 = 0.0;
            return;
        }
        a2 = juce::jlimit (-0.999999, 0.999999, a2);
        const double maxAbsA1 = juce::jmax (0.0, (1.0 + a2) - 1.0e-7);
        a1 = juce::jlimit (-maxAbsA1, maxAbsA1, a1);
    }
};

class BiquadFilter
{
public:
    void setCoefficients (const BiquadCoefficients& c) noexcept
    {
        b0_ = c.b0; b1_ = c.b1; b2_ = c.b2;
        a1_ = c.a1; a2_ = c.a2;

        // Cytomic ZDF TPT SVF state-space mapping (ported from Crunch EQ):
        // algebraically equivalent steady-state transfer function with superior
        // numerical stability and modulation behavior in 64-bit precision.
        const double dPlus  = 1.0 + a1_ + a2_;
        const double dMinus = 1.0 - a1_ + a2_;
        if (dPlus > 1.0e-15 && dMinus > 1.0e-15)
        {
            const double g     = std::sqrt (dPlus / dMinus);
            const double gk    = (2.0 * (1.0 - a2_)) / dMinus;
            const double denom = 1.0 + g * g + gk;
            svfA1_ = 1.0 / denom;
            svfA2_ = g * svfA1_;
            svfA3_ = g * svfA2_;

            m0_ = (b0_ - b1_ + b2_) / dMinus;
            m2_ = (b0_ + b1_ + b2_) / dPlus - m0_;
            m1_ = (2.0 * ((b0_ - b2_) - m0_ * (1.0 - a2_))) / (g * dMinus);
            useSvf_ = true;
        }
        else
        {
            useSvf_ = false;
        }
    }

    void reset() noexcept
    {
        s1_ = 0.0;
        s2_ = 0.0;
    }

    BiquadCoefficients getCoefficients() const noexcept
    {
        return { b0_, b1_, b2_, a1_, a2_ };
    }

    std::complex<double> responseAtDouble (double freqHz, double sampleRate) const noexcept
    {
        const double w  = juce::MathConstants<double>::twoPi * freqHz / sampleRate;
        const double cw = std::cos (w), sw = std::sin (w);
        const double c2 = cw * cw - sw * sw, s2 = 2.0 * sw * cw;
        const std::complex<double> num (b0_ + b1_ * cw + b2_ * c2, -(b1_ * sw + b2_ * s2));
        const std::complex<double> den (1.0 + a1_ * cw + a2_ * c2, -(a1_ * sw + a2_ * s2));
        return (std::norm (den) > 1.0e-30) ? (num / den) : std::complex<double> (1.0, 0.0);
    }

    double processSampleDouble (double in) noexcept
    {
        if (useSvf_)
        {
            const double v3 = in - s2_;
            const double v1 = svfA1_ * s1_ + svfA2_ * v3;
            const double v2 = s2_ + svfA2_ * s1_ + svfA3_ * v3;
            s1_ = 2.0 * v1 - s1_;
            s2_ = 2.0 * v2 - s2_;
            return m0_ * in + m1_ * v1 + m2_ * v2;
        }

        const double y = b0_ * in + s1_;
        s1_ = b1_ * in - a1_ * y + s2_;
        s2_ = b2_ * in - a2_ * y;
        return y;
    }

    float processSample (float x) noexcept
    {
        return (float) processSampleDouble ((double) x);
    }

    void processBlock (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

    void processBlockDouble (double* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSampleDouble (data[i]);
    }

    std::complex<float> responseAt (float freqHz, float sampleRate) const
    {
        const auto h = responseAtDouble ((double) freqHz, (double) sampleRate);
        return std::complex<float> ((float) h.real(), (float) h.imag());
    }

    static BiquadCoefficients designPeak (float freq, float q, float gainDb, float sr)
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cs = std::cos (w0);
        const double sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);

        BiquadCoefficients c;
        c.b0 = 1.0 + alpha * A;
        c.b1 = -2.0 * cs;
        c.b2 = 1.0 - alpha * A;
        c.a1 = -2.0 * cs;
        c.a2 = 1.0 - alpha / A;
        const double a0 = 1.0 + alpha / A;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0;
        c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    static BiquadCoefficients designLowShelf (float freq, float q, float gainDb, float sr)
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cs = std::cos (w0);
        const double sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;

        BiquadCoefficients c;
        c.b0 = A * ((A + 1.0) - (A - 1.0) * cs + twoSqrtAAlpha);
        c.b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs);
        c.b2 = A * ((A + 1.0) - (A - 1.0) * cs - twoSqrtAAlpha);
        const double a0 = (A + 1.0) + (A - 1.0) * cs + twoSqrtAAlpha;
        c.a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cs);
        c.a2 = (A + 1.0) + (A - 1.0) * cs - twoSqrtAAlpha;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0;
        c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    static BiquadCoefficients designHighShelf (float freq, float q, float gainDb, float sr)
    {
        const double A  = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cs = std::cos (w0);
        const double sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;

        BiquadCoefficients c;
        c.b0 = A * ((A + 1.0) + (A - 1.0) * cs + twoSqrtAAlpha);
        c.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
        c.b2 = A * ((A + 1.0) + (A - 1.0) * cs - twoSqrtAAlpha);
        const double a0 = (A + 1.0) - (A - 1.0) * cs + twoSqrtAAlpha;
        c.a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
        c.a2 = (A + 1.0) - (A - 1.0) * cs - twoSqrtAAlpha;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0;
        c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    // LowCut = 高通 (high-pass), HighCut = 低通 (low-pass)
    static BiquadCoefficients designLowCut (float freq, float q, float sr)
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cs = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        BiquadCoefficients c;
        c.b0 = (1.0 + cs) / 2.0;
        c.b1 = -(1.0 + cs);
        c.b2 = (1.0 + cs) / 2.0;
        const double a0 = 1.0 + alpha;
        c.a1 = -2.0 * cs;
        c.a2 = 1.0 - alpha;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0;
        c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    static BiquadCoefficients designHighCut (float freq, float q, float sr)
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cs = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        BiquadCoefficients c;
        c.b0 = (1.0 - cs) / 2.0;
        c.b1 = 1.0 - cs;
        c.b2 = (1.0 - cs) / 2.0;
        const double a0 = 1.0 + alpha;
        c.a1 = -2.0 * cs;
        c.a2 = 1.0 - alpha;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0;
        c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    static BiquadCoefficients designNotch (float freq, float q, float sr)
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cs = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        BiquadCoefficients c;
        c.b0 = 1.0;
        c.b1 = -2.0 * cs;
        c.b2 = 1.0;
        const double a0 = 1.0 + alpha;
        c.a1 = -2.0 * cs;
        c.a2 = 1.0 - alpha;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0;
        c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    static BiquadCoefficients designBandPass (float freq, float q, float sr)
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
        const double cs = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        BiquadCoefficients c;
        c.b0 = alpha;
        c.b1 = 0.0;
        c.b2 = -alpha;
        const double a0 = 1.0 + alpha;
        c.a1 = -2.0 * cs;
        c.a2 = 1.0 - alpha;
        c.b0 /= a0; c.b1 /= a0; c.b2 /= a0;
        c.a1 /= a0; c.a2 /= a0;
        return c;
    }

    static BiquadCoefficients designFirstOrderLowCut (float freq, float sr)
    {
        const double K = std::tan (juce::MathConstants<double>::pi * freq / sr);

        BiquadCoefficients c;
        c.b0 = 1.0 / (1.0 + K);
        c.b1 = -1.0 / (1.0 + K);
        c.b2 = 0.0;
        c.a1 = (K - 1.0) / (1.0 + K);
        c.a2 = 0.0;
        return c;
    }

    static BiquadCoefficients designFirstOrderHighCut (float freq, float sr)
    {
        const double K = std::tan (juce::MathConstants<double>::pi * freq / sr);

        BiquadCoefficients c;
        c.b0 = K / (1.0 + K);
        c.b1 = K / (1.0 + K);
        c.b2 = 0.0;
        c.a1 = (K - 1.0) / (1.0 + K);
        c.a2 = 0.0;
        return c;
    }

private:
    double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0;
    double a1_ = 0.0, a2_ = 0.0;
    double svfA1_ = 0.0, svfA2_ = 0.0, svfA3_ = 0.0;
    double m0_ = 1.0, m1_ = 0.0, m2_ = 0.0;
    double s1_ = 0.0, s2_ = 0.0;
    bool   useSvf_ = false;
};
