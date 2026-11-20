#include "DisplayView.h"

DisplayView::DisplayView (CrunchCompressorAudioProcessor& processor)
    : processor_ (processor)
{
    setWantsKeyboardFocus (true);

    histIn_.resize  ((size_t) kHistoryReadPoints, -100.0f);
    histOut_.resize ((size_t) kHistoryReadPoints, -100.0f);
    histGr_.resize  ((size_t) kHistoryReadPoints, 0.0f);

    inCurve_.preallocateSpace  (2048);
    inFill_.preallocateSpace   (2060);
    outCurve_.preallocateSpace (2048);
    outFill_.preallocateSpace  (2060);
    grPath_.preallocateSpace   (2048);
    livePath_.preallocateSpace (280);

    reloadUiPacksFromProcessor();
    lastAbSlotB_ = processor_.isAbSlotB();
    abSlideTween_.snapTo (lastAbSlotB_ ? 1.0f : 0.0f);
    threshGuideTween_.snapTo (0.0f);

    syncHistoryImmediateForSnapshot();
    startTimerHz (60);
}

DisplayView::~DisplayView()
{
    stopTimer();
}

EditorLayout DisplayView::makeLayout() const
{
    if (auto* parent = getParentComponent())
        return EditorLayout (parent->getLocalBounds(), composition_.snapshot().layout);
    return EditorLayout (juce::Rectangle<int> (0, 0, juce::jmax (920, getWidth()), 780),
                         composition_.snapshot().layout);
}

juce::Rectangle<float> DisplayView::plotWellRect() const
{
    return makeLayout().plotWellRect ((float) getHeight());
}

void DisplayView::reloadUiPacksFromProcessor()
{
    const auto& up = processor_.getUiPackSettings();
    const auto t   = (UiPack::ThemeId)   juce::jlimit (0, UiPack::kNumThemes   - 1, up.themeId);
    const auto p   = (UiPack::PaletteId) juce::jlimit (0, UiPack::kNumPalettes - 1, up.paletteId);
    const auto l   = UiPack::UiComposition::boundLayoutForTheme (t);
    const auto lc  = (UiPack::LocaleId)  juce::jlimit (0, UiPack::kNumLocales  - 1, up.localeId);

    composition_.applyPacks (t, p, l, lc);
    applyCompositionSnapshot (false);
}

void DisplayView::applyCompositionSnapshot (bool persistToProcessor)
{
    const auto& snap = composition_.snapshot();
    CompTheme::setCjkMode (snap.isCjkLocale());

    if (persistToProcessor)
    {
        CrunchCompressorAudioProcessor::UiPackSettings up;
        up.themeId   = (int) snap.themeId;
        up.paletteId = (int) snap.paletteId;
        up.layoutId  = (int) snap.layoutId;
        up.localeId  = (int) snap.localeId;
        processor_.setUiPackSettings (up, true);
    }

    staticPlotBgRev_ = 0;
    colourWashRev_   = 0;
    wellRimRev_      = 0;
    transferDirty_   = true;
    uiStatePending_  = true;

    if (onCompositionChanged)
        onCompositionChanged();
    repaint();
}

UiPack::VerificationVerdict DisplayView::setUiPacks (UiPack::ThemeId t, UiPack::PaletteId p,
                                                     UiPack::LayoutId l, UiPack::LocaleId lc)
{
    const auto v = composition_.applyPacks (t, p, l, lc);
    if (v.ok && v.reason != "unchanged")
        applyCompositionSnapshot (true);
    return v;
}

UiPack::VerificationVerdict DisplayView::setThemePack (UiPack::ThemeId t)
{
    const auto v = composition_.setThemePack (t);
    if (v.ok && v.reason != "unchanged")
        applyCompositionSnapshot (true);
    return v;
}

UiPack::VerificationVerdict DisplayView::setPalettePack (UiPack::PaletteId p)
{
    const auto v = composition_.setPalettePack (p);
    if (v.ok && v.reason != "unchanged")
        applyCompositionSnapshot (true);
    return v;
}

UiPack::VerificationVerdict DisplayView::setLayoutPack (UiPack::LayoutId l)
{
    const auto v = composition_.setLayoutPack (l);
    if (v.ok && v.reason != "unchanged")
        applyCompositionSnapshot (true);
    return v;
}

UiPack::VerificationVerdict DisplayView::setLocalePack (UiPack::LocaleId lc)
{
    const auto v = composition_.setLocalePack (lc);
    if (v.ok && v.reason != "unchanged")
        applyCompositionSnapshot (true);
    return v;
}

UiPack::VerificationVerdict DisplayView::applyNeumorphismPreset (UiPack::PaletteId pal)
{
    const auto v = composition_.applyNeumorphismPreset (pal);
    if (v.ok && v.reason != "unchanged")
        applyCompositionSnapshot (true);
    return v;
}

UiPack::VerificationVerdict DisplayView::applyClassicPreset (UiPack::PaletteId pal)
{
    const auto v = composition_.applyClassicPreset (pal);
    if (v.ok && v.reason != "unchanged")
        applyCompositionSnapshot (true);
    return v;
}

void DisplayView::rollbackUiPacks()
{
    composition_.rollback();
    applyCompositionSnapshot (true);
}

void DisplayView::parametersChanged()
{
    paramChangedFlag_.store (true, std::memory_order_relaxed);
}

void DisplayView::ensureTimerFullRate()
{
    idleFrames_ = 0;
    if (getTimerInterval() > 20)
        startTimerHz (60);
}

void DisplayView::setThresholdGuideActive (bool active)
{
    if (threshGuideTarget_ == active)
        return;
    threshGuideTarget_ = active;
    const double nowMs = (double) juce::Time::getMillisecondCounter();
    threshGuideTween_.start (nowMs, active ? 1.0f : 0.0f, Anim::kFadeMs, threshGuideTween_.value (nowMs));
    ensureTimerFullRate();
    repaint();
}

void DisplayView::syncHistoryImmediateForSnapshot()
{
    unsigned counter = 0;
    historyPoints_ = processor_.readHistory (kHistoryReadPoints,
                                             histIn_.data(), histOut_.data(), histGr_.data(),
                                             &counter);
    historyBase_   = (double) counter - (double) historyPoints_;
    scrollCursor_  = (double) counter;
    meterInDb_     = juce::jmax (-60.0f, processor_.getInputPeakDb());
    meterOutDb_    = juce::jmax (-60.0f, processor_.getOutputPeakDb());
    peakInDb_      = meterInDb_;
    peakOutDb_     = meterOutDb_;
    labelInDb_     = meterInDb_;
    labelOutDb_    = meterOutDb_;
    transferDirty_ = true;
}

void DisplayView::resized()
{
    transferDirty_   = true;
    staticPlotBgRev_ = 0;
    colourWashRev_   = 0;
    wellRimRev_      = 0;
}

// ---------------- 坐标映射 ----------------

float DisplayView::dbToY (float db, const juce::Rectangle<float>& well, float scale) const
{
    const float d      = juce::jlimit (kGraphBottomDb, kGraphTopDb, db);
    const float topPad = 12.0f * scale;
    const float top    = well.getY() + topPad;
    const float usable = juce::jmax (10.0f, well.getBottom() - top);
    return top + (kGraphTopDb - d) / (kGraphTopDb - kGraphBottomDb) * usable;
}

float DisplayView::grToY (float grDb, const juce::Rectangle<float>& well, float scale) const
{
    return dbToY (grDb, well, scale);
}

float DisplayView::dbToX (float db, const juce::Rectangle<float>& well) const
{
    const float d = juce::jlimit (kGraphBottomDb, kGraphTopDb, db);
    return well.getX() + (d - kGraphBottomDb) / (kGraphTopDb - kGraphBottomDb) * well.getWidth();
}

float DisplayView::meterY (float db, const EditorLayout& lay) const
{
    const float s      = lay.scale;
    const float top    = lay.topHeight() + 20.0f * s;
    const float bottom = (float) getHeight() - 24.0f * s;
    const float frac   = juce::jlimit (0.0f, 1.0f, (db - kMeterBottomDb) / (kMeterTopDb - kMeterBottomDb));
    return bottom - frac * juce::jmax (10.0f, bottom - top);
}

float DisplayView::historyEndOffset() const
{
    if (historyPoints_ <= 0 || scrollCursor_ < 0.0)
        return 0.0f;
    return (float) (scrollCursor_ - historyBase_);
}

