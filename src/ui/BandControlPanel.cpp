#include "BandControlPanel.h"

BandControlPanel::BandControlPanel (CrunchCompressorAudioProcessor& processor)
    : processor_ (processor)
{
    setupKnob (thresholdSlider_, thresholdLabel_, "Threshold", " dB");
    setupKnob (ratioSlider_,     ratioLabel_,     "Ratio",     " : 1");
    setupKnob (attackSlider_,    attackLabel_,    "Attack",    " ms");
    setupKnob (releaseSlider_,   releaseLabel_,   "Release",   " ms");
    setupKnob (inSlider_,        inLabel_,        "In",        " dB");
    setupKnob (mixSlider_,       mixLabel_,       "Mix",       " %");
    setupKnob (outSlider_,       outLabel_,       "Out",       " dB");
    setupKnob (colorAmountSlider_, colorAmountLabel_, "Amount", " %");

    setupCombo (kneeBox_,       kneeLabel_,       "KNEE");
    setupCombo (detectorBox_,   detectorLabel_,   "DETECTOR");
    setupCombo (lookaheadBox_,  lookaheadLabel_,  "LOOKAHEAD");

    kneeBox_.addItemList (Param::getKneeNames(), 1);
    detectorBox_.addItemList (Param::getDetectorNames(), 1);
    lookaheadBox_.addItemList (Param::getLookaheadNames(), 1);

    // STYLE module: covered by the "STYLE" module label above; its two
    // selectors get no individual captions.
    addAndMakeVisible (colorModeBox_);
    colorModeBox_.setJustificationType (juce::Justification::centredLeft);
    colorModeBox_.addItemList (Param::getColorModeNames(), 1);

    addAndMakeVisible (colorPositionBox_);
    colorPositionBox_.setJustificationType (juce::Justification::centredLeft);
    colorPositionBox_.addItemList (Param::getColorPositionNames(), 1);

    mixSlider_.setTooltip ("Mix: dry/wet balance of the final output (0% = dry input, 100% = fully processed)");

    addAndMakeVisible (autoGainButton_);
    autoGainButton_.setClickingTogglesState (true);
    autoGainButton_.setTooltip ("Auto Gain: keep the output level aligned with the dry input");

    addAndMakeVisible (bypassButton_);
    bypassButton_.setButtonText ("Bypass");
    bypassButton_.setTooltip ("Bypass: compressor fully transparent (no colour, no gain)");
    // Bypass is the INVERSE of the "enabled" parameter: the pill shows the
    // theme's off colour while the compressor runs and switches to the accent
    // colour once bypassed. A plain ButtonAttachment would light up while the
    // compressor is enabled, which is the opposite of what the label says.
    bypassButton_.setClickingTogglesState (false);
    bypassButton_.onClick = [this]
    {
        if (auto* p = processor_.apvts.getParameter (Param::enabled))
        {
            const bool nowEnabled = p->getValue() < 0.5f;
            p->setValueNotifyingHost (nowEnabled ? 1.0f : 0.0f);
        }
    };
    {
        if (auto* p = processor_.apvts.getParameter (Param::enabled))
            bypassButton_.setToggleState (p->getValue() < 0.5f, juce::dontSendNotification);
    }
    processor_.apvts.addParameterListener (Param::enabled, this);

    addAndMakeVisible (colorSectionLabel_);
    colorSectionLabel_.setText ("STYLE", juce::dontSendNotification);
    colorSectionLabel_.setJustificationType (juce::Justification::centredLeft);

    auto& apvts = processor_.apvts;

    thresholdAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::threshold, thresholdSlider_);
    ratioAtt_     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::ratio,     ratioSlider_);
    attackAtt_    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::attack,    attackSlider_);
    releaseAtt_   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::release,   releaseSlider_);

    kneeAtt_     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::knee,      kneeBox_);
    detectorAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::detector,  detectorBox_);
    autoGainAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, Param::autoGain, autoGainButton_);

    inAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::inputGain,  inSlider_);
    mixAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::mix,         mixSlider_);
    outAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::outputGain, outSlider_);

    lookaheadAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::lookahead,  lookaheadBox_);

    colorModeAtt_   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::colorMode,     colorModeBox_);
    colorAmountAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>   (apvts, Param::colorAmount,   colorAmountSlider_);
    colorPositionAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::colorPosition, colorPositionBox_);

    applyThemeColours();
}

BandControlPanel::~BandControlPanel()
{
    processor_.apvts.removeParameterListener (Param::enabled, this);
}

void BandControlPanel::setupKnob (juce::Slider& slider, juce::Label& label,
                                  const juce::String& title, const juce::String& suffix)
{
    addAndMakeVisible (slider);
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextValueSuffix (suffix);

    addAndMakeVisible (label);
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
}

void BandControlPanel::setupCombo (juce::ComboBox& box, juce::Label& label,
                                   const juce::String& title)
{
    addAndMakeVisible (box);
    box.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (label);
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
}

