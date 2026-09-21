#include "DisplayView.h"
#include "LookAndFeel.h"

DisplayView::DisplayView (CrunchCompressorAudioProcessor& processor)
    : processor_ (processor)
{
    startTimerHz (60);

    histIn_.resize ((size_t) kHistoryReadPoints);
    histOut_.resize ((size_t) kHistoryReadPoints);
    histGr_.resize ((size_t) kHistoryReadPoints);

    addAndMakeVisible (settingsButton_);
    settingsButton_.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9a\x99"));
    settingsButton_.setTooltip ("Settings");
    settingsButton_.onClick = [this] { showSettingsMenu(); };

    addAndMakeVisible (kneeButton_);
    kneeButton_.setButtonText ("< CURVE >");
    kneeButton_.setTooltip ("Show / hide the knee transfer curve");
    kneeButton_.setClickingTogglesState (true);
    kneeButton_.setToggleState (true, juce::dontSendNotification);

    addAndMakeVisible (kneeLabel_);
    kneeLabel_.setText ("KNEE", juce::dontSendNotification);
    kneeLabel_.setJustificationType (juce::Justification::centred);
    kneeLabel_.setFont (CrunchLookAndFeel::uiFont (11.0f, 600));
    kneeLabel_.setColour (juce::Label::textColourId,
                          CrunchPalette::textDim (processor_.isLightTheme()));
}

DisplayView::~DisplayView()
{
    stopTimer();
}

void DisplayView::resized()
{
    settingsButton_.setBounds (4, 4, 24, 24);
    kneeLabel_.setBounds (plotWidth() - 92, getHeight() / 2 - 27, 92, 13);
    kneeButton_.setBounds (plotWidth() - 92, getHeight() / 2 - 12, 92, 26);

    // Both cached geometries are in component coordinates, so a resize
    // invalidates them (they are rebuilt lazily on the next paint).
    transferDirty_ = true;
}

// Top inset reserved for the GR readout / settings button; the shared dB scale
// starts below it (0 dB at top inset, kGraphBottomDb at the bottom edge).
float DisplayView::topInset() const
{
    return juce::jmax (28.0f, (float) getHeight() * 0.10f);
}

// GR curve: mapped onto the SAME dB scale as the level graph. The recorded
// gain reductions are already negative dB (e.g. -6.3 for 6.3 dB of
// compression), so they map straight onto the scale: a reduction of X dB sits
// exactly on the "-X dB" grid line. Deep compression simply extends further
// down instead of clipping at a strip boundary.
float DisplayView::grToY (float grDb) const
{
    return dbToY (grDb);
}

float DisplayView::dbToY (float db) const
{
    const float d = juce::jlimit (kGraphBottomDb, kGraphTopDb, db);
    const float top = topInset();
    const float usable = (float) getHeight() - top;
    return top + (kGraphTopDb - d) / (kGraphTopDb - kGraphBottomDb) * usable;
}

float DisplayView::dbToX (float db) const
{
    const float d = juce::jlimit (kGraphBottomDb, kGraphTopDb, db);
    return (d - kGraphBottomDb) / (kGraphTopDb - kGraphBottomDb) * (float) plotWidth();
}

juce::Colour DisplayView::accentColour() const
{
    return processor_.getThemeAccent();
}

int DisplayView::plotWidth() const
{
    return juce::jmax (1, getWidth() - kMeterWidth);
}

