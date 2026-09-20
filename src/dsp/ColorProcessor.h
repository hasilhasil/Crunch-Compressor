#pragma once

#include <JuceHeader.h>

#include "BiquadFilter.h"

class ColorProcessor
{
public:
    void prepare (float sampleRate)
    {
        sampleRate_ = sampleRate;
        reset();
    }

    void reset()
    {
        preL_.reset();  preR_.reset();
        postL_.reset(); postR_.reset();
    }

    void setMode (int mode)
    {
        if (mode == mode_)
            return;

        mode_ = mode;
        updateFilters();
    }

    void setAmount (float amount01)
    {
        amount_ = juce::jlimit (0.0f, 1.0f, amount01);
    }

    void processSample (float& left, float& right) noexcept
    {
        if (mode_ == 0)
            return;

        const float drive = amount_ * 3.0f;

        if (mode_ == 3)
        {
            float l = std::tanh (drive * left);
            float r = std::tanh (drive * right);
            left  = juce::jlimit (-1.0f, 1.0f, left  * (1.0f - amount_) + l * amount_);
            right = juce::jlimit (-1.0f, 1.0f, right * (1.0f - amount_) + r * amount_);
            return;
        }

        if (amount_ <= 0.0001f)
            return;

        float l = preL_.processSample (left);
        float r = preR_.processSample (right);

        l = postL_.processSample (std::tanh (drive * l));
        r = postR_.processSample (std::tanh (drive * r));

        left  = left  * (1.0f - amount_) + l * amount_;
        right = right * (1.0f - amount_) + r * amount_;
    }

private:
    // Designs the pre/post biquads for a colour mode. Modes 0 and 3 leave the
    // outputs untouched.
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

    void updateFilters()
    {
        reset();

        if (mode_ == 1 || mode_ == 2)
        {
            BiquadCoefficients pre, post;
            designForMode (mode_, sampleRate_, pre, post);
            preL_.setCoefficients (pre);   preR_.setCoefficients (pre);
            postL_.setCoefficients (post); postR_.setCoefficients (post);
        }
    }

    BiquadFilter preL_, preR_, postL_, postR_;
    int   mode_ = 0;
    float amount_ = 0.0f;
    float sampleRate_ = 48000.0f;
};