void DisplayView::rebuildTransferCurveIfNeeded (const juce::Rectangle<float>& well)
{
    if (! transferDirty_ && transferWellBuilt_ == well && ! transferBasePath_.isEmpty())
        return;

    constexpr int points = 128;
    processor_.getTransferCurve (kGraphBottomDb, kGraphTopDb, points, curveInDb_, curveOutDb_);

    const float s = makeLayout().scale;
    transferBasePath_.clear();
    for (int i = 0; i < points; ++i)
    {
        const float x = dbToX (curveInDb_[(size_t) i], well);
        const float y = dbToY (curveOutDb_[(size_t) i], well, s);
        if (i == 0) transferBasePath_.startNewSubPath (x, y);
        else        transferBasePath_.lineTo (x, y);
    }

    transferDirty_     = false;
    transferWellBuilt_ = well;
}

void DisplayView::updatePeakHold (float& peakDb, juce::uint32& holdUntilMs,
                                  float levelDb, juce::uint32 nowMs, double dt)
{
    if (levelDb >= peakDb)
    {
        peakDb      = levelDb;
        holdUntilMs = nowMs + (juce::uint32) kPeakHoldMs;
    }
    else if (nowMs >= holdUntilMs)
    {
        peakDb = juce::jmax (levelDb, peakDb - (float) (kPeakFallDbPerSec * dt));
    }
}

void DisplayView::timerCallback()
{
    if (processor_.isWindowDragging())
        return;

    const double nowHiRes = juce::Time::getMillisecondCounterHiRes();
    const auto   nowMs    = juce::Time::getMillisecondCounter();

    if (onTickKnobs)
        onTickKnobs (nowHiRes);

    if (paramChangedFlag_.exchange (false, std::memory_order_relaxed))
    {
        transferDirty_  = true;
        uiStatePending_ = true;
    }

    unsigned counter = 0;
    historyPoints_ = processor_.readHistory (kHistoryReadPoints,
                                             histIn_.data(), histOut_.data(), histGr_.data(),
                                             &counter);
    const double counterNow = (double) counter;
    historyBase_ = counterNow - (double) historyPoints_;

    if (historyPoints_ >= 2)
    {
        if (scrollCursor_ < 0.0)
        {
            scrollCursor_ = counterNow;
            lastTickMs_   = 0.0;
        }
        else
        {
            double dtSec = lastTickMs_ > 0.0 ? (nowHiRes - lastTickMs_) / 1000.0 : 1.0 / 60.0;
            lastTickMs_  = nowHiRes;
            dtSec        = juce::jlimit (0.001, 0.1, dtSec);

            const double coeff = 1.0 - std::exp (-dtSec / kScrollTimeConstantSec);
            scrollCursor_ += (counterNow - scrollCursor_) * coeff;
            scrollCursor_  = juce::jlimit (counterNow - (double) (historyPoints_ - 1),
                                           counterNow,
                                           scrollCursor_);
        }
    }

    const float inDb  = processor_.getInputPeakDb();
    const float outDb = processor_.getOutputPeakDb();
    const double dtMeter = (lastMeterMs_ == 0)
                             ? 1.0 / 60.0
                             : juce::jlimit (0.001, 0.100, (double) (nowMs - lastMeterMs_) / 1000.0);
    lastMeterMs_ = nowMs;

    meterInDb_  = (inDb  > meterInDb_)  ? inDb  : juce::jmax (meterInDb_  - (float) (60.0 * dtMeter), -60.0f);
    meterOutDb_ = (outDb > meterOutDb_) ? outDb : juce::jmax (meterOutDb_ - (float) (60.0 * dtMeter), -60.0f);
    updatePeakHold (peakInDb_,  peakInHoldUntilMs_,  inDb,  nowMs, dtMeter);
    updatePeakHold (peakOutDb_, peakOutHoldUntilMs_, outDb, nowMs, dtMeter);

    const auto well = plotWellRect();
    const bool curveRebuilt = transferDirty_ || transferWellBuilt_ != well;
    if (curveRebuilt)
        rebuildTransferCurveIfNeeded (well);

    const float grNow        = processor_.getGainReductionDb();
    const bool  kneeNow      = processor_.isKneeCurveVisible();
    const bool  abBusy       = ! abSlideTween_.done ((double) nowMs);
    const bool  guideBusy    = ! threshGuideTween_.done ((double) nowMs);
    const bool  externalBusy = isExternalAnimating ? isExternalAnimating() : false;
    const bool  hadPending   = uiStatePending_;
    uiStatePending_          = false;

    const bool graphScrolling = (counter != lastPaintedCounter_)
                             || (std::abs (scrollCursor_ - lastPaintedCursor_) > 0.01);
    const bool contentChanged = curveRebuilt
                             || graphScrolling
                             || (kneeNow != lastPaintedKnee_)
                             || (std::abs (grNow - lastPaintedGr_) > 0.04f)
                             || abBusy
                             || guideBusy
                             || hadPending;

    const bool metersBusy = (meterInDb_  > -59.8f) || (meterOutDb_ > -59.8f)
                         || (peakInDb_   > -59.8f) || (peakOutDb_  > -59.8f);

    const bool idleNow = ! contentChanged && ! metersBusy && ! externalBusy;
    if (idleNow)
        ++idleFrames_;
    else
        idleFrames_ = 0;

    if (! idleNow)
    {
        if (getTimerInterval() > 20)
            startTimerHz (60);
    }
    else if (idleFrames_ >= kIdleHoldFrames)
    {
        if (getTimerInterval() < 50)
            startTimerHz (kIdleHz);
    }

    if (contentChanged)
    {
        lastPaintedCounter_ = counter;
        lastPaintedCursor_  = scrollCursor_;
        lastPaintedGr_      = grNow;
        lastPaintedKnee_    = kneeNow;
        repaint();
    }
    else if (metersBusy)
    {
        const auto lay = makeLayout();
        const int  mx  = juce::jmax (0, juce::roundToInt (lay.plotRight()) - 2);
        repaint (mx, juce::roundToInt (lay.topHeight()), getWidth() - mx, getHeight() - juce::roundToInt (lay.topHeight()));
    }
}

// ---------------- 细线辅助 ----------------

void DisplayView::drawHairlineH (juce::Graphics& g, float y, float x0, float x1, juce::Colour c) const
{
    g.setColour (c);
    g.fillRect (x0, std::floor (y), juce::jmax (0.0f, x1 - x0), 1.0f);
}

void DisplayView::drawHairlineV (juce::Graphics& g, float x, float y0, float y1, juce::Colour c) const
{
    g.setColour (c);
    g.fillRect (std::floor (x), y0, 1.0f, juce::jmax (0.0f, y1 - y0));
}

// ---------------- 绘制分块 ----------------

void DisplayView::drawBackground (juce::Graphics& g, const EditorLayout& lay)
{
    const auto& snap = composition_.snapshot();
    const auto& pal  = snap.palette;

    g.fillAll (pal.bg);

    if (snap.isNeumorphic() || snap.layout.sunkenPlotWell)
    {
        const auto well = lay.plotWellRect ((float) getHeight());
        if (g.getClipBounds().intersects (well.expanded (4.0f).toNearestInt()))
        {
            const float r = (snap.layout.plotCornerRadius > 0.0f ? snap.layout.plotCornerRadius : snap.theme.radiusLg) * lay.scale;
            const juce::Colour base = pal.bg.interpolatedWith (pal.track, 0.28f);
            g.setColour (base);
            g.fillRoundedRectangle (well, r);
        }
    }
}