void DisplayView::timerCallback()
{
    // The editor window is being dragged or resized (FL Studio moves plugin
    // editors as their own top-level window, one window-position change per
    // mouse sample). Stand down completely: the OS-driven repaints after each
    // move own the message loop during a drag, and our animation repaints
    // would only compete with them. Everything catches up on the first tick
    // after the drag ends.
    if (processor_.isWindowDragging())
        return;

    // readHistory returns the exact counter its copy was based on, so the
    // scroll position is aligned to the data instead of to a counter that
    // may have advanced between the two reads (that mismatch shifted the
    // window by a few points every tick and added to the visible jitter).
    unsigned counter = 0;
    historyPoints_ = processor_.readHistory (kHistoryReadPoints,
                                             histIn_.data(), histOut_.data(), histGr_.data(),
                                             &counter);
    const double counterNow = (double) counter;
    historyBase_ = counterNow - (double) historyPoints_;

    // Advance the scroll cursor toward the newest point. The host delivers
    // timer ticks irregularly; smoothing here keeps the waveform scrolling
    // at a steady rate instead of stepping with the frame timing. The
    // coefficient is derived from the real elapsed time so the glide rate is
    // independent of the host's tick rate.
    // BOTH clamps are essential: the host re-runs prepareToPlay (transport
    // start/stop, sample-rate/buffer changes) which resets the ring counter
    // to 0 閳?without the upper clamp a stale cursor would sit far ahead of
    // the data and the whole graph would vanish until the cursor caught up.
    if (historyPoints_ >= 2)
    {
        if (scrollCursor_ < 0.0)
        {
            scrollCursor_ = counterNow;
            lastTickMs_ = 0.0;
        }
        else
        {
            const double nowMs = juce::Time::getMillisecondCounterHiRes();
            double dtSec = lastTickMs_ > 0.0 ? (nowMs - lastTickMs_) / 1000.0 : 1.0 / 60.0;
            lastTickMs_ = nowMs;
            dtSec = juce::jlimit (0.001, 0.1, dtSec);

            const double coeff = 1.0 - std::exp (-dtSec / kScrollTimeConstantSec);
            scrollCursor_ += (counterNow - scrollCursor_) * coeff;
            scrollCursor_ = juce::jlimit (counterNow - (double) (historyPoints_ - 1),
                                          counterNow,
                                          scrollCursor_);
        }
    }

    const float inDb  = processor_.getInputPeakDb();
    const float outDb = processor_.getOutputPeakDb();
    meterInDb_  = (inDb  > meterInDb_)  ? inDb  : juce::jmax (meterInDb_  - 1.0f, -60.0f);
    meterOutDb_ = (outDb > meterOutDb_) ? outDb : juce::jmax (meterOutDb_ - 1.0f, -60.0f);

    meterInDbLabel_  = meterInDb_;
    meterOutDbLabel_ = meterOutDb_;

    // Peak-hold for the top strokes (ported from the Crunch EQ plugin). dt is
    // measured so the fall rate does not depend on the timer rate.
    const auto nowMs = juce::Time::getMillisecondCounter();
    const double dtMeter = (lastMeterMs_ == 0)
                            ? 1.0 / 60.0
                            : juce::jlimit (0.001, 0.100, (double) (nowMs - lastMeterMs_) / 1000.0);
    lastMeterMs_ = nowMs;

    updatePeakHold (peakInDb_,  peakInHoldUntilMs_,  inDb,  nowMs, dtMeter);
    updatePeakHold (peakOutDb_, peakOutHoldUntilMs_, outDb, nowMs, dtMeter);

    // Rebuild the static transfer-curve geometry only when the parameters or
    // the component size changed. Done here rather than in paint() so the
    // dirty flag is always consumed - even while the knee curve is hidden.
    const bool curveRebuilt = transferDirty_ || transferBaseW_ != getWidth() || transferBaseH_ != getHeight();
    if (curveRebuilt)
    {
        const int points = 128;
        processor_.getTransferCurve (kGraphBottomDb, kGraphTopDb, points,
                                     curveInDb_, curveOutDb_);

        transferBasePath_.clear();
        for (int i = 0; i < points; ++i)
        {
            const float x = dbToX (curveInDb_[(size_t) i]);
            const float y = dbToY (curveOutDb_[(size_t) i]);
            if (i == 0) transferBasePath_.startNewSubPath (x, y);
            else        transferBasePath_.lineTo (x, y);
        }

        transferDirty_  = false;
        transferBaseW_  = getWidth();
        transferBaseH_ = getHeight();
    }

    // Repaint only when the picture can actually differ. The graph scrolls
    // whenever the ring counter advances (audio playing); the cursor keeps
    // gliding for ~60 ms after the transport stops; the meters decay for
    // ~1 s. Once everything is static, a stopped instance converges to zero
    // repaints instead of redrawing the full display 60x/s.
    const float grNow = processor_.getGainReductionDb();
    const bool contentChanged = curveRebuilt
                             || counter != lastPaintedCounter_
                             || std::abs (scrollCursor_ - lastPaintedCursor_) > 0.01
                             || kneeButton_.getToggleState() != lastPaintedKnee_
                             || std::abs (grNow - lastPaintedGr_) > 0.05f;

    const bool metersChanged = std::abs (meterInDb_  - lastMeterIn_)  > 0.01f
                            || std::abs (meterOutDb_ - lastMeterOut_) > 0.01f
                            || std::abs (peakInDb_   - lastPeakIn_)   > 0.01f
                            || std::abs (peakOutDb_  - lastPeakOut_)  > 0.01f;

    if (contentChanged)
    {
        lastPaintedCounter_ = counter;
        lastPaintedCursor_  = scrollCursor_;
        lastPaintedGr_      = grNow;
        lastPaintedKnee_    = kneeButton_.getToggleState();
        repaint();
    }
    else if (metersChanged)
    {
        // Meters only: repaint just the meter strip instead of the whole
        // display (grid blit + history fills + curves).
        repaint (juce::Rectangle<int> (plotWidth(), 0, kMeterWidth, getHeight()));
    }

    lastMeterIn_  = meterInDb_;
    lastMeterOut_ = meterOutDb_;
    lastPeakIn_   = peakInDb_;
    lastPeakOut_  = peakOutDb_;
}

