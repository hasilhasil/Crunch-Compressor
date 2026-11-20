#include "BandControlPanel.h"

namespace
{
    void drawGhostBadge (juce::Graphics& g,
                         juce::Rectangle<float> rect,
                         const UiPack::PaletteTokens& pal,
                         const UiPack::ThemeTokens& theme,
                         bool active,
                         bool hovered,
                         float s,
                         juce::Colour activeTint = {})
    {
        const float r = 5.5f * s;
        const juce::Colour tint = activeTint.isTransparent() ? pal.accent : activeTint;
        if (active)
        {
            if (theme.isNeumorphic)
            {
                UiPack::NeuPainter::drawInsetRect (g, rect, r, pal, theme.insetButton, s, 1.0f, false);
                g.setColour (tint.withAlpha (0.16f));
                g.fillRoundedRectangle (rect.reduced (1.2f * s), juce::jmax (2.0f, r - 1.2f * s));
            }
            else
            {
                g.setColour (tint.withAlpha (0.22f));
                g.fillRoundedRectangle (rect, r);
                g.setColour (tint.withAlpha (0.85f));
                g.drawRoundedRectangle (rect, r, 1.0f * s);
            }
        }
        else if (hovered)
        {
            g.setColour (pal.text.withAlpha (0.10f));
            g.fillRoundedRectangle (rect, r);
        }
    }
} // namespace

