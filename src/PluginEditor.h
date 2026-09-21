#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ui/DisplayView.h"
#include "ui/BandControlPanel.h"
#include "ui/LookAndFeel.h"

class CrunchCompressorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             public juce::AudioProcessorValueTreeState::Listener,
                                             public juce::Timer
{
public:
    explicit CrunchCompressorAudioProcessorEditor (CrunchCompressorAudioProcessor&);
    ~CrunchCompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void moved() override;
    void timerCallback() override;
    void parentHierarchyChanged() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    // JUCE 8 gives HWND peers the Direct2D backend by default. Direct2D makes
    // every frame depend on the driver/GPU (and re-uploads software images to
    // the GPU on each drawImageAt, uncached), which is what made the display
    // drop frames intermittently - especially in hosts with a busy message
    // loop like FL Studio. The software renderer blits directly, so the frame
    // cost becomes predictable CPU work.
    void forceSoftwareRenderer();

    // Marks the start of a user drag/resize loop and arms the timer that ends
    // it (see moved() / timerCallback()).
    void noteWindowMove();

    // Timestamp of the last window move/resize event, used to detect when the
    // user's drag loop has finished.
    juce::uint32 lastMoveMs_ = 0;

    CrunchCompressorAudioProcessor& processor_;

    CrunchLookAndFeel lookAndFeel_;
    DisplayView display_;
    BandControlPanel panel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrunchCompressorAudioProcessorEditor)
};