// IGN 蓝噪声抖动谐波染色渐变底板（移植自 Crunch EQ）
void DisplayView::drawColourWash (juce::Graphics& g, const EditorLayout& lay)
{
    const auto& snap = composition_.snapshot();
    const auto& pal  = snap.palette;

    const int   mode   = juce::jlimit (0, 3, juce::roundToInt (
                          processor_.apvts.getRawParameterValue (Param::colorMode)->load()));
    const float amount = processor_.apvts.getRawParameterValue (Param::colorAmount)->load();
    if (mode == 0 || amount <= 1.0e-3f)
        return;

    const auto well = lay.plotWellRect ((float) getHeight());
    const int  iw   = juce::jmax (1, (int) std::ceil (well.getWidth()));
    const int  ih   = juce::jmax (1, (int) std::ceil (well.getHeight()));
    if (! g.getClipBounds().intersects (well.toNearestInt()))
        return;

    const bool cacheValid = colourWashCache_.isValid()
        && colourWashW_    == iw
        && colourWashH_    == ih
        && colourWashMode_ == mode
        && colourWashRev_  == snap.revision
        && std::abs (colourWashAmount_ - amount) < 0.08f;

    if (! cacheValid)
    {
        if (! colourWashCache_.isValid()
            || colourWashCache_.getWidth()  < iw
            || colourWashCache_.getHeight() < ih)
        {
            colourWashCache_ = juce::Image (juce::Image::ARGB,
                                            (iw + 63) & ~63, (ih + 63) & ~63, true);
        }

        const bool  isLight = pal.light;
        const float tNorm   = juce::jlimit (0.0f, 1.0f, amount * 0.01f);
        const float maxA    = isLight ? 0.36f : 0.28f;
        const float peakA   = maxA * std::pow (tNorm, 1.12f);

        juce::Colour base = pal.washClip;
        if (mode == 1)      base = pal.washWarm;
        else if (mode == 2) base = pal.washCold;

        const float bR      = (float) base.getRed();
        const float bG      = (float) base.getGreen();
        const float bB      = (float) base.getBlue();
        const float invW    = 1.0f / (float) juce::jmax (1, iw - 1);
        const float invH    = 1.0f / (float) juce::jmax (1, ih - 1);
        const float invSpan = 1.0f / (0.55f + 0.65f * tNorm);

        juce::Image::BitmapData bd (colourWashCache_, 0, 0, iw, ih, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < ih; ++y)
        {
            const float ny  = (float) y * invH;
            auto*       row = reinterpret_cast<juce::PixelARGB*> (bd.getLinePointer (y));
            for (int x = 0; x < iw; ++x)
            {
                const float nx = (float) x * invW;
                float u = 0.0f;
                if (mode == 1)
                    u = (0.58f * nx + 0.42f * ny) * invSpan;          // Warm: 左上 -> 右下
                else if (mode == 2)
                    u = (0.58f * (1.0f - nx) + 0.42f * ny) * invSpan; // Cold: 右上 -> 左下
                else
                    u = (1.0f - ny) * invSpan;                        // Clip: 自下 -> 向上

                if (u >= 1.0f)
                {
                    row[x].setARGB (0, 0, 0, 0);
                    continue;
                }

                const float oneMinusU = 1.0f - juce::jmax (0.0f, u);
                const float falloff   = oneMinusU * oneMinusU * (0.72f + 0.28f * oneMinusU);
                const float aFloat    = peakA * falloff * 255.0f;

                // Jorge Jimenez Interleaved Gradient Noise (IGN)
                const float ignProd = 0.06711056f * (float) x + 0.00583715f * (float) y;
                const float ignFrac = ignProd - std::floor (ignProd);
                const float ignVal  = 52.9829189f * ignFrac;
                const float dither  = (ignVal - std::floor (ignVal)) - 0.5f;

                const int a8 = juce::jlimit (0, 255, (int) std::lround (aFloat + dither));
                if (a8 <= 0)
                {
                    row[x].setARGB (0, 0, 0, 0);
                    continue;
                }

                const float sc = (float) a8 * (1.0f / 255.0f);
                const auto  r8 = (juce::uint8) juce::jlimit (0, 255, (int) std::lround (bR * sc));
                const auto  g8 = (juce::uint8) juce::jlimit (0, 255, (int) std::lround (bG * sc));
                const auto  b8 = (juce::uint8) juce::jlimit (0, 255, (int) std::lround (bB * sc));
                row[x].setARGB ((juce::uint8) a8, r8, g8, b8);
            }
        }

        colourWashMode_   = mode;
        colourWashAmount_ = amount;
        colourWashRev_    = snap.revision;
        colourWashW_      = iw;
        colourWashH_      = ih;
    }

    g.saveState();
    if (snap.layout.sunkenPlotWell)
    {
        juce::Path clipP;
        clipP.addRoundedRectangle (well, snap.layout.plotCornerRadius * lay.scale);
        g.reduceClipRegion (clipP);
    }
    else
    {
        g.reduceClipRegion (well.toNearestInt());
    }
    g.drawImage (colourWashCache_,
                 (int) well.getX(), (int) well.getY(), iw, ih,
                 0, 0, iw, ih);
    g.restoreState();
}

void DisplayView::drawGrid (juce::Graphics& g, const EditorLayout& lay)
{
    const auto& pal  = composition_.snapshot().palette;
    const auto  well = lay.plotWellRect ((float) getHeight());
    const float s    = lay.scale;

    // 纵向 10 等分时间参考线（每格 ≈ 0.5 秒）
    for (int i = 1; i < 10; ++i)
    {
        const float vx = well.getX() + well.getWidth() * ((float) i * 0.1f);
        drawHairlineV (g, vx, well.getY(), well.getBottom(), pal.grid);
    }

    // 横向每 6 dB 一条参考线（0 dB 加重）
    for (float db = kGraphBottomDb; db <= kGraphTopDb + 0.01f; db += kAxisStepDb)
    {
        const bool isZero = std::abs (db) < 0.01f;
        drawHairlineH (g, dbToY (db, well, s), well.getX(), well.getRight(),
                       isZero ? pal.gridZ : pal.grid);
    }
}