void DisplayView::updatePeakHold (float& peakDb, juce::uint32& holdUntilMs,
                                  float levelDb, juce::uint32 nowMs, double dt)
{
    if (levelDb >= peakDb)
    {
        // New peak: snap up and (re)start the hold.
        peakDb = levelDb;
        holdUntilMs = nowMs + (juce::uint32) kPeakHoldMs;
    }
    else if (nowMs >= holdUntilMs)
    {
        // Hold expired: fall back towards the current level, never below it, so
        // the stroke always sits on top of the bar.
        peakDb = juce::jmax (levelDb, peakDb - (float) (kPeakFallDbPerSec * dt));
    }
}

float DisplayView::historyEndOffset() const
{
    if (historyPoints_ <= 0 || scrollCursor_ < 0.0)
        return 0.0f;
    return (float) (scrollCursor_ - historyBase_);
}

void DisplayView::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Theme: Blue");
    menu.addItem (2, "Theme: Red");
    menu.addItem (3, "Theme: Cream");

    menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
    {
        if (r >= 1 && r <= 3)
        {
            processor_.setTheme (r - 1);
            if (auto* laf = dynamic_cast<CrunchLookAndFeel*> (&getLookAndFeel()))
                laf->setTheme (r - 1);
            if (onThemeChanged)
                onThemeChanged();
            repaint();
        }
    });
}

void DisplayView::drawGrid (juce::Graphics& g)
{
    const bool light = processor_.isLightTheme();

    g.setColour (CrunchPalette::background (light));
    g.fillAll();

    g.setColour (CrunchPalette::grid (light));
    for (int i = 1; i <= 10; ++i)
        g.drawVerticalLine (plotWidth() * i / 10, 0.0f, (float) getHeight());

    for (float db = kGraphBottomDb; db <= kGraphTopDb + 0.01f; db += kAxisStepDb)
        g.drawHorizontalLine ((int) dbToY (db), 0.0f, (float) plotWidth());

    g.setColour (CrunchPalette::gridZero (light));
    g.drawHorizontalLine ((int) dbToY (0.0f), 0.0f, (float) plotWidth());

    // Left axis: 0 dB at the very top, one label every 6 dB down to -60 dB.
    // The GR curve shares this scale (GR X dB sits on the -X dB line), so the
    // labels read both level and gain reduction.
    g.setFont (CrunchLookAndFeel::uiFont (10.5f, 500));
    g.setColour (CrunchPalette::textDim (light));
    for (float db = kGraphTopDb; db >= kGraphBottomDb - 0.01f; db -= kAxisStepDb)
    {
        juce::String label = (db > 0.0f ? "+" : "") + juce::String ((int) db);
        g.drawText (label, 4, (int) dbToY (db) - 8, 40, 16, juce::Justification::left, false);
    }
}