BandControlPanel::BandControlPanel (CrunchCompressorAudioProcessor& processor)
    : processor_ (processor)
{
    // 1. 左舱 COLOUR：染色量旋钮 (0 .. 100 %，拧起自动激活 Warm)
    colorAmountKnob_.setRange (Param::kColorAmountMin, Param::kColorAmountMax, 0.1);
    colorAmountKnob_.setDefaultValue (Param::kColorAmountDefault);
    colorAmountKnob_.setDecimals (0);
    colorAmountKnob_.setValueSuffix ("%");
    colorAmountKnob_.setShowLabel (false);
    colorAmountKnob_.setSelectionFeedbackEnabled (true);
    colorAmountKnob_.onDragStart   = [this] { beginGesture (Param::colorAmount); };
    colorAmountKnob_.onDragEnd     = [this] { endGesture   (Param::colorAmount); };
    colorAmountKnob_.onValueChange = [this] (double v)
    {
        if (! syncingKnobs_)
        {
            if (v > 0.2 && getChoiceParam (Param::colorMode) == 0)
            {
                setChoiceParam (Param::colorMode, 1); // 自动开启 Warm
                updateColorModeSlide (1, false);
            }
            setFloatParam (Param::colorAmount, (float) v);
            if (onGlobalStateChanged) onGlobalStateChanged();
        }
    };
    addAndMakeVisible (colorAmountKnob_);

    // 2. 中舱 DYNAMICS：THRESHOLD（启用同轨双层实时检测器电平叠加 + 悬停/拖拽画布引导线）
    thresholdKnob_.setRange (Param::kThresholdMin, Param::kThresholdMax, 0.1);
    thresholdKnob_.setDefaultValue (Param::kThresholdDefault);
    thresholdKnob_.setDecimals (1);
    thresholdKnob_.setValueSuffix ("dB");
    thresholdKnob_.setAccentLabel (true);
    thresholdKnob_.setSelectionFeedbackEnabled (true);
    thresholdKnob_.setTrackMeterEnabled (true);
    thresholdKnob_.onHoverOrDragChanged = [this] (bool active)
    {
        if (onThresholdFocusChanged)
            onThresholdFocusChanged (active);
    };
    thresholdKnob_.onDragStart   = [this] { beginGesture (Param::threshold); };
    thresholdKnob_.onDragEnd     = [this] { endGesture   (Param::threshold); };
    thresholdKnob_.onValueChange = [this] (double v)
    {
        if (! syncingKnobs_)
        {
            setFloatParam (Param::threshold, (float) v);
            if (onGlobalStateChanged) onGlobalStateChanged();
        }
    };
    addAndMakeVisible (thresholdKnob_);

    // 3. 中舱 DYNAMICS：RATIO (1.00 .. 20.00 : 1)
    ratioKnob_.setRange (Param::kRatioMin, Param::kRatioMax, 0.1);
    ratioKnob_.setDefaultValue (Param::kRatioDefault);
    ratioKnob_.setDecimals (1);
    ratioKnob_.setValueSuffix (": 1");
    ratioKnob_.setAccentLabel (true);
    ratioKnob_.setSelectionFeedbackEnabled (true);
    ratioKnob_.onDragStart   = [this] { beginGesture (Param::ratio); };
    ratioKnob_.onDragEnd     = [this] { endGesture   (Param::ratio); };
    ratioKnob_.onValueChange = [this] (double v)
    {
        if (! syncingKnobs_)
        {
            setFloatParam (Param::ratio, (float) v);
            if (onGlobalStateChanged) onGlobalStateChanged();
        }
    };
    addAndMakeVisible (ratioKnob_);

    // 4. 中舱 DYNAMICS：ATTACK (0.1 .. 200 ms, skew 10 ms)
    attackKnob_.setRange (Param::kAttackMin, Param::kAttackMax, 0.1);
    attackKnob_.setSkew (10.0);
    attackKnob_.setDefaultValue (Param::kAttackDefault);
    attackKnob_.setDecimals (1);
    attackKnob_.setAdaptiveMsDecimals (true);
    attackKnob_.setValueSuffix ("ms");
    attackKnob_.setAccentLabel (true);
    attackKnob_.setSelectionFeedbackEnabled (true);
    attackKnob_.onDragStart   = [this] { beginGesture (Param::attack); };
    attackKnob_.onDragEnd     = [this] { endGesture   (Param::attack); };
    attackKnob_.onValueChange = [this] (double v)
    {
        if (! syncingKnobs_)
        {
            setFloatParam (Param::attack, (float) v);
            if (onGlobalStateChanged) onGlobalStateChanged();
        }
    };
    addAndMakeVisible (attackKnob_);

    // 5. 中舱 DYNAMICS：RELEASE (10 .. 1000 ms, skew 100 ms)
    releaseKnob_.setRange (Param::kReleaseMin, Param::kReleaseMax, 0.5);
    releaseKnob_.setSkew (100.0);
    releaseKnob_.setDefaultValue (Param::kReleaseDefault);
    releaseKnob_.setDecimals (0);
    releaseKnob_.setValueSuffix ("ms");
    releaseKnob_.setAccentLabel (true);
    releaseKnob_.setSelectionFeedbackEnabled (true);
    releaseKnob_.onDragStart   = [this] { beginGesture (Param::release); };
    releaseKnob_.onDragEnd     = [this] { endGesture   (Param::release); };
    releaseKnob_.onValueChange = [this] (double v)
    {
        if (! syncingKnobs_)
        {
            setFloatParam (Param::release, (float) v);
            if (onGlobalStateChanged) onGlobalStateChanged();
        }
    };
    addAndMakeVisible (releaseKnob_);

    // 6. 右舱 I/O·MIX：INPUT / MIX / OUTPUT
    inKnob_.setRange (Param::kIoGainMin, Param::kIoGainMax, 0.1);
    inKnob_.setDefaultValue (0.0);
    inKnob_.setBipolar (true);
    inKnob_.setBipolarOrigin (0.0);
    inKnob_.setDecimals (1);
    inKnob_.setValueSuffix ("dB");
    inKnob_.setSelectionFeedbackEnabled (true);
    inKnob_.onDragStart   = [this] { beginGesture (Param::inputGain); };
    inKnob_.onDragEnd     = [this] { endGesture   (Param::inputGain); };
    inKnob_.onValueChange = [this] (double v)
    {
        if (! syncingKnobs_)
            setFloatParam (Param::inputGain, (float) v);
    };
    addAndMakeVisible (inKnob_);

    mixKnob_.setRange (Param::kMixMin, Param::kMixMax, 0.1);
    mixKnob_.setDefaultValue (Param::kMixDefault);
    mixKnob_.setDecimals (0);
    mixKnob_.setValueSuffix ("%");
    mixKnob_.setSelectionFeedbackEnabled (true);
    mixKnob_.onDragStart   = [this] { beginGesture (Param::mix); };
    mixKnob_.onDragEnd     = [this] { endGesture   (Param::mix); };
    mixKnob_.onValueChange = [this] (double v)
    {
        if (! syncingKnobs_)
            setFloatParam (Param::mix, (float) v);
    };
    addAndMakeVisible (mixKnob_);

    outKnob_.setRange (Param::kIoGainMin, Param::kIoGainMax, 0.1);
    outKnob_.setDefaultValue (0.0);
    outKnob_.setBipolar (true);
    outKnob_.setBipolarOrigin (0.0);
    outKnob_.setDecimals (1);
    outKnob_.setValueSuffix ("dB");
    outKnob_.setSelectionFeedbackEnabled (true);
    outKnob_.onDragStart   = [this] { beginGesture (Param::outputGain); };
    outKnob_.onDragEnd     = [this] { endGesture   (Param::outputGain); };
    outKnob_.onValueChange = [this] (double v)
    {
        if (! syncingKnobs_)
            setFloatParam (Param::outputGain, (float) v);
    };
    addAndMakeVisible (outKnob_);

    for (auto* param : processor_.getParameters())
    {
        if (auto* idp = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor_.apvts.addParameterListener (idp->paramID, this);
    }

    applyUiSnapshot (snap_);
    syncFromProcessor (true);
}

BandControlPanel::~BandControlPanel()
{
    for (auto* param : processor_.getParameters())
    {
        if (auto* idp = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor_.apvts.removeParameterListener (idp->paramID, this);
    }
}

EditorLayout BandControlPanel::makeLayout() const
{
    if (auto* parent = getParentComponent())
        return EditorLayout (parent->getLocalBounds(), snap_.layout);
    return EditorLayout (juce::Rectangle<int> (0, 0, juce::jmax (920, getWidth()), 780), snap_.layout);
}

EditorLayout::BottomSlots BandControlPanel::slots() const
{
    return makeLayout().bottomSlots ((float) getWidth(), (float) getHeight());
}

void BandControlPanel::applyUiSnapshot (const UiPack::UiSnapshot& snap)
{
    snap_ = snap;
    const auto& pal   = snap_.palette;
    const auto& theme = snap_.theme;

    Knob* allKnobs[8] = {
        &colorAmountKnob_, &thresholdKnob_, &ratioKnob_, &attackKnob_,
        &releaseKnob_, &inKnob_, &mixKnob_, &outKnob_
    };
    const char* keys[8] = {
        "knob.amount", "knob.threshold", "knob.ratio", "knob.attack",
        "knob.release", "knob.inputGain", "knob.mix", "knob.outputGain"
    };

    for (int i = 0; i < 8; ++i)
    {
        allKnobs[i]->setPalette (pal);
        allKnobs[i]->setThemeTokens (theme);
        allKnobs[i]->setLabel (snap_.t (keys[i]));
    }

    globalLabelLocale_ = -1;
    deckBackdropRev_   = 0;
    syncFromProcessor (false);
    layoutKnobs();
    repaint();
}

float BandControlPanel::getFloatParam (const juce::String& id) const
{
    if (auto* v = processor_.apvts.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

int BandControlPanel::getChoiceParam (const juce::String& id) const
{
    if (auto* v = processor_.apvts.getRawParameterValue (id))
        return juce::roundToInt (v->load());
    return 0;
}

void BandControlPanel::setFloatParam (const juce::String& id, float value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor_.apvts.getParameter (id)))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void BandControlPanel::setChoiceParam (const juce::String& id, int index)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor_.apvts.getParameter (id)))
    {
        const int numChoices = p->choices.size();
        const int clamped    = juce::jlimit (0, juce::jmax (0, numChoices - 1), index);
        const float norm     = (numChoices > 1) ? ((float) clamped / (float) (numChoices - 1)) : 0.0f;
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

void BandControlPanel::beginGesture (const juce::String& id)
{
    if (auto* p = processor_.apvts.getParameter (id))
        p->beginChangeGesture();
}

void BandControlPanel::endGesture (const juce::String& id)
{
    if (auto* p = processor_.apvts.getParameter (id))
        p->endChangeGesture();
}

void BandControlPanel::syncFromProcessor (bool snapSlides)
{
    juce::ScopedValueSetter<bool> svs (syncingKnobs_, true);

    const int curPos  = juce::jlimit (0, 1, getChoiceParam (Param::colorPosition));
    const int curMode = juce::jlimit (0, 3, getChoiceParam (Param::colorMode));
    const int curKnee = juce::jlimit (0, 1, getChoiceParam (Param::knee));
    const int curDet  = juce::jlimit (0, 1, getChoiceParam (Param::detector));

    updateColorPosSlide  (curPos,  snapSlides);
    updateColorModeSlide (curMode, snapSlides);
    updateKneeSlide      (curKnee, snapSlides);
    updateDetectorSlide  (curDet,  snapSlides);

    colorAmountKnob_.setValue (curMode == 0 ? 0.0 : (double) getFloatParam (Param::colorAmount), false);
    thresholdKnob_.setValue   (getFloatParam (Param::threshold),  false);
    ratioKnob_.setValue       (getFloatParam (Param::ratio),      false);
    attackKnob_.setValue      (getFloatParam (Param::attack),     false);
    releaseKnob_.setValue     (getFloatParam (Param::release),    false);
    inKnob_.setValue          (getFloatParam (Param::inputGain),  false);
    mixKnob_.setValue         (getFloatParam (Param::mix),        false);
    outKnob_.setValue         (getFloatParam (Param::outputGain), false);
}

void BandControlPanel::parameterChanged (const juce::String&, float)
{
    paramDirty_.store (true, std::memory_order_relaxed);
}

void BandControlPanel::updateColorPosSlide (int newIdx, bool snap)
{
    const int idx = juce::jlimit (0, 1, newIdx);
    if (snap || idx == colorPosSlideToIdx_)
    {
        colorPosSlideFromIdx_ = colorPosSlideToIdx_ = idx;
        colorPosSlideTween_.snapTo (1.0f);
        return;
    }
    colorPosSlideFromIdx_ = colorPosSlideToIdx_;
    colorPosSlideToIdx_   = idx;
    colorPosSlideTween_.start ((double) juce::Time::getMillisecondCounter(), 1.0f, Anim::kSegSlideMs, 0.0f);
    segMoving_ = true;
}

void BandControlPanel::updateColorModeSlide (int newMode, bool snap)
{
    const int idx = juce::jlimit (-1, 2, newMode - 1);
    if (snap || idx == colorModeSlideToIdx_)
    {
        colorModeSlideFromIdx_ = colorModeSlideToIdx_ = idx;
        colorModeSlideTween_.snapTo (1.0f);
        return;
    }
    colorModeSlideFromIdx_ = (colorModeSlideToIdx_ >= 0) ? colorModeSlideToIdx_ : idx;
    colorModeSlideToIdx_   = idx;
    colorModeSlideTween_.start ((double) juce::Time::getMillisecondCounter(), 1.0f, Anim::kSegSlideMs, 0.0f);
    segMoving_ = true;
}

void BandControlPanel::updateKneeSlide (int newIdx, bool snap)
{
    const int idx = juce::jlimit (0, 1, newIdx);
    if (snap || idx == kneeSlideToIdx_)
    {
        kneeSlideFromIdx_ = kneeSlideToIdx_ = idx;
        kneeSlideTween_.snapTo (1.0f);
        return;
    }
    kneeSlideFromIdx_ = kneeSlideToIdx_;
    kneeSlideToIdx_   = idx;
    kneeSlideTween_.start ((double) juce::Time::getMillisecondCounter(), 1.0f, Anim::kSegSlideMs, 0.0f);
    segMoving_ = true;
}

void BandControlPanel::updateDetectorSlide (int newIdx, bool snap)
{
    const int idx = juce::jlimit (0, 1, newIdx);
    if (snap || idx == detSlideToIdx_)
    {
        detSlideFromIdx_ = detSlideToIdx_ = idx;
        detSlideTween_.snapTo (1.0f);
        return;
    }
    detSlideFromIdx_ = detSlideToIdx_;
    detSlideToIdx_   = idx;
    detSlideTween_.start ((double) juce::Time::getMillisecondCounter(), 1.0f, Anim::kSegSlideMs, 0.0f);
    segMoving_ = true;
}

void BandControlPanel::tickKnobs (double nowMs)
{
    bool needPanelRepaint = false;
    if (paramDirty_.exchange (false, std::memory_order_relaxed))
    {
        syncFromProcessor (false);
        needPanelRepaint = true;
    }

    bool busy = false;
    auto stepSegTween = [&] (Tween& tw, int& fromIdx, int toIdx)
    {
        if (! tw.done (nowMs))
        {
            tw.valueBezier (nowMs, 0.22f, 1.0f, 0.36f, 1.0f);
            busy             = true;
            needPanelRepaint = true;
        }
        else if (fromIdx != toIdx)
        {
            fromIdx          = toIdx;
            tw.snapTo (1.0f);
            needPanelRepaint = true;
        }
    };

    stepSegTween (colorPosSlideTween_,  colorPosSlideFromIdx_,  colorPosSlideToIdx_);
    stepSegTween (colorModeSlideTween_, colorModeSlideFromIdx_, colorModeSlideToIdx_);
    stepSegTween (kneeSlideTween_,      kneeSlideFromIdx_,      kneeSlideToIdx_);
    stepSegTween (detSlideTween_,       detSlideFromIdx_,       detSlideToIdx_);
    segMoving_ = busy;

    if (needPanelRepaint)
        repaint();

    // 同步 THRESHOLD 旋钮同轨双层实时输入电平弧
    if (! processor_.isPluginBypassed())
        thresholdKnob_.setTrackMeterValue ((double) processor_.getDetectorLevelDb());
    else
        thresholdKnob_.setTrackMeterValue (-180.0);

    Knob* allKnobs[8] = {
        &colorAmountKnob_, &thresholdKnob_, &ratioKnob_, &attackKnob_,
        &releaseKnob_, &inKnob_, &mixKnob_, &outKnob_
    };
    for (auto* kb : allKnobs)
    {
        if (kb->isVisible())
        {
            kb->advanceStates (nowMs);
            kb->setEditCursorBlink ((juce::uint32) nowMs);
        }
    }
}

bool BandControlPanel::isKnobAnimating() const noexcept
{
    return segMoving_
        || colorAmountKnob_.isAnimating()
        || thresholdKnob_.isAnimating()
        || ratioKnob_.isAnimating()
        || attackKnob_.isAnimating()
        || releaseKnob_.isAnimating()
        || inKnob_.isAnimating()
        || mixKnob_.isAnimating()
        || outKnob_.isAnimating();
}

// ---------------- 几何槽位计算 ----------------

BandControlPanel::StageGeometry BandControlPanel::computeStageGeometry (const juce::Rectangle<float>& cd, float s) const noexcept
{
    StageGeometry sg;
    const float leftW    = 136.0f * s;
    const float stackGap = 20.0f  * s;
    const float maxAvail = juce::jmax (260.0f * s, cd.getWidth() - 24.0f * s);
    sg.heroCellW         = juce::jlimit (74.0f * s, 108.0f * s, (maxAvail - leftW - stackGap) * 0.25f);
    sg.heroDiam          = juce::jmin   (58.0f * s, sg.heroCellW - 18.0f * s);

    const float heroW  = sg.heroCellW * 4.0f;
    const float totalW = leftW + stackGap + heroW;
    const float startX = juce::jmax (cd.getX() + 12.0f * s, cd.getCentreX() - totalW * 0.5f);
    const float topY   = cd.getY() + 18.0f * s;
    const float bodyH  = cd.getBottom() - topY - 6.0f * s;

    sg.leftStack   = { startX,                             topY, leftW, bodyH };
    sg.heroCluster = { sg.leftStack.getRight() + stackGap, topY, heroW, bodyH };
    return sg;
}

juce::Rectangle<float> BandControlPanel::colorPosTrayRect (const juce::Rectangle<float>& deck, float s) const
{
    const float w = 68.0f * s;
    return { deck.getRight() - w - 10.0f * s, deck.getY() + 8.0f * s, w, 20.0f * s };
}

juce::Rectangle<float> BandControlPanel::colorPosCellRect (const juce::Rectangle<float>& deck, float s, int idx) const
{
    const auto  tray = colorPosTrayRect (deck, s).reduced (1.5f * s);
    const float hw   = tray.getWidth() * 0.5f;
    return { tray.getX() + (float) idx * hw, tray.getY(), hw, tray.getHeight() };
}

juce::Rectangle<float> BandControlPanel::colorHqBtnRect (const juce::Rectangle<float>& deck, float s) const
{
    return { deck.getRight() - 36.0f * s, deck.getY() + 33.0f * s, 26.0f * s, 18.0f * s };
}

juce::Rectangle<float> BandControlPanel::colorModeTrayRect (const juce::Rectangle<float>& deck, float s) const
{
    return { deck.getX() + 10.0f * s, deck.getBottom() - 28.0f * s, deck.getWidth() - 20.0f * s, 21.0f * s };
}

juce::Rectangle<float> BandControlPanel::colorModeCellRect (const juce::Rectangle<float>& deck, float s, int idx) const
{
    const auto  tray = colorModeTrayRect (deck, s).reduced (1.5f * s);
    const float cw   = tray.getWidth() / 3.0f;
    return { tray.getX() + (float) idx * cw + 0.5f * s, tray.getY(), cw - 1.0f * s, tray.getHeight() };
}

juce::Rectangle<float> BandControlPanel::kneeTrayRect (const juce::Rectangle<float>& ls, float s) const
{
    const float cy = ls.getCentreY();
    return { ls.getX(), cy - 32.0f * s, ls.getWidth(), 26.0f * s };
}

juce::Rectangle<float> BandControlPanel::kneeCellRect (const juce::Rectangle<float>& ls, float s, int idx) const
{
    const auto  tray = kneeTrayRect (ls, s).reduced (1.8f * s);
    const float hw   = tray.getWidth() * 0.5f;
    return { tray.getX() + (float) idx * hw, tray.getY(), hw, tray.getHeight() };
}

juce::Rectangle<float> BandControlPanel::detectorTrayRect (const juce::Rectangle<float>& ls, float s) const
{
    const float cy = ls.getCentreY();
    return { ls.getX(), cy + 6.0f * s, ls.getWidth(), 26.0f * s };
}

juce::Rectangle<float> BandControlPanel::detectorCellRect (const juce::Rectangle<float>& ls, float s, int idx) const
{
    const auto  tray = detectorTrayRect (ls, s).reduced (1.8f * s);
    const float hw   = tray.getWidth() * 0.5f;
    return { tray.getX() + (float) idx * hw, tray.getY(), hw, tray.getHeight() };
}

juce::Rectangle<float> BandControlPanel::autoGainBtnRect (const juce::Rectangle<float>& deck, float s) const
{
    const float cellW  = juce::jmin (78.0f * s, (deck.getWidth() - 16.0f * s) / 3.0f);
    const float trioW  = cellW * 3.0f;
    const float startX = deck.getCentreX() - trioW * 0.5f;
    const float outCx  = startX + cellW * 2.5f;
    const float w      = 44.0f * s;
    const float h      = 19.0f * s;
    const float y      = deck.getBottom() - 25.0f * s;
    return { outCx - w * 0.5f, y, w, h };
}

void BandControlPanel::layoutKnobs()
{
    const auto  lay = makeLayout();
    const float s   = lay.scale;
    const auto  bs  = slots();

    // 1. 左舱 COLOUR：AMOUNT 旋钮居中（严格避开右侧 [HQ] 点击热区）
    {
        const auto& cd       = bs.colourDeck;
        const auto  hqRect   = colorHqBtnRect (cd, s);
        const float topY     = cd.getY() + 28.0f * s;
        const float kh       = cd.getBottom() - 28.0f * s - topY;
        const float diam     = juce::jmin (50.0f * s, kh * 0.68f);
        const float maxHalfW = juce::jmax (24.0f * s, hqRect.getX() - 3.0f * s - cd.getCentreX());
        const float knobBoxW = juce::jmin (diam + 18.0f * s, maxHalfW * 2.0f);
        colorAmountKnob_.setKnobDiameter (diam);
        colorAmountKnob_.setUiTextSizes (11.0f * s, 12.0f * s);
        colorAmountKnob_.setBounds (juce::Rectangle<float> (cd.getCentreX() - knobBoxW * 0.5f,
                                                            topY, knobBoxW, kh).toNearestInt());
    }

    // 2. 中舱 DYNAMICS：四大主旋钮（THRESHOLD / RATIO / ATTACK / RELEASE）
    {
        const auto sg = computeStageGeometry (bs.compDeck, s);
        const float kh = sg.heroCluster.getHeight() - 4.0f * s;
        Knob* mainKnobs[4] = { &thresholdKnob_, &ratioKnob_, &attackKnob_, &releaseKnob_ };
        for (int i = 0; i < 4; ++i)
        {
            mainKnobs[i]->setKnobDiameter (sg.heroDiam);
            mainKnobs[i]->setUiTextSizes (12.0f * s, 12.5f * s);
            mainKnobs[i]->setBounds (juce::Rectangle<float> (sg.heroCluster.getX() + (float) i * sg.heroCellW,
                                                             sg.heroCluster.getY() + 2.0f * s,
                                                             sg.heroCellW, kh).toNearestInt());
        }
    }

    // 3. 右舱 I/O·MIX：INPUT / MIX / OUTPUT 三颗辅助旋钮
    {
        const auto& id     = bs.ioDeck;
        const float cellW  = juce::jmin (78.0f * s, (id.getWidth() - 16.0f * s) / 3.0f);
        const float trioW  = cellW * 3.0f;
        const float startX = id.getCentreX() - trioW * 0.5f;
        const float topY   = id.getY() + 28.0f * s;
        const float kh     = id.getBottom() - topY - 6.0f * s;
        const float diam   = juce::jmin (45.0f * s, juce::jmin (cellW - 14.0f * s, kh * 0.53f));

        Knob* ioKnobs[3] = { &inKnob_, &mixKnob_, &outKnob_ };
        for (int i = 0; i < 3; ++i)
        {
            ioKnobs[i]->setKnobDiameter (diam);
            ioKnobs[i]->setUiTextSizes (11.0f * s, 11.8f * s);
            const float cellH = (i == 2) ? (kh - 10.0f * s) : kh;
            ioKnobs[i]->setBounds (juce::Rectangle<float> (startX + (float) i * cellW,
                                                           topY, cellW, cellH).toNearestInt());
        }
    }
}

void BandControlPanel::resized()
{
    layoutKnobs();
}

// ---------------- 绘制 ----------------

void BandControlPanel::paint (juce::Graphics& g)
{
    const auto  lay        = makeLayout();
    const float s          = lay.scale;
    const auto& pal        = snap_.palette;
    const auto& theme      = snap_.theme;
    const auto  bs         = slots();
    const float W          = (float) getWidth();
    const float H          = (float) getHeight();
    const int   curW       = getWidth(), curH = getHeight();
    const auto  clipBounds = g.getClipBounds();
    const double nowMs     = (double) juce::Time::getMillisecondCounter();

    // 静态底座单图缓存（三张舱体卡片 + 标题 + 四条内凹胶囊槽）
    if (curW > 0 && curH > 0)
    {
        if (! deckBackdropCache_.isValid() || deckBackdropW_ != curW || deckBackdropH_ != curH
            || deckBackdropRev_ != snap_.revision)
        {
            const int allocW = (curW + 63) & ~63;
            const int allocH = (curH + 63) & ~63;
            if (! deckBackdropCache_.isValid()
                || deckBackdropCache_.getWidth()  < allocW
                || deckBackdropCache_.getHeight() < allocH)
            {
                deckBackdropCache_ = juce::Image (juce::Image::RGB, allocW, allocH, false);
            }

            juce::Graphics bg (deckBackdropCache_);
            bg.reduceClipRegion (0, 0, curW, curH);
            bg.fillAll (pal.bg);

            auto drawDeckCard = [&] (const juce::Rectangle<float>& rect)
            {
                const float r = (snap_.isNeumorphic() ? theme.radiusLg : 8.0f) * s;
                if (snap_.isNeumorphic())
                    UiPack::NeuPainter::drawRaisedRect (bg, rect, r, pal, theme.shadowButton, s, 1.0f, false);
                else
                {
                    bg.setColour (pal.panel);
                    bg.fillRoundedRectangle (rect, r);
                    bg.setColour (pal.gridZ.withAlpha (0.85f));
                    bg.drawRoundedRectangle (rect, r, 1.0f * s);
                }
            };

            drawDeckCard (bs.colourDeck);
            drawDeckCard (bs.compDeck);
            drawDeckCard (bs.ioDeck);

            bg.setColour (pal.dim);
            bg.setFont (CompTheme::font (CompTheme::bold(), 11.5f * s));
            bg.drawText (snap_.t ("color.section"),
                         juce::Rectangle<float> (bs.colourDeck.getX() + 10.0f * s, bs.colourDeck.getY() + 8.0f * s,
                                                 84.0f * s, 20.0f * s),
                         juce::Justification::centredLeft, false);
            bg.drawText (snap_.t ("comp.section"),
                         juce::Rectangle<float> (bs.compDeck.getX() + 12.0f * s, bs.compDeck.getY() + 8.0f * s,
                                                 120.0f * s, 20.0f * s),
                         juce::Justification::centredLeft, false);
            bg.drawText (snap_.t ("io.section"),
                         juce::Rectangle<float> (bs.ioDeck.getX() + 10.0f * s, bs.ioDeck.getY() + 8.0f * s,
                                                 bs.ioDeck.getWidth() - 20.0f * s, 20.0f * s),
                         juce::Justification::centredLeft, false);

            const auto sg       = computeStageGeometry (bs.compDeck, s);
            const auto posTray  = colorPosTrayRect  (bs.colourDeck, s);
            const auto modeTray = colorModeTrayRect (bs.colourDeck, s);
            const auto knTray   = kneeTrayRect      (sg.leftStack,  s);
            const auto dtTray   = detectorTrayRect  (sg.leftStack,  s);

            if (snap_.isNeumorphic())
            {
                UiPack::NeuPainter::drawInsetRect (bg, posTray,  5.0f * s, pal, theme.insetButton, s, 1.0f, true);
                UiPack::NeuPainter::drawInsetRect (bg, modeTray, 5.5f * s, pal, theme.insetButton, s, 1.0f, true);
                UiPack::NeuPainter::drawInsetRect (bg, knTray,   6.0f * s, pal, theme.insetButton, s, 1.0f, true);
                UiPack::NeuPainter::drawInsetRect (bg, dtTray,   6.0f * s, pal, theme.insetButton, s, 1.0f, true);
            }
            else
            {
                bg.setColour (pal.track);
                bg.fillRoundedRectangle (posTray,  4.5f * s);
                bg.fillRoundedRectangle (modeTray, 5.0f * s);
                bg.fillRoundedRectangle (knTray,   5.5f * s);
                bg.fillRoundedRectangle (dtTray,   5.5f * s);
            }

            // 中舱左控制栈与四大主旋钮之间的柔和垂直分隔线
            const float divX = sg.leftStack.getRight() + (sg.heroCluster.getX() - sg.leftStack.getRight()) * 0.5f;
            bg.setColour (pal.grid.withMultipliedAlpha (0.85f));
            bg.drawLine (divX, sg.leftStack.getY() + 6.0f * s, divX, sg.leftStack.getBottom() - 6.0f * s, 1.0f * s);

            deckBackdropW_   = curW;
            deckBackdropH_   = curH;
            deckBackdropRev_ = snap_.revision;
        }

        g.drawImage (deckBackdropCache_,
                     clipBounds.getX(), clipBounds.getY(), clipBounds.getWidth(), clipBounds.getHeight(),
                     clipBounds.getX(), clipBounds.getY(), clipBounds.getWidth(), clipBounds.getHeight());
    }
    else
    {
        g.fillAll (pal.bg);
    }

    // 1. 左舱 COLOUR：Pre/Post 滑块 + Warm/Cold/Clip 滑块 + [HQ] 4x 抗混叠微徽章
    {
        const auto& cd     = bs.colourDeck;
        const int   curPos = juce::jlimit (0, 1, getChoiceParam (Param::colorPosition));

        {
            const float uPos  = colorPosSlideTween_.valueBezier (nowMs, 0.22f, 1.0f, 0.36f, 1.0f);
            const auto  pFrom = colorPosCellRect (cd, s, juce::jlimit (0, 1, colorPosSlideFromIdx_));
            const auto  pTo   = colorPosCellRect (cd, s, juce::jlimit (0, 1, colorPosSlideToIdx_));
            const juce::Rectangle<float> posCap (
                pFrom.getX()      + (pTo.getX()      - pFrom.getX())      * uPos,
                pFrom.getY()      + (pTo.getY()      - pFrom.getY())      * uPos,
                pFrom.getWidth()  + (pTo.getWidth()  - pFrom.getWidth())  * uPos,
                pFrom.getHeight() + (pTo.getHeight() - pFrom.getHeight()) * uPos);
            UiPack::NeuPainter::drawCompactCap (g, posCap, pal, theme, true, false, s, 1.0f, 3.5f * s, false);
        }

        const juce::String posLabels[2] = { snap_.t ("color.pre"), snap_.t ("color.post") };
        for (int i = 0; i < 2; ++i)
        {
            const auto cell = colorPosCellRect (cd, s, i);
            const bool on   = (i == curPos);
            const bool hov  = (lastHoveredId_ == 10 + i) && ! on;
            if (hov)
                UiPack::NeuPainter::drawCompactCap (g, cell, pal, theme, false, true, s, 1.0f, 3.5f * s, false);
            g.setColour (on ? pal.accent : (hov ? pal.text : pal.dim));
            g.setFont (CompTheme::font (on ? CompTheme::semiBold() : CompTheme::regular(), 10.0f * s));
            g.drawText (posLabels[i], cell, juce::Justification::centred, false);
        }

        const int curMode = juce::jlimit (0, 3, getChoiceParam (Param::colorMode));
        {
            const float uMode = colorModeSlideTween_.valueBezier (nowMs, 0.22f, 1.0f, 0.36f, 1.0f);
            if (colorModeSlideToIdx_ >= 0 && colorModeSlideFromIdx_ >= 0)
            {
                const auto mFrom = colorModeCellRect (cd, s, juce::jlimit (0, 2, colorModeSlideFromIdx_));
                const auto mTo   = colorModeCellRect (cd, s, juce::jlimit (0, 2, colorModeSlideToIdx_));
                const juce::Rectangle<float> modeCap (
                    mFrom.getX()      + (mTo.getX()      - mFrom.getX())      * uMode,
                    mFrom.getY()      + (mTo.getY()      - mFrom.getY())      * uMode,
                    mFrom.getWidth()  + (mTo.getWidth()  - mFrom.getWidth())  * uMode,
                    mFrom.getHeight() + (mTo.getHeight() - mFrom.getHeight()) * uMode);
                UiPack::NeuPainter::drawCompactCap (g, modeCap, pal, theme, true, false, s, 1.0f, 4.0f * s, false);
            }
            else if (colorModeSlideToIdx_ < 0 && colorModeSlideFromIdx_ >= 0 && uMode < 0.99f)
            {
                const auto mFrom = colorModeCellRect (cd, s, juce::jlimit (0, 2, colorModeSlideFromIdx_));
                UiPack::NeuPainter::drawCompactCap (g, mFrom, pal, theme, true, false, s, 1.0f - uMode, 4.0f * s, false);
            }
        }

        const juce::String modeLabels[3] = {
            snap_.t ("color.warm"), snap_.t ("color.cold"), snap_.t ("color.clip")
        };
        for (int i = 0; i < 3; ++i)
        {
            const auto cell = colorModeCellRect (cd, s, i);
            const bool on   = (curMode == i + 1);
            const bool hov  = (lastHoveredId_ == 20 + i) && ! on;
            if (hov)
                UiPack::NeuPainter::drawCompactCap (g, cell, pal, theme, false, true, s, 1.0f, 4.0f * s, false);
            g.setColour (on ? pal.accent : (hov ? pal.text : pal.dim));
            g.setFont (CompTheme::font (on ? CompTheme::semiBold() : CompTheme::regular(), 10.5f * s));
            g.drawText (modeLabels[i], cell, juce::Justification::centred, false);
        }

        // [HQ] 母带 4x 线性相位抗混叠微开关
        {
            const auto  hqRect = colorHqBtnRect (cd, s);
            const bool  hqOn   = processor_.isColorHqEnabled();
            const bool  hqHov  = (lastHoveredId_ == 15);
            const float r      = 4.8f * s;

            if (hqOn)
            {
                g.setColour (pal.accent.withAlpha (0.10f));
                g.drawRoundedRectangle (hqRect.expanded (2.6f * s), r + 2.4f * s, 2.2f * s);
                g.setColour (pal.accent.withAlpha (0.22f));
                g.drawRoundedRectangle (hqRect.expanded (1.1f * s), r + 1.0f * s, 1.5f * s);
                g.setColour (pal.accent.withAlpha (0.72f));
                g.drawRoundedRectangle (hqRect, r, 1.0f * s);

                g.setFont (CompTheme::latinFont (CompTheme::bold(), 9.5f * s));
                g.setColour (pal.accent.withAlpha (0.24f));
                for (float dx : { -0.7f * s, 0.7f * s })
                    for (float dy : { -0.7f * s, 0.7f * s })
                        g.drawText ("HQ", hqRect.translated (dx, dy), juce::Justification::centred, false);

                g.setColour (pal.accent);
                g.drawText ("HQ", hqRect, juce::Justification::centred, false);
            }
            else
            {
                if (hqHov)
                {
                    g.setColour (pal.text.withAlpha (0.14f));
                    g.drawRoundedRectangle (hqRect, r, 0.9f * s);
                }
                g.setFont (CompTheme::latinFont (CompTheme::bold(), 9.5f * s));
                g.setColour (hqHov ? pal.text : pal.dim);
                g.drawText ("HQ", hqRect, juce::Justification::centred, false);
            }
        }
    }

    // 2. 中舱 DYNAMICS 左控制栈：[Hard | Soft] 与 [Peak | RMS] 双滑动胶囊
    {
        const auto sg      = computeStageGeometry (bs.compDeck, s);
        const auto& ls     = sg.leftStack;
        const int  curKnee = juce::jlimit (0, 1, getChoiceParam (Param::knee));
        const int  curDet  = juce::jlimit (0, 1, getChoiceParam (Param::detector));

        // Knee: [Hard | Soft]
        {
            const float uKn   = kneeSlideTween_.valueBezier (nowMs, 0.22f, 1.0f, 0.36f, 1.0f);
            const auto  kFrom = kneeCellRect (ls, s, juce::jlimit (0, 1, kneeSlideFromIdx_));
            const auto  kTo   = kneeCellRect (ls, s, juce::jlimit (0, 1, kneeSlideToIdx_));
            const juce::Rectangle<float> kCap (
                kFrom.getX()      + (kTo.getX()      - kFrom.getX())      * uKn,
                kFrom.getY()      + (kTo.getY()      - kFrom.getY())      * uKn,
                kFrom.getWidth()  + (kTo.getWidth()  - kFrom.getWidth())  * uKn,
                kFrom.getHeight() + (kTo.getHeight() - kFrom.getHeight()) * uKn);
            UiPack::NeuPainter::drawCompactCap (g, kCap, pal, theme, true, false, s, 1.0f, 4.2f * s, false);

            const juce::String kLabels[2] = { snap_.t ("comp.hard"), snap_.t ("comp.soft") };
            for (int i = 0; i < 2; ++i)
            {
                const auto cell = kneeCellRect (ls, s, i);
                const bool on   = (i == curKnee);
                const bool hov  = (lastHoveredId_ == 30 + i) && ! on;
                if (hov)
                    UiPack::NeuPainter::drawCompactCap (g, cell, pal, theme, false, true, s, 1.0f, 4.2f * s, false);
                g.setColour (on ? pal.accent : (hov ? pal.text : pal.dim));
                g.setFont (CompTheme::font (on ? CompTheme::semiBold() : CompTheme::regular(), 11.2f * s));
                g.drawText (kLabels[i], cell, juce::Justification::centred, false);
            }
        }

        // Detector: [Peak | RMS]
        {
            const float uDt   = detSlideTween_.valueBezier (nowMs, 0.22f, 1.0f, 0.36f, 1.0f);
            const auto  dFrom = detectorCellRect (ls, s, juce::jlimit (0, 1, detSlideFromIdx_));
            const auto  dTo   = detectorCellRect (ls, s, juce::jlimit (0, 1, detSlideToIdx_));
            const juce::Rectangle<float> dCap (
                dFrom.getX()      + (dTo.getX()      - dFrom.getX())      * uDt,
                dFrom.getY()      + (dTo.getY()      - dFrom.getY())      * uDt,
                dFrom.getWidth()  + (dTo.getWidth()  - dFrom.getWidth())  * uDt,
                dFrom.getHeight() + (dTo.getHeight() - dFrom.getHeight()) * uDt);
            UiPack::NeuPainter::drawCompactCap (g, dCap, pal, theme, true, false, s, 1.0f, 4.2f * s, false);

            const juce::String dLabels[2] = { snap_.t ("comp.peak"), snap_.t ("comp.rms") };
            for (int i = 0; i < 2; ++i)
            {
                const auto cell = detectorCellRect (ls, s, i);
                const bool on   = (i == curDet);
                const bool hov  = (lastHoveredId_ == 32 + i) && ! on;
                if (hov)
                    UiPack::NeuPainter::drawCompactCap (g, cell, pal, theme, false, true, s, 1.0f, 4.2f * s, false);
                g.setColour (on ? pal.accent : (hov ? pal.text : pal.dim));
                g.setFont (CompTheme::font (on ? CompTheme::semiBold() : CompTheme::regular(), 11.2f * s));
                g.drawText (dLabels[i], cell, juce::Justification::centred, false);
            }
        }
    }

    // 3. 右舱 I/O·MIX：OUTPUT 旋钮读数正下方的 [AUTO] 自动增益幽灵微徽章
    {
        const auto agRect = autoGainBtnRect (bs.ioDeck, s);
        const bool agOn   = (getChoiceParam (Param::autoGain) != 0);
        const bool agHov  = (lastHoveredId_ == 40);

        drawGhostBadge (g, agRect, pal, theme, agOn, agHov, s, pal.accent);
        g.setColour (agOn ? pal.accent : (agHov ? pal.text : pal.dim));
        g.setFont (CompTheme::latinFont (CompTheme::bold(), 9.5f * s));
        g.drawText ("AUTO", agRect, juce::Justification::centred, false);
    }

    // 4. 最下方全局参数条（Global Bar 3 格减法终态：LOOKAHEAD 下拉 / CURVE 单击直切 / BYPASS）
    {
        const auto& gb = bs.globalBar;
        g.setColour (pal.surfaceA);
        g.fillRect (gb);
        if (! snap_.isNeumorphic())
        {
            g.setColour (pal.gridZ);
            g.fillRect (0.0f, std::floor (gb.getY()) + 0.5f, W, 1.0f);
        }

        const juce::String labels[EditorLayout::kNumGlobalCells] = {
            snap_.t ("global.lookahead"),
            snap_.t ("global.kneeCurve"),
            snap_.t ("global.bypass")
        };

        const auto labelFont = CompTheme::font (CompTheme::uiMedium(), 12.5f * s);
        const auto latinValF = CompTheme::latinFont (CompTheme::uiRegular(), 12.5f * s);
        const auto transValF = CompTheme::font (CompTheme::uiRegular(), 12.5f * s);

        if (std::abs (globalLabelScale_ - s) > 1.0e-4f || globalLabelLocale_ != (int) snap_.localeId)
        {
            for (int i = 0; i < EditorLayout::kNumGlobalCells; ++i)
                globalLabelWidths_[(size_t) i] = juce::GlyphArrangement::getStringWidth (labelFont, labels[i]);
            globalLabelScale_  = s;
            globalLabelLocale_ = (int) snap_.localeId;
        }

        const int  laIdx       = juce::jlimit (0, 2, getChoiceParam (Param::lookahead));
        const char* laMs[2]    = { "1 ms", "3 ms" };
        const bool curveVis    = processor_.isKneeCurveVisible();

        const juce::String values[2] = {
            (laIdx == 0) ? snap_.t ("global.laOff") : juce::String (laMs[laIdx - 1]),
            curveVis ? snap_.t ("global.on") : snap_.t ("global.off")
        };
        const bool useTransVal[2] = { (laIdx == 0), true };
        const bool hasChevron[2]  = { true, false };

        for (int i = 0; i < 2; ++i)
        {
            const auto r       = lay.globalCell (W, H, i);
            const bool hovered = (lastHoveredId_ == 50 + i);

            if (snap_.isNeumorphic())
            {
                UiPack::NeuPainter::drawButton (g, r, pal, theme, false, hovered, s, 1.0f, 6.0f * s);
            }
            else if (hovered)
            {
                g.setColour (pal.accent.withAlpha (0.08f));
                g.fillRoundedRectangle (r, 4.0f * s);
            }

            auto text      = r.withSizeKeepingCentre (r.getWidth() - (snap_.isNeumorphic() ? 16.0f * s : 10.0f * s), 20.0f * s);
            const float lw = globalLabelWidths_[(size_t) i];
            g.setFont (labelFont);
            g.setColour (pal.dim);
            g.drawText (labels[i], text.withWidth (lw + 2.0f), juce::Justification::centredLeft, false);

            auto vr = text.withTrimmedLeft (lw + 8.0f * s).withTrimmedRight (hasChevron[i] ? 12.0f * s : 4.0f * s);
            g.setFont (useTransVal[i] ? transValF : latinValF);
            g.setColour ((i == 1 && ! curveVis) ? pal.dim : pal.text);
            g.drawText (values[i], vr, juce::Justification::centredLeft, false);

            if (hasChevron[i])
            {
                const float ax = r.getRight() - (snap_.isNeumorphic() ? 9.0f * s : 6.0f * s);
                const float ay = r.getCentreY();
                g.setColour (pal.dim);
                g.drawLine (ax - 2.5f * s, ay - 1.2f * s, ax, ay + 1.2f * s, 1.1f * s);
                g.drawLine (ax, ay + 1.2f * s, ax + 2.5f * s, ay - 1.2f * s, 1.1f * s);
            }
        }

        // Cell 2: 全局 BYPASS 按钮
        {
            const auto r      = lay.globalCell (W, H, 2);
            const bool active = processor_.isPluginBypassed();
            const bool hov    = (lastHoveredId_ == 52);
            UiPack::NeuPainter::drawButton (g, r, pal, theme, active, hov, s, 1.0f, 6.0f * s);
            g.setColour (active ? pal.accent : pal.text);
            g.setFont (CompTheme::font (CompTheme::uiMedium(), 12.5f * s));
            g.drawText (labels[2], r, juce::Justification::centred, false);
        }
    }
}

// ---------------- 悬停与点击交互 ----------------

int BandControlPanel::hitTestHoverId (juce::Point<float> p) const
{
    const auto  lay = makeLayout();
    const float s   = lay.scale;
    const auto  bs  = slots();

    if (colorHqBtnRect (bs.colourDeck, s).contains (p))
        return 15;
    for (int i = 0; i < 2; ++i)
        if (colorPosCellRect (bs.colourDeck, s, i).contains (p))
            return 10 + i;
    for (int i = 0; i < 3; ++i)
        if (colorModeCellRect (bs.colourDeck, s, i).contains (p))
            return 20 + i;

    const auto sg = computeStageGeometry (bs.compDeck, s);
    for (int i = 0; i < 2; ++i)
        if (kneeCellRect (sg.leftStack, s, i).contains (p))
            return 30 + i;
    for (int i = 0; i < 2; ++i)
        if (detectorCellRect (sg.leftStack, s, i).contains (p))
            return 32 + i;

    if (autoGainBtnRect (bs.ioDeck, s).contains (p))
        return 40;

    const float W = (float) getWidth(), H = (float) getHeight();
    for (int i = 0; i < EditorLayout::kNumGlobalCells; ++i)
        if (lay.globalCell (W, H, i).contains (p))
            return 50 + i;

    return -1;
}

void BandControlPanel::mouseMove (const juce::MouseEvent& e)
{
    const int hov = hitTestHoverId (e.position);
    if (hov != lastHoveredId_)
    {
        lastHoveredId_ = hov;
        repaint();
    }
}

void BandControlPanel::mouseExit (const juce::MouseEvent&)
{
    if (lastHoveredId_ != -1)
    {
        lastHoveredId_ = -1;
        repaint();
    }
}

void BandControlPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto  p   = e.position;
    const auto  lay = makeLayout();
    const float s   = lay.scale;
    const auto  bs  = slots();

    // 1. COLOUR 舱：[HQ] + [Pre|Post] + [Warm|Cold|Clip]
    if (colorHqBtnRect (bs.colourDeck, s).contains (p))
    {
        processor_.setColorHqEnabled (! processor_.isColorHqEnabled());
        if (onGlobalStateChanged) onGlobalStateChanged();
        repaint();
        return;
    }
    for (int i = 0; i < 2; ++i)
    {
        if (colorPosCellRect (bs.colourDeck, s, i).contains (p))
        {
            setChoiceParam (Param::colorPosition, i);
            updateColorPosSlide (i, false);
            if (onGlobalStateChanged) onGlobalStateChanged();
            repaint();
            return;
        }
    }
    for (int i = 0; i < 3; ++i)
    {
        if (colorModeCellRect (bs.colourDeck, s, i).contains (p))
        {
            const int curMode  = getChoiceParam (Param::colorMode);
            const int nextMode = (curMode == i + 1) ? 0 : (i + 1);
            setChoiceParam (Param::colorMode, nextMode);
            if (nextMode > 0 && getFloatParam (Param::colorAmount) < 0.5f)
                setFloatParam (Param::colorAmount, 20.0f);
            updateColorModeSlide (nextMode, false);
            syncFromProcessor (false);
            if (onGlobalStateChanged) onGlobalStateChanged();
            repaint();
            return;
        }
    }

    // 2. DYNAMICS 舱左栈：[Hard|Soft] + [Peak|RMS]
    const auto sg = computeStageGeometry (bs.compDeck, s);
    for (int i = 0; i < 2; ++i)
    {
        if (kneeCellRect (sg.leftStack, s, i).contains (p))
        {
            setChoiceParam (Param::knee, i);
            updateKneeSlide (i, false);
            if (onGlobalStateChanged) onGlobalStateChanged();
            repaint();
            return;
        }
    }
    for (int i = 0; i < 2; ++i)
    {
        if (detectorCellRect (sg.leftStack, s, i).contains (p))
        {
            setChoiceParam (Param::detector, i);
            updateDetectorSlide (i, false);
            if (onGlobalStateChanged) onGlobalStateChanged();
            repaint();
            return;
        }
    }

    // 3. I/O·MIX 舱：[AUTO] 自动增益开关
    if (autoGainBtnRect (bs.ioDeck, s).contains (p))
    {
        const int cur = getChoiceParam (Param::autoGain);
        setChoiceParam (Param::autoGain, cur == 0 ? 1 : 0);
        if (onGlobalStateChanged) onGlobalStateChanged();
        repaint();
        return;
    }

    // 4. 底部 Global Bar 3 槽位（LOOKAHEAD 下拉 / CURVE 单击直切 / BYPASS）
    const float W = (float) getWidth(), H = (float) getHeight();
    for (int i = 0; i < EditorLayout::kNumGlobalCells; ++i)
    {
        const auto cell = lay.globalCell (W, H, i);
        if (! cell.contains (p))
            continue;

        if (i == 0 && onRequestPopup) // LOOKAHEAD
        {
            const juce::StringArray items { snap_.t ("global.laOff"), "1 ms", "3 ms" };
            const int cur = juce::jlimit (0, 2, getChoiceParam (Param::lookahead));
            onRequestPopup ("global.lookahead", cell, items, cur, snap_.isCjkLocale(),
                            [this] (int pick)
                            {
                                setChoiceParam (Param::lookahead, pick);
                                if (onGlobalStateChanged) onGlobalStateChanged();
                                repaint();
                            });
            return;
        }
        if (i == 1) // CURVE：单击直接切换显隐（减法精修：省去二元下拉菜单的二次点击）
        {
            processor_.setKneeCurveVisible (! processor_.isKneeCurveVisible());
            if (onGlobalStateChanged) onGlobalStateChanged();
            repaint();
            return;
        }
        if (i == 2) // BYPASS
        {
            processor_.setPluginBypassed (! processor_.isPluginBypassed());
            if (onGlobalStateChanged) onGlobalStateChanged();
            repaint();
            return;
        }
    }
}