void DisplayView::drawHistoryGraph (juce::Graphics& g, const EditorLayout& lay)
{
    const int n = historyPoints_;
    if (n < 2)
        return;

    const auto& pal       = composition_.snapshot().palette;
    const float s         = lay.scale;
    const auto  well      = lay.plotWellRect ((float) getHeight());
    const float endOffset = historyEndOffset();
    const float bottomY   = well.getBottom();
    const float silentY   = bottomY + 3.0f * s;

    auto mapWaveY = [&] (float db) -> float
    {
        return (db <= kGraphBottomDb - 0.5f) ? silentY : dbToY (db, well, s);
    };

    // 第 1 层（底层）：输入包络剪影（单次遍历同时构建描边与填充，省去 50% 重复计算）
    inCurve_.clear();
    float inFirstX = well.getX(), inLastX = well.getRight();
    if (appendDecimated (inCurve_, histIn_.data(), n, endOffset, well, false, inFirstX, inLastX, mapWaveY))
    {
        inFill_ = inCurve_;
        inFill_.lineTo (inLastX,  silentY);
        inFill_.lineTo (inFirstX, silentY);
        inFill_.closeSubPath();

        juce::ColourGradient greyGrad (pal.inFill.withAlpha (pal.light ? 0.36f : 0.42f), 0.0f, well.getY(),
                                       pal.inFill.withAlpha (0.06f),                     0.0f, bottomY, false);
        g.setGradientFill (greyGrad);
        g.fillPath (inFill_);

        g.setColour (pal.inFill.withAlpha (0.62f));
        g.strokePath (inCurve_, juce::PathStrokeType (1.1f * s));
    }

    // 第 2 层（上层）：输出包络主体（主题色渐变填充 + 高对比度 2px 轮廓线）
    outCurve_.clear();
    float outFirstX = well.getX(), outLastX = well.getRight();
    if (appendDecimated (outCurve_, histOut_.data(), n, endOffset, well, false, outFirstX, outLastX, mapWaveY))
    {
        outFill_ = outCurve_;
        outFill_.lineTo (outLastX,  silentY);
        outFill_.lineTo (outFirstX, silentY);
        outFill_.closeSubPath();

        juce::ColourGradient accentGrad (pal.accent.withAlpha (pal.light ? 0.44f : 0.62f), 0.0f, well.getY(),
                                         pal.accent.withAlpha (0.08f),                     0.0f, bottomY, false);
        g.setGradientFill (accentGrad);
        g.fillPath (outFill_);

        g.setColour (pal.waveOutline.withAlpha (0.95f));
        g.strokePath (outCurve_, juce::PathStrokeType (1.9f * s,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }
}

void DisplayView::drawGrCurve (juce::Graphics& g, const EditorLayout& lay)
{
    const int n = historyPoints_;
    if (n < 2)
        return;

    const auto& pal  = composition_.snapshot().palette;
    const float s    = lay.scale;
    const auto  well = lay.plotWellRect ((float) getHeight());

    grPath_.clear();
    float grFirstX = well.getX(), grLastX = well.getRight();
    if (appendDecimated (grPath_, histGr_.data(), n, historyEndOffset(), well, true, grFirstX, grLastX,
                         [&] (float db) { return grToY (db, well, s); }))
    {
        g.setColour (pal.grTrace);
        g.strokePath (grPath_, juce::PathStrokeType (1.85f * s,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }
}

void DisplayView::drawTransferCurve (juce::Graphics& g, const EditorLayout& lay)
{
    const auto& pal  = composition_.snapshot().palette;
    const float s    = lay.scale;
    const auto  well = lay.plotWellRect ((float) getHeight());

    rebuildTransferCurveIfNeeded (well);
    const int points = (int) curveInDb_.size();
    if (points < 2 || transferBasePath_.isEmpty())
        return;

    // 底层：静态传递函数曲线
    g.setColour (pal.text.withAlpha (pal.light ? 0.34f : 0.48f));
    g.strokePath (transferBasePath_, juce::PathStrokeType (2.0f * s,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

    if (historyPoints_ <= 0)
        return;

    // 顶层：50ms 峰值保持动态滑行亮段
    const int   holdPoints = juce::jmax (1, (int) (kGrHoldMs / kHistoryStrideMs));
    const int   endIdx     = juce::jlimit (0, historyPoints_ - 1, (int) historyEndOffset());
    const int   start      = juce::jmax (0, endIdx - holdPoints + 1);
    float       outLive    = histOut_[(size_t) start];
    for (int i = start + 1; i <= endIdx; ++i)
        outLive = std::max (outLive, histOut_[(size_t) i]);

    float fracIdx = 0.0f;
    for (int i = 1; i < points; ++i)
    {
        if (curveOutDb_[(size_t) i] >= outLive)
        {
            const float span = curveOutDb_[(size_t) i] - curveOutDb_[(size_t) (i - 1)];
            const float t    = span > 1.0e-6f ? (outLive - curveOutDb_[(size_t) (i - 1)]) / span : 0.0f;
            fracIdx = (float) (i - 1) + juce::jlimit (0.0f, 1.0f, t);
            break;
        }
        fracIdx = (float) i;
    }

    livePath_.clear();
    float tipX = dbToX (curveInDb_[0], well);
    float tipY = dbToY (curveOutDb_[0], well, s);
    for (int i = 0; i <= (int) fracIdx; ++i)
    {
        const float x = dbToX (curveInDb_[(size_t) i], well);
        const float y = dbToY (curveOutDb_[(size_t) i], well, s);
        if (i == 0) livePath_.startNewSubPath (x, y);
        else        livePath_.lineTo (x, y);
        tipX = x;
        tipY = y;
    }

    const int i0 = (int) fracIdx;
    if (i0 < points - 1)
    {
        const float t = fracIdx - (float) i0;
        tipX = dbToX (curveInDb_[(size_t) i0]  * (1.0f - t) + curveInDb_[(size_t) (i0 + 1)]  * t, well);
        tipY = dbToY (curveOutDb_[(size_t) i0] * (1.0f - t) + curveOutDb_[(size_t) (i0 + 1)] * t, well, s);
        livePath_.lineTo (tipX, tipY);
    }

    g.setColour (pal.transferActive);
    g.strokePath (livePath_, juce::PathStrokeType (2.6f * s,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    if (outLive > kGraphBottomDb + 0.5f)
    {
        const float rDot = 3.2f * s;
        g.fillEllipse (tipX - rDot, tipY - rDot, rDot * 2.0f, rDot * 2.0f);
    }
}

void DisplayView::drawThresholdGuide (juce::Graphics& g, const EditorLayout& lay, float guideAlpha)
{
    if (guideAlpha <= 0.01f)
        return;

    const auto& pal    = composition_.snapshot().palette;
    const float s      = lay.scale;
    const auto  well   = lay.plotWellRect ((float) getHeight());
    const float thresh = processor_.apvts.getRawParameterValue (Param::threshold)->load();
    const float ty     = dbToY (thresh, well, s);

    const juce::Colour lineCol = CompTheme::warnText (pal);
    g.setColour (lineCol.withMultipliedAlpha (0.68f * guideAlpha));

    const float dashOn   = 5.5f * s;
    const float dashOff  = 4.0f * s;
    const float dashStep = dashOn + dashOff;
    const float pillW    = 66.0f * s;
    const float pillH    = 18.0f * s;
    const float lineEnd  = well.getRight() - pillW - 14.0f * s;

    for (float xx = well.getX() + 42.0f * s; xx < lineEnd; xx += dashStep)
        g.drawLine (xx, ty, juce::jmin (lineEnd, xx + dashOn), ty, 1.3f * s);

    const float py = juce::jlimit (well.getY() + 4.0f * s, well.getBottom() - pillH - 4.0f * s, ty - pillH * 0.5f);
    const juce::Rectangle<float> pill (well.getRight() - pillW - 8.0f * s, py, pillW, pillH);
    g.setColour (pal.surface.withMultipliedAlpha (0.90f * guideAlpha));
    g.fillRoundedRectangle (pill, 4.5f * s);
    g.setColour (lineCol.withMultipliedAlpha (0.80f * guideAlpha));
    g.drawRoundedRectangle (pill, 4.5f * s, 1.0f * s);
    g.setFont (CompTheme::latinFont (CompTheme::semiBold(), 10.5f * s));
    g.drawText (juce::String (thresh, 1) + " dB", pill, juce::Justification::centred, false);
}

void DisplayView::drawScales (juce::Graphics& g, const EditorLayout& lay)
{
    const auto& pal  = composition_.snapshot().palette;
    const float fs   = lay.scale;
    const auto  well = lay.plotWellRect ((float) getHeight());

    g.setFont (CompTheme::latinFont (CompTheme::regular(), 11.0f * fs));
    for (float db = kGraphTopDb; db >= kGraphBottomDb + 0.5f; db -= kAxisStepDb)
    {
        const bool  isZero = std::abs (db) < 0.01f;
        const float lineY  = dbToY (db, well, fs);
        // Place "0" cleanly below the 0 dB line (so the idle 0 dB red GR line never cuts through the digit),
        // and place negative dB ticks neatly above/beside their grid lines with a subtle readability pill.
        const float y = isZero ? (lineY + 2.5f * fs)
                               : juce::jlimit (well.getY() + 2.0f * fs,
                                               well.getBottom() - 15.0f * fs,
                                               lineY - 13.5f * fs);
        const juce::String txt = juce::String ((int) std::lround (db));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 8.0f * fs;
        const juce::Rectangle<float> pill (well.getX() + 5.0f * fs, y - 0.5f * fs, tw, 14.0f * fs);
        g.setColour (pal.bg.withAlpha (0.90f));
        g.fillRoundedRectangle (pill, 3.2f * fs);

        g.setColour (isZero ? pal.text : pal.dim);
        g.drawText (txt, pill, juce::Justification::centred, false);
    }
}

void DisplayView::drawMeters (juce::Graphics& g, const EditorLayout& lay)
{
    const auto& snap   = composition_.snapshot();
    const auto& pal    = snap.palette;
    const float s      = lay.scale;
    const float x      = lay.plotRight();
    const float top    = lay.topHeight() + 20.0f * s;
    const float bottom = (float) getHeight() - 24.0f * s;

    g.setColour (pal.panel.withAlpha (snap.isNeumorphic() ? 0.35f : 0.68f));
    g.fillRect (x, lay.topHeight(), lay.meterWidth(), (float) getHeight() - lay.topHeight());
    if (! snap.isNeumorphic())
        drawHairlineV (g, x, lay.topHeight(), (float) getHeight(), pal.gridZ);

    const float barW = 13.0f * s;
    const float b1   = x + 6.0f * s;
    const float b2   = x + 25.0f * s;
    const float cx1  = b1 + barW * 0.5f;
    const float cx2  = b2 + barW * 0.5f;

    // 顶部 IN / OUT 柱心微字标
    g.setFont (CompTheme::latinFont (CompTheme::semiBold(), 9.0f * s));
    g.setColour (pal.dim);
    g.drawText ("IN",  juce::Rectangle<float> (cx1 - 10.0f * s, lay.topHeight() + 4.5f * s, 20.0f * s, 13.0f * s),
                juce::Justification::centred, false);
    g.drawText ("OUT", juce::Rectangle<float> (cx2 - 11.0f * s, lay.topHeight() + 4.5f * s, 22.0f * s, 13.0f * s),
                juce::Justification::centred, false);

    for (const float bx : { b1, b2 })
    {
        const juce::Rectangle<float> trough (bx, top, barW, juce::jmax (10.0f, bottom - top));
        if (snap.isNeumorphic())
            UiPack::NeuPainter::drawInsetRect (g, trough, 4.0f * s, pal, snap.theme.insetButton, s, 1.0f, true);
        else
        {
            g.setColour (pal.meterBg);
            g.fillRect (trough);
        }
    }

    const float yIn  = meterY (meterInDb_,  lay);
    const float yOut = meterY (meterOutDb_, lay);
    g.setColour (pal.inFill.withAlpha (0.70f));
    g.fillRect (b1, yIn, barW, juce::jmax (0.0f, bottom - yIn));

    g.setGradientFill (juce::ColourGradient (pal.outLevel, 0.0f, top, pal.accent, 0.0f, bottom, false));
    g.fillRect (b2, yOut, barW, juce::jmax (0.0f, bottom - yOut));

    // 峰值保持暂留条：超过 0 dB 立即变纯红 pal.accent2 (#ff0000) 告警
    g.setColour (peakInDb_ > 0.0f ? pal.accent2 : pal.outLevel);
    g.fillRect (b1, meterY (peakInDb_, lay), barW, 2.0f * s);
    g.setColour (peakOutDb_ > 0.0f ? pal.accent2 : pal.outLevel);
    g.fillRect (b2, meterY (peakOutDb_, lay), barW, 2.0f * s);

    const auto now = juce::Time::getMillisecondCounter();
    if (now >= nextMeterLabelMs_)
    {
        labelInDb_        = meterInDb_;
        labelOutDb_       = meterOutDb_;
        nextMeterLabelMs_ = now + 250;
    }

    // 底部实时 dB 读数严格对齐两柱真实中心轴，920x580 最小尺寸下依然宽裕清爽
    g.setFont (CompTheme::latinFont (CompTheme::regular(), 9.8f * s));
    g.setColour (pal.text);
    g.drawText (juce::String (labelInDb_, 1),
                juce::Rectangle<float> (cx1 - 11.5f * s, (float) getHeight() - 19.5f * s, 23.0f * s, 15.0f * s),
                juce::Justification::centred, false);
    g.drawText (juce::String (labelOutDb_, 1),
                juce::Rectangle<float> (cx2 - 11.5f * s, (float) getHeight() - 19.5f * s, 23.0f * s, 15.0f * s),
                juce::Justification::centred, false);
}

void DisplayView::drawTopStrip (juce::Graphics& g, const EditorLayout& lay)
{
    const auto& snap = composition_.snapshot();
    const auto& pal  = snap.palette;
    const float s    = lay.scale;

    g.setColour (pal.surfaceA);
    g.fillRect (0.0f, 0.0f, lay.width, lay.topHeight());
    if (! snap.isNeumorphic())
        drawHairlineH (g, lay.topHeight() - 1.0f, 0.0f, lay.width, pal.gridZ.withAlpha (0.65f));

    // 1. 左上角品牌标识：圆点 + CRUNCH + COMP（Hasil UI Skill §一.2）
    g.setColour (pal.accent);
    g.fillEllipse (17.0f * s, 20.0f * s, 6.0f * s, 6.0f * s);
    const auto brandFont = CompTheme::brandTitleFont (18.0f * s);
    g.setFont (brandFont);
    g.setColour (pal.text);
    g.drawText ("CRUNCH", juce::Rectangle<float> (30.0f * s, 0.0f, 80.0f * s, lay.topHeight()),
                juce::Justification::centredLeft, false);
    if (std::abs (brandCrunchScale_ - s) > 1.0e-4f)
    {
        brandCrunchWidth_ = juce::GlyphArrangement::getStringWidth (brandFont, "CRUNCH");
        brandCrunchScale_ = s;
    }
    const float modelX = 30.0f * s + brandCrunchWidth_ + 6.0f * s;
    g.setColour (pal.accent);
    g.drawText ("COMP", juce::Rectangle<float> (modelX, 0.0f, 58.0f * s, lay.topHeight()),
                juce::Justification::centredLeft, false);

    // 2. 二级分类预设选择器
    {
        const auto presRect = lay.presetSelector();
        const bool presHov  = (lastHoveredChromeId_ == 2);
        UiPack::NeuPainter::drawButton (g, presRect, pal, snap.theme, presetMenuOpen_, presHov, s, 1.0f, 6.0f * s);

        const int curIdx = juce::jlimit (0, CrunchCompressorAudioProcessor::kNumFactoryPresets - 1,
                                         processor_.getCurrentPresetIndex());
        const int catIdx = curIdx / CrunchCompressorAudioProcessor::kPresetsPerCategory;
        const juce::String display = snap.t ("preset.cat." + juce::String (catIdx))
                                   + " / " + snap.t ("preset.item." + juce::String (curIdx));

        g.setColour (presetMenuOpen_ ? pal.accent : pal.text);
        g.setFont (CompTheme::font (CompTheme::uiMedium(), 12.5f * s));
        g.drawText (display,
                    presRect.withTrimmedLeft (12.0f * s).withTrimmedRight (24.0f * s),
                    juce::Justification::centredLeft, true);

        const float pax = presRect.getRight() - 12.0f * s;
        const float pay = presRect.getCentreY();
        juce::Path chev;
        if (presetMenuOpen_)
        {
            chev.startNewSubPath (pax - 4.0f * s, pay + 2.0f * s);
            chev.lineTo          (pax,            pay - 2.5f * s);
            chev.lineTo          (pax + 4.0f * s, pay + 2.0f * s);
        }
        else
        {
            chev.startNewSubPath (pax - 4.0f * s, pay - 2.0f * s);
            chev.lineTo          (pax,            pay + 2.5f * s);
            chev.lineTo          (pax + 4.0f * s, pay - 2.0f * s);
        }
        g.setColour (presetMenuOpen_ ? pal.accent : pal.dim);
        g.strokePath (chev, juce::PathStrokeType (1.5f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. A | B 状态对比胶囊（0.22s 柔和阻尼平移动画）
    {
        const auto  abRect = lay.abCompareButton();
        const float slideT = juce::jlimit (0.0f, 1.0f, abSlideTween_.value ((double) juce::Time::getMillisecondCounter()));
        const float halfW  = abRect.getWidth() * 0.5f;
        const juce::Rectangle<float> cellA (abRect.getX(),         abRect.getY(), halfW, abRect.getHeight());
        const juce::Rectangle<float> cellB (abRect.getX() + halfW, abRect.getY(), halfW, abRect.getHeight());
        const juce::Rectangle<float> slideCell (abRect.getX() + halfW * slideT, abRect.getY(), halfW, abRect.getHeight());

        if (snap.isNeumorphic())
        {
            UiPack::NeuPainter::drawInsetRect (g, abRect, 6.0f * s, pal, snap.theme.insetButton, s, 1.0f, true);
            const auto activeCell = slideCell.reduced (2.2f * s);
            UiPack::NeuPainter::drawCompactCap (g, activeCell, pal, snap.theme, true, false, s, 1.0f, 4.5f * s, true);
        }
        else
        {
            g.setColour (pal.track);
            g.fillRoundedRectangle (abRect, 6.0f * s);
            g.setColour (pal.gridZ);
            g.drawRoundedRectangle (abRect, 6.0f * s, 1.0f * s);

            const auto activeCell = slideCell.reduced (2.0f * s);
            g.setColour (pal.accent.withMultipliedAlpha (0.26f));
            g.fillRoundedRectangle (activeCell, 4.5f * s);
            g.setColour (pal.accent);
            g.drawRoundedRectangle (activeCell, 4.5f * s, 1.2f * s);
        }

        const juce::Colour onCol = snap.isNeumorphic() ? pal.onAccent : pal.accent;
        g.setFont (CompTheme::latinFont (slideT < 0.5f ? CompTheme::bold() : CompTheme::medium(), 13.0f * s));
        g.setColour (pal.dim.interpolatedWith (onCol, 1.0f - slideT));
        g.drawText ("A", cellA, juce::Justification::centred, false);

        g.setFont (CompTheme::latinFont (slideT >= 0.5f ? CompTheme::bold() : CompTheme::medium(), 13.0f * s));
        g.setColour (pal.dim.interpolatedWith (onCol, slideT));
        g.drawText ("B", cellB, juce::Justification::centred, false);
    }

    // 4. 实时增益衰减遥测内凹徽章（GR -x.x dB / 衰减 -x.x dB，双字体分段绘制）
    {
        const auto  grBox = lay.grBadge();
        const float grAbs = juce::jmax (0.0f, -processor_.getGainReductionDb());
        const bool  compActive = grAbs > 0.08f && ! processor_.isPluginBypassed();

        if (snap.isNeumorphic())
        {
            UiPack::NeuPainter::drawInsetRect (g, grBox, 6.0f * s, pal, snap.theme.insetFocus, s, 1.0f, true);
        }
        else
        {
            g.setColour (pal.meterBg.withAlpha (0.90f));
            g.fillRoundedRectangle (grBox, 6.0f * s);
            g.setColour (compActive ? pal.grTrace.withAlpha (0.55f) : pal.gridZ);
            g.drawRoundedRectangle (grBox, 6.0f * s, 1.0f * s);
        }

        const juce::String preStr = snap.t ("top.grPrefix");
        const juce::String valStr = (compActive ? "-" : "") + juce::String (grAbs, 1) + " dB";

        const auto fPre = snap.isCjkLocale()
                            ? CompTheme::font (CompTheme::medium(), 11.5f * s)
                            : CompTheme::latinFont (CompTheme::semiBold(), 11.5f * s);
        const auto fVal = CompTheme::latinFont (CompTheme::semiBold(), 13.5f * s);

        const float wPre = juce::GlyphArrangement::getStringWidth (fPre, preStr);
        const float wVal = juce::GlyphArrangement::getStringWidth (fVal, valStr);
        const float x0   = grBox.getCentreX() - (wPre + wVal) * 0.5f;

        g.setFont (fPre);
        g.setColour (pal.dim);
        g.drawText (preStr, juce::Rectangle<float> (x0, grBox.getY(), wPre + 2.0f, grBox.getHeight()),
                    juce::Justification::centredLeft, false);

        g.setFont (fVal);
        g.setColour (compActive ? pal.grTrace : pal.text);
        g.drawText (valStr, juce::Rectangle<float> (x0 + wPre, grBox.getY(), wVal + 4.0f, grBox.getHeight()),
                    juce::Justification::centredLeft, false);
    }

    // 5. 最右侧三道错落调节滑杆主题设置按钮
    {
        const auto t    = lay.theme();
        const bool tHov = (lastHoveredChromeId_ == 6);
        UiPack::NeuPainter::drawButton (g, t, pal, snap.theme, packMenuOpen_, tHov, s, 1.0f, 6.0f * s);

        const auto  centre   = t.getCentre();
        const float halfSpan = 6.8f * s;
        const float rowY[3]  = { centre.y - 5.0f * s, centre.y, centre.y + 5.0f * s };
        const float knobX[3] = { centre.x - 2.6f * s, centre.x + 3.0f * s, centre.x - 1.2f * s };
        const float rKnob    = 1.9f * s;
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (packMenuOpen_ ? pal.accent.withMultipliedAlpha (0.85f) : pal.dim);
            g.drawLine (centre.x - halfSpan, rowY[i], centre.x + halfSpan, rowY[i], 1.4f * s);
            g.setColour (pal.accent);
            g.fillEllipse (knobX[i] - rKnob, rowY[i] - rKnob, rKnob * 2.0f, rKnob * 2.0f);
        }
    }
}

void DisplayView::paint (juce::Graphics& g)
{
    const auto  clipBounds = g.getClipBounds();
    const auto& snap       = composition_.snapshot();
    const auto  lay        = makeLayout();
    const int   topBarBot  = juce::roundToInt (lay.topHeight()) + 4;
    const int   meterLeft  = juce::roundToInt (lay.plotRight()) - 4;
    const int   curW       = getWidth(), curH = getHeight();

    if (curW == staticPlotBgW_ && curH == staticPlotBgH_)
        staticPlotStableHits_ = juce::jmin (10, staticPlotStableHits_ + 1);
    else
    {
        staticPlotBgW_        = curW;
        staticPlotBgH_        = curH;
        staticPlotStableHits_ = 1;
    }

    bool usedStaticBgCache = false;
    if (! processor_.isWindowDragging() && staticPlotStableHits_ >= 2 && curW > 0 && curH > 0)
    {
        if (! staticPlotBgCache_.isValid()
            || staticPlotBgCache_.getWidth()  < curW
            || staticPlotBgCache_.getHeight() < curH)
        {
            const int allocW = (curW + 63) & ~63;
            const int allocH = (curH + 63) & ~63;
            staticPlotBgCache_ = juce::Image (juce::Image::RGB, allocW, allocH, false);
            staticPlotBgRev_   = 0;
        }
        if (staticPlotBgRev_ != snap.revision)
        {
            juce::Graphics cg (staticPlotBgCache_);
            cg.reduceClipRegion (0, 0, curW, curH);
            drawBackground (cg, lay);
            cg.saveState();
            const auto well = lay.plotWellRect ((float) curH);
            cg.reduceClipRegion (snap.layout.sunkenPlotWell ? well.reduced (1.0f).toNearestInt()
                                                            : well.toNearestInt());
            drawGrid (cg, lay);
            cg.restoreState();
            staticPlotBgRev_ = snap.revision;
        }
        g.drawImage (staticPlotBgCache_,
                     clipBounds.getX(), clipBounds.getY(), clipBounds.getWidth(), clipBounds.getHeight(),
                     clipBounds.getX(), clipBounds.getY(), clipBounds.getWidth(), clipBounds.getHeight());
        usedStaticBgCache = true;
    }
    else
    {
        staticPlotBgRev_ = 0;
        drawBackground (g, lay);
    }

    // 谐波染色渐变底板（网格之上、滚动波形与传递曲线之下）
    drawColourWash (g, lay);

    if (clipBounds.getBottom() > topBarBot - 4 && clipBounds.getX() < meterLeft + 4)
    {
        const auto  well       = lay.plotWellRect ((float) curH);
        const float guideAlpha = threshGuideTween_.value ((double) juce::Time::getMillisecondCounter());

        g.saveState();
        g.reduceClipRegion (snap.layout.sunkenPlotWell ? well.reduced (1.0f).toNearestInt()
                                                       : well.toNearestInt());
        if (! usedStaticBgCache)
            drawGrid (g, lay);

        if (! processor_.isWindowDragging())
        {
            drawHistoryGraph (g, lay);
            drawGrCurve (g, lay);
            if (processor_.isKneeCurveVisible())
                drawTransferCurve (g, lay);
        }
        drawThresholdGuide (g, lay, guideAlpha);
        g.restoreState();

        // 新拟物内凹视窗四角反向遮罩 + 内凹阴影环单图缓存
        if (snap.isNeumorphic() || snap.layout.sunkenPlotWell)
        {
            if (! wellRimCache_.isValid() || wellRimW_ != curW || wellRimH_ != curH || wellRimRev_ != snap.revision)
            {
                const int allocW = (curW + 63) & ~63;
                const int allocH = (curH + 63) & ~63;
                if (! wellRimCache_.isValid() || wellRimCache_.getWidth() < allocW || wellRimCache_.getHeight() < allocH)
                    wellRimCache_ = juce::Image (juce::Image::ARGB, allocW, allocH, true);
                wellRimCache_.clear (juce::Rectangle<int> (0, 0, curW, curH));
                juce::Graphics rg (wellRimCache_);
                const float r = (snap.layout.plotCornerRadius > 0.0f ? snap.layout.plotCornerRadius : snap.theme.radiusLg) * lay.scale;
                UiPack::NeuPainter::drawRoundedWellCornerMaskAndRim (rg, well, r, snap.palette, snap.theme.insetWell, lay.scale);
                wellRimW_   = curW;
                wellRimH_   = curH;
                wellRimRev_ = snap.revision;
            }
            g.drawImage (wellRimCache_,
                         clipBounds.getX(), clipBounds.getY(), clipBounds.getWidth(), clipBounds.getHeight(),
                         clipBounds.getX(), clipBounds.getY(), clipBounds.getWidth(), clipBounds.getHeight());
        }

        drawScales (g, lay);
    }

    if (clipBounds.getRight() >= meterLeft)
        drawMeters (g, lay);
    if (clipBounds.getY() <= topBarBot)
        drawTopStrip (g, lay);
}

void DisplayView::paintOverChildren (juce::Graphics& g)
{
    const auto lay = makeLayout();
    drawPackSwitcher (g, lay);
    drawPresetMenu (g, lay);
}

// ---------------- 四包主题切换卡片与同心圆角二级分类预设菜单 ----------------

void DisplayView::drawPackSwitcher (juce::Graphics& g, const EditorLayout& lay)
{
    if (! packMenuOpen_)
        return;

    const auto& snap = composition_.snapshot();
    const auto& pal  = snap.palette;
    const float s    = lay.scale;
    const auto  card = lay.packSwitcherCard();

    if (snap.isNeumorphic())
    {
        UiPack::NeuPainter::drawRaisedRect (g, card, snap.theme.radiusLg * s, pal, snap.theme.shadowCard, s, 1.0f, false);
    }
    else
    {
        g.setColour (pal.halo.withMultipliedAlpha (0.44f));
        g.fillRoundedRectangle (card.translated (0.0f, 5.0f * s).expanded (3.0f * s), 12.0f * s);
        g.setColour (pal.surface.withAlpha (0.96f));
        g.fillRoundedRectangle (card, 12.0f * s);
        g.setColour (pal.gridZ);
        g.drawRoundedRectangle (card, 12.0f * s, 1.0f * s);
    }

    g.setColour (pal.text);
    g.setFont (CompTheme::font (CompTheme::uiMedium(), 13.5f * s));
    g.drawText (snap.t ("pack.title"),
                card.withTrimmedLeft (12.0f * s).withHeight (30.0f * s),
                juce::Justification::centredLeft, false);

    auto drawSectionTitle = [&] (const char* key, float yOff)
    {
        g.setColour (pal.dim);
        g.setFont (CompTheme::font (CompTheme::uiMedium(), 11.0f * s));
        g.drawText (snap.t (key),
                    juce::Rectangle<float> (card.getX() + 12.0f * s, card.getY() + yOff * s,
                                            card.getWidth() - 24.0f * s, 16.0f * s),
                    juce::Justification::centredLeft, false);
    };

    auto drawPackBtn = [&] (int sec, int idx, const juce::String& text, bool active, bool forceLatin = false)
    {
        const auto br  = lay.packOptionRect (sec, idx);
        const bool hov = br.contains (lastMousePos_);
        UiPack::NeuPainter::drawButton (g, br, pal, snap.theme, active, hov, s, 1.0f, 5.0f * s);
        g.setColour (active ? pal.accent : (hov ? pal.text : pal.dim));
        g.setFont (forceLatin
                     ? CompTheme::latinFont (active ? CompTheme::uiMedium() : CompTheme::uiRegular(), 11.5f * s)
                     : CompTheme::font (active ? CompTheme::uiMedium() : CompTheme::uiRegular(), 11.5f * s));
        g.drawText (text, br.reduced (3.0f * s, 0.0f), juce::Justification::centred, false);
    };

    const bool isNeuPreset     = snap.themeId == UiPack::ThemeId::Neumorphism
                              && snap.paletteId == UiPack::PaletteId::NeuLight
                              && snap.layoutId == UiPack::LayoutId::Sculpted;
    const bool isClassicPreset = snap.themeId == UiPack::ThemeId::Classic
                              && snap.paletteId == UiPack::PaletteId::Blue
                              && snap.layoutId == UiPack::LayoutId::Standard;
    drawPackBtn (0, 0, snap.t ("pack.presetNeu"),     isNeuPreset);
    drawPackBtn (0, 1, snap.t ("pack.presetClassic"), isClassicPreset);

    drawSectionTitle ("pack.theme", 65.0f);
    drawPackBtn (1, 0, snap.t ("pack.theme.classic"), snap.themeId == UiPack::ThemeId::Classic);
    drawPackBtn (1, 1, snap.t ("pack.theme.neu"),     snap.themeId == UiPack::ThemeId::Neumorphism);

    drawSectionTitle ("pack.palette", 111.0f);
    const char* palLabels[6] = { "Blue", "Red", "Cream", "NeuLight", "NeuTeal", "NeuDark" };
    for (int i = 0; i < 6; ++i)
        drawPackBtn (2, i, palLabels[i], (int) snap.paletteId == i, true);

    drawSectionTitle ("pack.locale", 187.0f);
    drawPackBtn (4, 0, "English", (int) snap.localeId == 0, true);
    drawPackBtn (4, 1, juce::String::fromUTF8 ("\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"), (int) snap.localeId == 1, false);

    drawPackBtn (5, 0, snap.t ("pack.rollback"), false);
}

void DisplayView::togglePackSwitcher()
{
    closePresetMenu();
    packMenuOpen_ = ! packMenuOpen_;
    repaint();
}

void DisplayView::closePackSwitcher()
{
    if (! packMenuOpen_)
        return;
    packMenuOpen_ = false;
    repaint();
}

bool DisplayView::handlePackSwitcherClick (juce::Point<float> p, const EditorLayout& lay)
{
    if (! packMenuOpen_)
        return false;
    if (! lay.packSwitcherCard().contains (p))
        return false;

    for (int i = 0; i < 2; ++i)
        if (lay.packOptionRect (0, i).contains (p))
        {
            if (i == 0) applyNeumorphismPreset (UiPack::PaletteId::NeuLight);
            else        applyClassicPreset (UiPack::PaletteId::Blue);
            return true;
        }
    for (int i = 0; i < 2; ++i)
        if (lay.packOptionRect (1, i).contains (p))
        {
            setThemePack ((UiPack::ThemeId) i);
            return true;
        }
    for (int i = 0; i < 6; ++i)
        if (lay.packOptionRect (2, i).contains (p))
        {
            setPalettePack ((UiPack::PaletteId) i);
            return true;
        }
    for (int i = 0; i < 2; ++i)
        if (lay.packOptionRect (4, i).contains (p))
        {
            setLocalePack ((UiPack::LocaleId) i);
            return true;
        }
    if (lay.packOptionRect (5, 0).contains (p))
    {
        rollbackUiPacks();
        return true;
    }
    return true;
}

juce::Rectangle<float> DisplayView::presetPrimaryRect (const EditorLayout& lay) const
{
    const float s    = lay.scale;
    const auto  trig = lay.presetSelector();
    const int   n    = CrunchCompressorAudioProcessor::kNumPresetCategories;
    const float w    = 182.0f * s;
    const float h    = (float) n * 28.0f * s + (float) (n - 1) * 2.0f * s + 12.0f * s;
    return { trig.getX(), lay.topHeight() + 4.0f * s, w, h };
}

juce::Rectangle<float> DisplayView::presetSecondaryRect (const EditorLayout& lay) const
{
    const float s   = lay.scale;
    const auto  pri = presetPrimaryRect (lay);
    const int   n   = CrunchCompressorAudioProcessor::kPresetsPerCategory;
    const float w   = 224.0f * s;
    const float h   = (float) n * 28.0f * s + (float) (n - 1) * 2.0f * s + 52.0f * s;
    const int   cat = juce::jlimit (0, CrunchCompressorAudioProcessor::kNumPresetCategories - 1, presetHoverCategory_);
    const float y   = juce::jlimit (pri.getY(),
                                    juce::jmax (pri.getY(), (float) getHeight() - h - 8.0f * s),
                                    pri.getY() + (float) cat * 18.0f * s);
    return { pri.getRight() + 6.0f * s, y, w, h };
}

juce::Rectangle<float> DisplayView::presetCategoryRowRect (const EditorLayout& lay, int cat) const
{
    const float s   = lay.scale;
    const auto  pri = presetPrimaryRect (lay);
    const int   c   = juce::jlimit (0, CrunchCompressorAudioProcessor::kNumPresetCategories - 1, cat);
    return { pri.getX() + 6.0f * s, pri.getY() + 6.0f * s + (float) c * 30.0f * s, pri.getWidth() - 12.0f * s, 28.0f * s };
}

juce::Rectangle<float> DisplayView::presetItemRowRect (const EditorLayout& lay, int itemInCat) const
{
    const float s   = lay.scale;
    const auto  sec = presetSecondaryRect (lay);
    const int   idx = juce::jlimit (0, CrunchCompressorAudioProcessor::kPresetsPerCategory - 1, itemInCat);
    return { sec.getX() + 6.0f * s, sec.getY() + 6.0f * s + (float) idx * 30.0f * s, sec.getWidth() - 12.0f * s, 28.0f * s };
}

juce::Rectangle<float> DisplayView::presetSaveAsRect (const EditorLayout& lay) const
{
    const float s   = lay.scale;
    const auto  sec = presetSecondaryRect (lay);
    const float top = sec.getBottom() - 34.0f * s;
    return { sec.getX() + 6.0f * s, top, sec.getWidth() - 12.0f * s, 28.0f * s };
}

void DisplayView::openPresetMenu()
{
    if (presetMenuOpen_)
    {
        closePresetMenu();
        return;
    }
    closePackSwitcher();
    presetMenuOpen_      = true;
    presetHoverCategory_ = juce::jlimit (0, CrunchCompressorAudioProcessor::kNumPresetCategories - 1,
                                         processor_.getCurrentPresetIndex() / CrunchCompressorAudioProcessor::kPresetsPerCategory);
    presetHoverItem_     = -1;
    presetHoverSaveAs_   = false;
    repaint();
}

void DisplayView::closePresetMenu()
{
    if (! presetMenuOpen_)
        return;
    presetMenuOpen_    = false;
    presetHoverItem_   = -1;
    presetHoverSaveAs_ = false;
    repaint();
}

bool DisplayView::handlePresetMenuClick (juce::Point<float> p, const EditorLayout& lay)
{
    if (! presetMenuOpen_)
        return false;

    for (int c = 0; c < CrunchCompressorAudioProcessor::kNumPresetCategories; ++c)
    {
        if (presetCategoryRowRect (lay, c).contains (p))
        {
            presetHoverCategory_ = c;
            presetHoverItem_     = -1;
            presetHoverSaveAs_   = false;
            repaint();
            return true;
        }
    }

    for (int i = 0; i < CrunchCompressorAudioProcessor::kPresetsPerCategory; ++i)
    {
        if (presetItemRowRect (lay, i).contains (p))
        {
            const int presetIdx = presetHoverCategory_ * CrunchCompressorAudioProcessor::kPresetsPerCategory + i;
            processor_.applyFactoryPreset (presetIdx);
            closePresetMenu();
            transferDirty_ = true;
            if (onPresetApplied)
                onPresetApplied();
            repaint();
            return true;
        }
    }

    if (presetSaveAsRect (lay).contains (p))
        return true;

    if (presetPrimaryRect (lay).contains (p) || presetSecondaryRect (lay).contains (p))
        return true;

    return false;
}

void DisplayView::drawPresetMenu (juce::Graphics& g, const EditorLayout& lay)
{
    if (! presetMenuOpen_)
        return;

    const auto& snap   = composition_.snapshot();
    const auto& pal    = snap.palette;
    const float s      = lay.scale;
    const auto  pri    = presetPrimaryRect (lay);
    const auto  sec    = presetSecondaryRect (lay);
    const float pad    = 6.0f * s;
    const float corner = (snap.isNeumorphic() ? snap.theme.radiusLg : 10.0f) * s;
    const float rEdge  = juce::jmax (4.0f * s, corner - pad);
    const float rMid   = 4.0f * s;

    auto drawMenuCard = [&] (juce::Rectangle<float> box)
    {
        if (snap.isNeumorphic())
        {
            UiPack::NeuPainter::drawRaisedRect (g, box, corner, pal, snap.theme.shadowCard, s, 0.98f, false);
        }
        else
        {
            g.setColour (pal.halo.withMultipliedAlpha (0.45f));
            g.fillRoundedRectangle (box.translated (0.0f, 3.5f * s).expanded (2.0f * s), corner + 2.0f * s);
            g.setColour (pal.surface.withAlpha (0.96f));
            g.fillRoundedRectangle (box, corner);
            g.setColour (pal.accent.withMultipliedAlpha (0.65f));
            g.drawRoundedRectangle (box, corner, 1.0f * s);
        }
    };

    drawMenuCard (pri);
    drawMenuCard (sec);

    const int curPreset = processor_.getCurrentPresetIndex();
    const int curCat    = curPreset / CrunchCompressorAudioProcessor::kPresetsPerCategory;
    const int nCats     = CrunchCompressorAudioProcessor::kNumPresetCategories;

    for (int c = 0; c < nCats; ++c)
    {
        const auto  row    = presetCategoryRowRect (lay, c);
        const bool  isOpen = (c == presetHoverCategory_);
        const bool  hasCur = (c == curCat);
        const float rTop   = (c == 0)         ? rEdge : rMid;
        const float rBot   = (c == nCats - 1) ? rEdge : rMid;

        if (isOpen || hasCur)
        {
            const auto rowPath = PackPopupOverlay::makeConcentricRowPath (row, rTop, rBot);
            if (snap.isNeumorphic())
            {
                g.setColour (isOpen ? pal.bg.interpolatedWith (pal.accent, 0.14f) : pal.bgLight.withAlpha (0.75f));
                g.fillPath (rowPath);
            }
            else
            {
                g.setColour (pal.accent.withMultipliedAlpha (isOpen ? 0.26f : 0.14f));
                g.fillPath (rowPath);
            }
        }

        g.setColour (isOpen ? pal.accent : (hasCur ? pal.text : pal.dim));
        g.setFont (CompTheme::font (isOpen || hasCur ? CompTheme::uiMedium() : CompTheme::uiRegular(), 12.5f * s));
        g.drawText (snap.t ("preset.cat." + juce::String (c)),
                    row.withTrimmedLeft (10.0f * s).withTrimmedRight (20.0f * s),
                    juce::Justification::centredLeft, false);

        const float ax = row.getRight() - 10.0f * s, ay = row.getCentreY();
        juce::Path arrow;
        arrow.startNewSubPath (ax - 2.0f * s, ay - 3.5f * s);
        arrow.lineTo          (ax + 2.0f * s, ay);
        arrow.lineTo          (ax - 2.0f * s, ay + 3.5f * s);
        g.strokePath (arrow, juce::PathStrokeType (1.4f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    for (int i = 0; i < CrunchCompressorAudioProcessor::kPresetsPerCategory; ++i)
    {
        const auto  row       = presetItemRowRect (lay, i);
        const int   presetIdx = presetHoverCategory_ * CrunchCompressorAudioProcessor::kPresetsPerCategory + i;
        const bool  isCur     = (presetIdx == curPreset);
        const bool  isHov     = (i == presetHoverItem_);
        const float rTop      = (i == 0) ? rEdge : rMid;

        if (isCur || isHov)
        {
            const auto rowPath = PackPopupOverlay::makeConcentricRowPath (row, rTop, rMid);
            if (snap.isNeumorphic())
            {
                g.setColour (isCur ? pal.bg.interpolatedWith (pal.accent, 0.16f) : pal.bgLight.withAlpha (0.85f));
                g.fillPath (rowPath);
            }
            else
            {
                g.setColour (pal.accent.withMultipliedAlpha (isCur ? 0.28f : 0.16f));
                g.fillPath (rowPath);
            }
        }

        g.setColour (isCur ? pal.accent : pal.text);
        g.setFont (CompTheme::font (isCur ? CompTheme::semiBold() : CompTheme::regular(), 12.5f * s));
        g.drawText (snap.t ("preset.item." + juce::String (presetIdx)),
                    row.withTrimmedLeft (12.0f * s),
                    juce::Justification::centredLeft, false);
    }

    const auto  saveBtn = presetSaveAsRect (lay);
    const float sepY    = saveBtn.getY() - 5.0f * s;
    g.setColour (pal.gridZ.withMultipliedAlpha (snap.isNeumorphic() ? 0.45f : 0.85f));
    g.drawLine (sec.getX() + 10.0f * s, sepY, sec.getRight() - 10.0f * s, sepY, 1.0f * s);

    const auto savePath = PackPopupOverlay::makeConcentricRowPath (saveBtn, rMid, rEdge);
    g.setColour (presetHoverSaveAs_ ? pal.accent.withAlpha (0.18f) : pal.track.withAlpha (snap.isNeumorphic() ? 0.45f : 0.70f));
    g.fillPath (savePath);
    g.setColour (presetHoverSaveAs_ ? pal.accent : pal.text);
    g.setFont (CompTheme::font (CompTheme::uiMedium(), 12.0f * s));
    g.drawText (snap.t ("preset.saveAs"), saveBtn, juce::Justification::centred, false);
}

// ---------------- 鼠标与键盘交互 ----------------

int DisplayView::hitTestChromeHoverId (juce::Point<float> pos, const EditorLayout& lay) const
{
    if (lay.presetSelector().contains (pos))  return 2;
    if (lay.abCompareButton().contains (pos)) return 3;
    if (lay.theme().contains (pos))           return 6;
    return -1;
}

void DisplayView::mouseMove (const juce::MouseEvent& e)
{
    lastMousePos_  = e.position;
    const auto lay = makeLayout();

    if (packMenuOpen_ && lay.packSwitcherCard().contains (e.position))
    {
        repaint (lay.packSwitcherCard().expanded (8.0f).toNearestInt());
        return;
    }

    if (presetMenuOpen_)
    {
        int  newCat  = presetHoverCategory_;
        int  newItem = -1;
        bool newSave = presetSaveAsRect (lay).contains (e.position);

        for (int c = 0; c < CrunchCompressorAudioProcessor::kNumPresetCategories; ++c)
            if (presetCategoryRowRect (lay, c).contains (e.position))
                newCat = c;
        for (int i = 0; i < CrunchCompressorAudioProcessor::kPresetsPerCategory; ++i)
            if (presetItemRowRect (lay, i).contains (e.position))
                newItem = i;

        if (newCat != presetHoverCategory_ || newItem != presetHoverItem_ || newSave != presetHoverSaveAs_)
        {
            presetHoverCategory_ = newCat;
            presetHoverItem_     = newItem;
            presetHoverSaveAs_   = newSave;
            repaint();
        }
        return;
    }

    const int chromeHov = hitTestChromeHoverId (e.position, lay);
    if (chromeHov != lastHoveredChromeId_)
    {
        lastHoveredChromeId_ = chromeHov;
        repaint (0, 0, getWidth(), juce::roundToInt (lay.topHeight()) + 4);
    }
}

void DisplayView::mouseExit (const juce::MouseEvent&)
{
    lastMousePos_ = { -100.0f, -100.0f };
    if (lastHoveredChromeId_ != -1)
    {
        lastHoveredChromeId_ = -1;
        repaint (0, 0, getWidth(), juce::roundToInt (makeLayout().topHeight()) + 4);
    }
}

void DisplayView::mouseDown (const juce::MouseEvent& e)
{
    ensureTimerFullRate();
    const auto p   = e.position;
    const auto lay = makeLayout();

    if (packMenuOpen_)
    {
        if (handlePackSwitcherClick (p, lay))
            return;
        if (! lay.theme().contains (p))
            closePackSwitcher();
    }

    if (presetMenuOpen_)
    {
        if (handlePresetMenuClick (p, lay))
            return;
        if (! lay.presetSelector().contains (p))
            closePresetMenu();
    }

    if (lay.presetSelector().contains (p))
    {
        openPresetMenu();
        return;
    }
    if (lay.abCompareButton().contains (p))
    {
        closePresetMenu();
        closePackSwitcher();
        processor_.toggleAbSlot();
        lastAbSlotB_ = processor_.isAbSlotB();
        abSlideTween_.start ((double) juce::Time::getMillisecondCounter(),
                             lastAbSlotB_ ? 1.0f : 0.0f,
                             Anim::kSegSlideMs,
                             abSlideTween_.value ((double) juce::Time::getMillisecondCounter()));
        transferDirty_  = true;
        uiStatePending_ = true;
        if (onPresetApplied)
            onPresetApplied();
        repaint();
        return;
    }
    if (lay.theme().contains (p))
    {
        togglePackSwitcher();
        return;
    }
}

bool DisplayView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (packMenuOpen_)   { closePackSwitcher(); return true; }
        if (presetMenuOpen_) { closePresetMenu();   return true; }
    }
    return false;
}