void DisplayView::refreshGridCache()
{
    const bool light = processor_.isLightTheme();

    if (gridImage_.isValid()
        && gridCacheW_ == getWidth() && gridCacheH_ == getHeight()
        && gridCacheLight_ == light)
        return;

    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    // Opaque ARGB: drawGrid() fills the background first, so under the
    // software renderer the per-frame blit is a direct copy.
    gridImage_ = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    {
        juce::Graphics gi (gridImage_);
        drawGrid (gi);
    }

    gridCacheW_ = getWidth();
    gridCacheH_ = getHeight();
    gridCacheLight_ = light;
}

void DisplayView::drawHistoryGraph (juce::Graphics& g)
{
    const int n = historyPoints_;
    if (n < 2)
        return;

    const bool light = processor_.isLightTheme();
    const float plotW = (float) plotWidth();
    const float h = (float) getHeight();
    const float endOffset = historyEndOffset();

    // Layer 1: the INPUT level, styled like the Crunch EQ's grey spectrum layer
    // (inputFill gradient fill + a 1 px outline). Signal source unchanged: the
    // input peak history.
    inCurve_.clear();
    appendDecimated (inCurve_, histIn_.data(), n, endOffset, false, [this] (float db) { return dbToY (db); });

    const auto grey = CrunchPalette::inputFill (light);
    juce::ColourGradient greyGrad (grey.withAlpha (0.42f), 0.0f, 0.0f,
                                   grey.withAlpha (0.08f), 0.0f, h, false);
    g.setGradientFill (greyGrad);

    inFill_.clear();
    appendDecimated (inFill_, histIn_.data(), n, endOffset, false, [this] (float db) { return dbToY (db); });
    inFill_.lineTo (plotW, h);
    inFill_.lineTo (0.0f, h);
    inFill_.closeSubPath();
    g.fillPath (inFill_);

    g.setColour (grey.withAlpha (0.55f));
    g.strokePath (inCurve_, juce::PathStrokeType (1.0f));

    // Layer 2: the OUTPUT level, styled like the Crunch EQ's accent spectrum
    // layer (theme accent gradient fill + the 2 px white outline). Signal source
    // unchanged: the output peak history. It sits on top of the input layer, so
    // make-up gain shows as accent sticking out of the grey, and compression
    // shows as grey sticking out of the accent.
    outCurve_.clear();
    appendDecimated (outCurve_, histOut_.data(), n, endOffset, false, [this] (float db) { return dbToY (db); });

    const auto accent = accentColour();
    juce::ColourGradient accentGrad (accent.withAlpha (0.65f), 0.0f, 0.0f,
                                     accent.withAlpha (0.10f), 0.0f, h, false);
    g.setGradientFill (accentGrad);

    outFill_.clear();
    appendDecimated (outFill_, histOut_.data(), n, endOffset, false, [this] (float db) { return dbToY (db); });
    outFill_.lineTo (plotW, h);
    outFill_.lineTo (0.0f, h);
    outFill_.closeSubPath();
    g.fillPath (outFill_);

    // white outline of the output layer
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.strokePath (outCurve_, juce::PathStrokeType (2.0f));
}

// Red gain-reduction curve: shows the amount of compression over time on the
// shared dB scale (reduction of X dB = the -X dB grid line). Drawn on top of
// the level history so it stays readable where the curves overlap.
void DisplayView::drawGrCurve (juce::Graphics& g)
{
    const int n = historyPoints_;
    if (n < 2)
        return;

    // per-column minimum = deepest compression in that column
    grPath_.clear();
    appendDecimated (grPath_, histGr_.data(), n, historyEndOffset(), true, [this] (float db) { return grToY (db); });

    g.setColour (CrunchPalette::grLine (processor_.isLightTheme()));
    g.strokePath (grPath_, juce::PathStrokeType (1.6f));
}

