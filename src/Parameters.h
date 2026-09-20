#pragma once

#include <JuceHeader.h>

namespace Param
{
    constexpr float kIoGainMin   = -36.0f;
    constexpr float kIoGainMax   = 36.0f;

    constexpr float kThresholdMin     = -60.0f;
    constexpr float kThresholdMax     = 0.0f;
    constexpr float kThresholdDefault = -20.0f;

    constexpr float kRatioMin     = 1.0f;
    constexpr float kRatioMax     = 20.0f;
    constexpr float kRatioDefault = 4.0f;

    constexpr float kAttackMin     = 0.1f;
    constexpr float kAttackMax     = 200.0f;
    constexpr float kAttackDefault = 10.0f;

    constexpr float kReleaseMin     = 10.0f;
    constexpr float kReleaseMax     = 1000.0f;
    constexpr float kReleaseDefault = 100.0f;

    constexpr float kColorAmountMin     = 0.0f;
    constexpr float kColorAmountMax     = 100.0f;
    constexpr float kColorAmountDefault = 0.0f;

    // Dry/wet mix of the final output: 0% = fully dry (input untouched),
    // 100% = fully wet (processed chain only). Default 100% keeps the
    // plugin's behaviour identical to before the mix control existed.
    constexpr float kMixMin     = 0.0f;
    constexpr float kMixMax     = 100.0f;
    constexpr float kMixDefault = 100.0f;

    inline const juce::String enabled       = "enabled";
    inline const juce::String inputGain     = "inputGain";
    inline const juce::String outputGain    = "outputGain";
    inline const juce::String threshold     = "threshold";
    inline const juce::String ratio         = "ratio";
    inline const juce::String attack        = "attack";
    inline const juce::String release       = "release";
    inline const juce::String knee          = "knee";
    inline const juce::String detector      = "detector";
    inline const juce::String lookahead     = "lookahead";
    inline const juce::String autoGain      = "autoGain";
    inline const juce::String colorMode     = "colorMode";
    inline const juce::String colorAmount   = "colorAmount";
    inline const juce::String colorPosition = "colorPosition";
    inline const juce::String mix           = "mix";

    juce::StringArray getKneeNames();
    juce::StringArray getDetectorNames();
    juce::StringArray getLookaheadNames();
    juce::StringArray getAutoGainNames();
    juce::StringArray getColorModeNames();
    juce::StringArray getColorPositionNames();

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    int   kneeIndexToDb (int index);
    float lookaheadIndexToMs (int index);
    bool  detectorIsRms (int index);
}