// Label fonts/colours are theme-dependent (accent for the main knobs and the
// STYLE module, plain text everywhere else). They are re-applied whenever the
// look and feel changes so a theme switch recolours them instantly.
void BandControlPanel::applyThemeColours()
{
    const bool light  = processor_.isLightTheme();
    const auto accent = CrunchLookAndFeel::accentForTheme (processor_.getTheme());
    const auto text   = CrunchPalette::text (light);
    const auto dim    = CrunchPalette::textDim (light);

    const auto styleMain = [accent] (juce::Label& l)
    {
        l.setFont (CrunchLookAndFeel::uiFont (16.5f, 600));
        l.setColour (juce::Label::textColourId, accent);
    };

    const auto styleSmall = [text] (juce::Label& l)
    {
        l.setFont (CrunchLookAndFeel::uiFont (14.5f, 600));
        l.setColour (juce::Label::textColourId, text);
    };

    const auto styleCaption = [dim] (juce::Label& l)
    {
        l.setFont (CrunchLookAndFeel::uiFont (12.5f, 600));
        l.setColour (juce::Label::textColourId, dim);
    };

    styleMain (thresholdLabel_);
    styleMain (ratioLabel_);
    styleMain (attackLabel_);
    styleMain (releaseLabel_);

    styleSmall (inLabel_);
    styleSmall (mixLabel_);
    styleSmall (outLabel_);
    styleSmall (colorAmountLabel_);

    styleCaption (kneeLabel_);
    styleCaption (detectorLabel_);
    styleCaption (lookaheadLabel_);

    colorSectionLabel_.setFont (CrunchLookAndFeel::uiFont (15.5f, 700));
    colorSectionLabel_.setColour (juce::Label::textColourId, accent);
}

void BandControlPanel::lookAndFeelChanged()
{
    applyThemeColours();
    repaint();
}

void BandControlPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == Param::enabled)
        bypassButton_.setToggleState (newValue < 0.5f, juce::dontSendNotification);
}

void BandControlPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    const int H = area.getHeight();
    const int W = area.getWidth();

    const auto vcentre = [] (juce::Rectangle<int> r, int h)
    {
        const int top = r.getY() + (r.getHeight() - h) / 2;
        return juce::Rectangle<int> (r.getX(), top, r.getWidth(), h);
    };

    const auto placeKnob = [] (juce::Slider& slider, juce::Label& label,
                               juce::Rectangle<int> bounds, int labelH)
    {
        auto knob = bounds;
        label.setBounds (knob.removeFromTop (labelH));
        slider.setBounds (knob);
    };

    const auto placeCaptionCombo = [&vcentre] (juce::Rectangle<int> col, int captionH, int comboH,
                                               juce::Label& label, juce::ComboBox& box)
    {
        label.setBounds (col.removeFromTop (captionH));
        box.setBounds (vcentre (col, comboH));
    };

    // --- left column: STYLE module (top) + captioned selectors --------------
    const int leftW = juce::jlimit (190, (int) (W * 0.27f), 280);
    auto left = area.removeFromLeft (leftW);
    area.removeFromLeft (10);

    colorSectionLabel_.setBounds (left.removeFromTop (18));
    {
        auto row = left.removeFromTop (30);
        const int posW = juce::jmin (78, row.getWidth() / 3);
        colorPositionBox_.setBounds (vcentre (row.removeFromRight (posW).reduced (0, 2), 26));
        colorModeBox_.setBounds (vcentre (row.reduced (0, 2), 26));
    }

    // Amount knob (label above, value below)
    {
        auto amountRow = left.removeFromTop (104);
        colorAmountLabel_.setBounds (amountRow.removeFromTop (15));
        colorAmountSlider_.setBounds (amountRow);
    }

    left.removeFromTop (8);
    {
        const int colW = left.getWidth() / 2;
        const int selH = left.getHeight() / 2;
        auto row1 = left.removeFromTop (selH);
        placeCaptionCombo (row1.removeFromLeft (colW), 13, 26, kneeLabel_,     kneeBox_);
        placeCaptionCombo (row1,                     13, 26, detectorLabel_, detectorBox_);
        placeCaptionCombo (left,                    13, 26, lookaheadLabel_, lookaheadBox_);
    }

    // --- right zone: main knob row + In/Mix/Out strip ------------------------
    // The main row is taller than the bottom strip: its knobs read as the
    // primary controls (like the Close/Mid/Far row of the reference UI).
    const int stripH = (int) ((float) H * 0.42f);
    auto mainRow = area.removeFromTop (H - stripH);
    auto stripRow = area;

    bypassButton_.setBounds (vcentre (mainRow.removeFromRight (90).reduced (2), 32));

    const int knobW = mainRow.getWidth() / 4;
    placeKnob (thresholdSlider_, thresholdLabel_, mainRow.removeFromLeft (knobW).reduced (2), 18);
    placeKnob (ratioSlider_,     ratioLabel_,     mainRow.removeFromLeft (knobW).reduced (2), 18);
    placeKnob (attackSlider_,    attackLabel_,    mainRow.removeFromLeft (knobW).reduced (2), 18);
    placeKnob (releaseSlider_,   releaseLabel_,   mainRow.reduced (2), 18);

    autoGainButton_.setBounds (vcentre (stripRow.removeFromRight (118).reduced (2), 32));

    const int knobW2 = stripRow.getWidth() / 3;
    placeKnob (inSlider_,  inLabel_,  stripRow.removeFromLeft (knobW2).reduced (2), 16);
    placeKnob (mixSlider_, mixLabel_, stripRow.removeFromLeft (knobW2).reduced (2), 16);
    placeKnob (outSlider_, outLabel_, stripRow.reduced (2), 16);
}