void DisplayView::drawTransferCurve (juce::Graphics& g)
{
    const bool light = processor_.isLightTheme();
    const int points = 128;
    const auto& inDb  = curveInDb_;
    const auto& outDb = curveOutDb_;

    // bottom layer: white static transfer curve (full, never moves).
    // Geometry is cached by timerCallback() (see transferBasePath_); it is
    // only rebuilt when the parameters or the size change.
    g.setColour (CrunchPalette::curveBase (light).withAlpha (0.55f));
    g.strokePath (transferBasePath_, juce::PathStrokeType (2.0f));

    // top layer: green segment of the same curve, from the origin up to the
    // current output level. Its endpoint slides along the white curve as the
    // compressed level moves 閳?like a level meter filling inside the curve.
    if (historyPoints_ <= 0)
        return;

    // Peak-hold: follow the maximum output level over the trailing hold
    // window (kGrHoldMs) instead of the instantaneous value, so the endpoint
    // glides like a peak-hold meter instead of jittering with every ripple.
    // The window ends at the (smoothed) scroll cursor so the green layer
    // stays consistent with the history graph it overlays.
    // History points are recorded every kHistoryStrideMs regardless of SR.
    const int holdPoints = juce::jmax (1, (int) (kGrHoldMs / kHistoryStrideMs));
    const int endIdx = juce::jlimit (0, historyPoints_ - 1, (int) historyEndOffset());
    const int start = juce::jmax (0, endIdx - holdPoints + 1);
    float outLive = histOut_[(size_t) start];
    for (int i = start + 1; i <= endIdx; ++i)
        outLive = std::max (outLive, histOut_[(size_t) i]);

    // the transfer curve is strictly increasing, so scan for the fractional
    // index where the curve's output reaches the live output level
    float fracIdx = 0.0f;
    for (int i = 1; i < points; ++i)
    {
        if (outDb[(size_t) i] >= outLive)
        {
            const float span = outDb[(size_t) i] - outDb[(size_t) (i - 1)];
            const float t = span > 1.0e-6f ? (outLive - outDb[(size_t) (i - 1)]) / span : 0.0f;
            fracIdx = (float) (i - 1) + juce::jlimit (0.0f, 1.0f, t);
            break;
        }
        fracIdx = (float) i;
    }

    juce::Path& live = livePath_;
    live.clear();
    for (int i = 0; i <= (int) fracIdx; ++i)
    {
        const float x = dbToX (inDb[(size_t) i]);
        const float y = dbToY (outDb[(size_t) i]);
        if (i == 0) live.startNewSubPath (x, y);
        else live.lineTo (x, y);
    }

    // interpolated endpoint exactly at the live output level
    const int i0 = (int) fracIdx;
    if (i0 < points - 1)
    {
        const float t = fracIdx - (float) i0;
        const float x = dbToX (inDb[(size_t) i0] * (1.0f - t) + inDb[(size_t) (i0 + 1)] * t);
        const float y = dbToY (outDb[(size_t) i0] * (1.0f - t) + outDb[(size_t) (i0 + 1)] * t);
        live.lineTo (x, y);
    }

    g.setColour (CrunchPalette::curveLine (light));
    g.strokePath (live, juce::PathStrokeType (2.5f));
}

void DisplayView::drawGainReduction (juce::Graphics& g)
{
    const float grDb = juce::jmax (0.0f, -processor_.getGainReductionDb());

    g.setFont (CrunchLookAndFeel::uiFont (kGrReadoutFontPx, kGrReadoutFontWeight));
    g.setColour (CrunchPalette::textDim (processor_.isLightTheme()));

    // Right-aligned in the reserved top strip, clear of the meter column on
    // the right. The box is wide enough for the longest readout and vertically
    // centred, so the larger type stays inside topInset() (above the 0 dB
    // grid line) at every editor size.
    g.drawText ("GR " + juce::String (grDb, 1) + " dB",
                plotWidth() - 180, 3, 172, 25, juce::Justification::right, false);
}

