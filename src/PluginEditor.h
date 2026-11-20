#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ui/DisplayView.h"
#include "ui/BandControlPanel.h"
#include "ui/LookAndFeel.h"
#include "ui/PackPopupOverlay.h"

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

    DisplayView&      getDisplayView() noexcept { return display_; }
    BandControlPanel& getControlPanel() noexcept { return panel_; }

private:
    void layoutComponents();
    void applySnapshotToAll();
    void forceSoftwareRenderer();
    void noteWindowMove();

    juce::uint32 lastMoveMs_ = 0;

    CrunchCompressorAudioProcessor& processor_;

    CrunchLookAndFeel lookAndFeel_;
    DisplayView       display_;
    BandControlPanel  panel_;
    PackPopupOverlay  popupOverlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrunchCompressorAudioProcessorEditor)
};
