#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ui/DisplayView.h"
#include "ui/BandControlPanel.h"
#include "ui/LookAndFeel.h"

class CrunchCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit CrunchCompressorAudioProcessorEditor (CrunchCompressorAudioProcessor&);
    ~CrunchCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    CrunchCompressorAudioProcessor& processor_;

    CrunchLookAndFeel lookAndFeel_;
    DisplayView display_;
    BandControlPanel panel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrunchCompressorAudioProcessorEditor)
};