void DisplayView::drawMeters (juce::Graphics& g)
{
    const bool light = processor_.isLightTheme();
    const int x0 = plotWidth();
    const int areaW = getWidth() - x0;
    const int barW = (areaW - 6) / 2;
    const int bar1x = x0 + 2;
    const int bar2x = x0 + 4 + barW;

    const float minDb = kMeterBottomDb, maxDb = kMeterTopDb;
    const float h = (float) getHeight();

    const auto meterY = [&] (float db)
    {
        const float frac = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return (1.0f - frac) * h;
    };

    // Meters. Same style as the Crunch EQ plugin: the IN bar is a flat colour
    // wash, the OUT bar is a vertical gradient spanning the full bar height
    // (theme accent at the top, the theme's "current" colour at the bottom).
    // The top stroke rides the held peak (peak hold) and is white in the dark
    // Blue/Red themes so it stands out; in Cream it keeps the grey.
    const auto drawMeter = [&] (int x, int w, float db, float peakDb,
                                const juce::String& label, const juce::String& dbText,
                                juce::Colour col, juce::Colour bottomColour,
                                juce::Colour capColour, bool gradient)
    {
        g.setColour (CrunchPalette::meterBg (light));
        g.fillRect (x, 0, w, getHeight());

        g.setColour (CrunchPalette::grid (light));
        for (float dbTick = minDb; dbTick <= 0.0f; dbTick += 12.0f)
            g.drawHorizontalLine ((int) meterY (dbTick), (float) x, (float) (x + w));

        const float y  = meterY (juce::jlimit (minDb, maxDb, db));
        const float py = meterY (juce::jlimit (minDb, maxDb, peakDb));

        if (gradient)
            g.setGradientFill (juce::ColourGradient (col,          (float) x, 0.0f,
                                                     bottomColour, (float) x, h, false));
        else
            g.setColour (col.withAlpha (0.35f));

        g.fillRect (x, (int) y, w, getHeight() - (int) y);

        // top stroke: the held peak, so short peaks stay visible
        g.setColour (capColour);
        g.fillRect (x, (int) py, w, 2);

        g.setFont (CrunchLookAndFeel::uiFont (11.0f, 500));
        g.setColour (CrunchPalette::textDim (light));
        g.drawText (label, x, 3, w, 13, juce::Justification::centred, false);

        g.setFont (CrunchLookAndFeel::uiFont (10.0f, 500));
        g.drawText (dbText, x, getHeight() - 16, w, 13, juce::Justification::centred, false);
    };

    const juce::Colour accent = accentColour();

    // Cream (light) ends at a darker accent; Blue/Red end at the current OUT
    // colour so the bar fades towards the tone it used before.
    const juce::Colour outBottom = light ? accent.darker (0.5f)
                                         : CrunchPalette::outputLevel (light);

    // Top stroke: white in the dark Blue/Red themes; Cream keeps the grey.
    const juce::Colour capColour = light ? CrunchPalette::inputFill (light)
                                         : juce::Colours::white;

    drawMeter (bar1x, barW, meterInDb_,  peakInDb_,  "IN",  juce::String (meterInDbLabel_, 1),
               CrunchPalette::inputFill (light), CrunchPalette::inputFill (light), capColour, false);
    drawMeter (bar2x, barW, meterOutDb_, peakOutDb_, "OUT", juce::String (meterOutDbLabel_, 1),
               accent, outBottom, capColour, true);
}

void DisplayView::paint (juce::Graphics& g)
{
    refreshGridCache();
    g.drawImageAt (gridImage_, 0, 0);

    // While the window is being dragged, every OS move event already forces a
    // full repaint; the scrolling history fills are the expensive part of the
    // frame, so they are dropped for those repaints only (the grid, GR readout
    // and meters stay correct). Normal ticks redraw everything.
    if (! processor_.isWindowDragging())
    {
        drawHistoryGraph (g);
        drawGrCurve (g);

        if (kneeButton_.getToggleState())
            drawTransferCurve (g);
    }

    drawGainReduction (g);
    drawMeters (g);
}
