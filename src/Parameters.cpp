#include "Parameters.h"

namespace Param
{
    juce::StringArray getKneeNames()
    {
        return juce::StringArray { "Hard", "Soft" };
    }

    juce::StringArray getDetectorNames()
    {
        return juce::StringArray { "Peak", "RMS" };
    }

    juce::StringArray getLookaheadNames()
    {
        return juce::StringArray { "Off", "1 ms", "3 ms" };
    }

    juce::StringArray getAutoGainNames()
    {
        return juce::StringArray { "Off", "On" };
    }

    juce::StringArray getColorModeNames()
    {
        return juce::StringArray { "Off", "Warm", "Cold", "Clip" };
    }

    juce::StringArray getColorPositionNames()
    {
        return juce::StringArray { "Pre", "Post" };
    }

    int kneeIndexToDb (int index)
    {
        return index == 1 ? 12 : 0;
    }

    float lookaheadIndexToMs (int index)
    {
        switch (index)
        {
            case 1: return 1.0f;
            case 2: return 3.0f;
        }
        return 0.0f;
    }

    bool detectorIsRms (int index)
    {
        return index == 1;
    }

    static juce::NormalisableRange<float> attackRange()
    {
        juce::NormalisableRange<float> r (kAttackMin, kAttackMax, 0.1f);
        r.setSkewForCentre (10.0f);
        return r;
    }

    static juce::NormalisableRange<float> releaseRange()
    {
        juce::NormalisableRange<float> r (kReleaseMin, kReleaseMax, 0.5f);
        r.setSkewForCentre (100.0f);
        return r;
    }

    // Value readout formatting, matching the reference UI: dB values get two
    // decimals ("-0.00"), ms values drop the decimals once >= 1 ("10 ms",
    // "0.1 ms"), percentages are integers. The unit suffix is appended by the
    // slider (setTextValueSuffix).
    static juce::String dbText (float v, int)
    {
        return juce::String (v, 2);
    }

    static juce::String msText (float v, int)
    {
        return juce::String (v, v >= 1.0f ? 0 : 1);
    }

    static std::function<juce::String (float, int)> plainText (int decimals)
    {
        return [decimals] (float v, int) { return juce::String (v, decimals); };
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { enabled, 1 }, "Compressor", true));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { inputGain, 1 }, "Input Gain",
            juce::NormalisableRange<float> (kIoGainMin, kIoGainMax, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { outputGain, 1 }, "Output Gain",
            juce::NormalisableRange<float> (kIoGainMin, kIoGainMax, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { threshold, 1 }, "Threshold",
            juce::NormalisableRange<float> (kThresholdMin, kThresholdMax, 0.1f), kThresholdDefault,
            juce::AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ratio, 1 }, "Ratio",
            juce::NormalisableRange<float> (kRatioMin, kRatioMax, 0.1f), kRatioDefault,
            juce::AudioParameterFloatAttributes().withLabel (" : 1").withStringFromValueFunction (plainText (2))));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { attack, 1 }, "Attack",
            attackRange(), kAttackDefault,
            juce::AudioParameterFloatAttributes().withLabel (" ms").withStringFromValueFunction (msText)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { release, 1 }, "Release",
            releaseRange(), kReleaseDefault,
            juce::AudioParameterFloatAttributes().withLabel (" ms").withStringFromValueFunction (msText)));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { knee, 1 }, "Knee", getKneeNames(), 1));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { detector, 1 }, "Detector", getDetectorNames(), 0));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { lookahead, 1 }, "Lookahead", getLookaheadNames(), 0));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { autoGain, 1 }, "Auto Gain", getAutoGainNames(), 0));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { colorMode, 1 }, "Color Mode", getColorModeNames(), 0));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { colorAmount, 1 }, "Color Amount",
            juce::NormalisableRange<float> (kColorAmountMin, kColorAmountMax, 0.1f), kColorAmountDefault,
            juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (plainText (0))));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { colorPosition, 1 }, "Color Position", getColorPositionNames(), 1));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { mix, 1 }, "Mix",
            juce::NormalisableRange<float> (kMixMin, kMixMax, 0.1f), kMixDefault,
            juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (plainText (0))));

        return layout;
    }
}
