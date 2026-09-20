#pragma once

#include <JuceHeader.h>

#include <complex>

#include "../Parameters.h"

struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;   // a0 is normalised to 1.0
};

class BiquadFilter
{
public:
    void setCoefficients (const BiquadCoefficients& c)
    {
        b0_ = c.b0; b1_ = c.b1; b2_ = c.b2;
        a1_ = c.a1; a2_ = c.a2;
    }

    void reset()
    {
        z1_ = 0.0;
        z2_ = 0.0;
    }

    float processSample (float x) noexcept
    {
        const double in  = (double) x;
        const double y   = b0_ * in + z1_;
        z1_ = b1_ * in - a1_ * y + z2_;
        z2_ = b2_ * in - a2_ * y;
        return (float) y;
    }

    void processBlock (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
            data[i] = processSample (data[i]);
    }

    std::complex<float> responseAt (float freqHz, float sampleRate) const
    {
        const double w = juce::MathConstants<double>::twoPi * freqHz / sampleRate;
        const std::complex<double> z1 (std::cos (w), -std::sin (w));
        const std::complex<double> z2 (std::cos (2.0 * w), -std::sin (2.0 * w));

        const std::complex<double> num (b0_ + b1_ * z1.real() + b2_ * z2.real(),
                                        b1_ * z1.imag() + b2_ * z2.imag());
        const std::complex<double> den (1.0 + a1_ * z1.real() + a2_ * z2.real(),
                                        a1_ * z1.imag() + a2_ * z2.imag());

        const auto h = num / den;
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
    double z1_ = 0.0, z2_ = 0.0;
};
