#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "LookAndFeel.h"

// Plain text pill for Auto Gain: rounded track, accent fill when engaged,
// with a soft glow behind the label.
class AutoGainButton : public juce::Button
{
public:
    AutoGainButton() : juce::Button ("Auto Gain") {}

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override
    {
        const bool on = getToggleState();
        const auto accent = accentColour();

        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const float corner = juce::jmin (14.0f, bounds.getHeight() * 0.5f);
        g.setColour (on ? accent : CrunchPalette::track (isLightTheme()).withAlpha (0.9f));
        g.fillRoundedRectangle (bounds, corner);

        if (on)
        {
            rebuildGlowIfNeeded (accent);
            if (glowImage_.isValid())
                g.drawImageAt (glowImage_,
                               (getWidth()  - glowImage_.getWidth())  / 2,
                               (getHeight() - glowImage_.getHeight()) / 2);
        }

        auto textColour = on ? juce::Colour (0xfffffff8)
                             : (shouldDrawButtonAsHighlighted ? CrunchPalette::text (isLightTheme())
                                                              : CrunchPalette::textDim (isLightTheme()));
        if (on && shouldDrawButtonAsDown)
            textColour = textColour.brighter (0.3f);

        g.setFont (CrunchLookAndFeel::uiFont (14.0f, 600));
        g.setColour (textColour);
        g.drawText (kText, bounds.toNearestInt(), juce::Justification::centred, false);
    }

private:
    juce::Colour accentColour() const
    {
        if (auto* laf = dynamic_cast<CrunchLookAndFeel*> (&getLookAndFeel()))
            return laf->getAccentColour();
        return juce::Colour (0xffe07856);
    }

    bool isLightTheme() const
    {
        if (auto* laf = dynamic_cast<CrunchLookAndFeel*> (&getLookAndFeel()))
            return laf->isLightTheme();
        return true;
    }

    juce::Font textFont()
    {
        return CrunchLookAndFeel::uiFont (13.5f, 600);
    }

    void rebuildGlowIfNeeded (juce::Colour accent)
    {
        if (glowValid_ && glowColour_ == accent && glowW_ == getWidth() && glowH_ == getHeight())
            return;

        glowValid_  = true;
        glowW_      = getWidth();
        glowH_      = getHeight();
        glowColour_ = accent;
        glowImage_  = {};

        const int w = glowW_ + glowPad_ * 2;
        const int h = glowH_ + glowPad_ * 2;
        if (w <= 0 || h <= 0)
            return;

        juce::Image glow (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics gi (glow);
            gi.setFont (textFont());
            gi.setColour (accent);
            gi.drawText (kText, glowPad_, glowPad_, glowW_, glowH_,
                         juce::Justification::centred, false);
        }

        juce::ImageConvolutionKernel kernel (glowRadius_ * 2 + 1);
        kernel.createGaussianBlur ((float) glowRadius_);
        kernel.applyToImage (glow, glow, glow.getBounds());
        glowImage_ = glow;
    }

    static constexpr const char* kText = "AUTO GAIN";
    static constexpr int glowPad_    = 9;
    static constexpr int glowRadius_ = 3;

    juce::Image  glowImage_;
    juce::Colour glowColour_ {};
    int  glowW_ = -1;
    int  glowH_ = -1;
    bool glowValid_ = false;
};

// Bottom panel (240 px, two zones):
//
//   +---------------------------------------------+---------------------------+
//   | STYLE                          (label above)| Threshold  Ratio  Attack  | Release   Bypass
//   | [Color Mode][Position]                     |  (main knobs, accent labels,|
//   | Amount                                     |   larger than the strip)  |
//   | ---------                                   |                           |
//   | KNEE                                        | In        Mix        Out   | AUTO GAIN
//   | [Hard/Soft]                                 |  (bottom knob strip)       |
//   | DETECTOR                                    |                           |
//   | [Peak/RMS]                                  |                           |
//   | LOOKAHEAD                                   |                           |
//   | [Off/1 ms/3 ms]                             |                           |
//   +---------------------------------------------+---------------------------+
//
// Every selector / button carries a caption above it so its function is
// obvious. The colour (STYLE) module sits on the left, aligned with the
// selector column, with its own module label ("STYLE") above.
class BandControlPanel : public juce::Component,
                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit BandControlPanel (CrunchCompressorAudioProcessor& processor);
    ~BandControlPanel() override;

    void resized() override;
    void lookAndFeelChanged() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& title, const juce::String& suffix);
    void setupCombo (juce::ComboBox& box, juce::Label& label, const juce::String& title);
    void applyThemeColours();

    CrunchCompressorAudioProcessor& processor_;

    juce::Slider thresholdSlider_, ratioSlider_, attackSlider_, releaseSlider_;
    juce::Slider inSlider_, mixSlider_, outSlider_, colorAmountSlider_;
    juce::Label  thresholdLabel_, ratioLabel_, attackLabel_, releaseLabel_;
    juce::Label  inLabel_, mixLabel_, outLabel_, colorAmountLabel_;
    juce::ComboBox kneeBox_, detectorBox_, lookaheadBox_;
    juce::Label  kneeLabel_, detectorLabel_, lookaheadLabel_;
    juce::ComboBox colorModeBox_, colorPositionBox_;
    juce::TextButton bypassButton_;
    AutoGainButton   autoGainButton_;
    juce::Label colorSectionLabel_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   thresholdAtt_, ratioAtt_, attackAtt_, releaseAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   inAtt_, mixAtt_, outAtt_, colorAmountAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> kneeAtt_, detectorAtt_, lookaheadAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> colorModeAtt_, colorPositionAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   autoGainAtt_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandControlPanel)
};
