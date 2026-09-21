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

    repaint();
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

void DisplayView::drawHistoryGraph (juce::Graphics& g)
{
    const int n = historyPoints_;
    if (n < 2)
        return;

    const bool light = processor_.isLightTheme();
    const float plotW = (float) plotWidth();
    const float h = (float) getHeight();
    const float endOffset = historyEndOffset();

    // input level area (grey, filled to the bottom). Decimated to one point
    // per pixel column (max inside each column): identical look, ~12x less
    // path work, and no Path copy for the fill.
    juce::Path inFill;
    appendDecimated (inFill, histIn_.data(), n, endOffset, false, [this] (float db) { return dbToY (db); });
    inFill.lineTo (plotW, h);
    inFill.lineTo (0.0f, h);
    inFill.closeSubPath();
    g.setColour (CrunchPalette::inputFill (light).withAlpha (0.45f));
    g.fillPath (inFill);

    // output level line (light)
    juce::Path outPath;
    appendDecimated (outPath, histOut_.data(), n, endOffset, false, [this] (float db) { return dbToY (db); });
    g.setColour (CrunchPalette::outputLevel (light));
    g.strokePath (outPath, juce::PathStrokeType (1.6f));
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
    juce::Path p;
    appendDecimated (p, histGr_.data(), n, historyEndOffset(), true, [this] (float db) { return grToY (db); });

    g.setColour (CrunchPalette::grLine (processor_.isLightTheme()));
    g.strokePath (p, juce::PathStrokeType (1.6f));
}

void DisplayView::drawTransferCurve (juce::Graphics& g)
{
    const bool light = processor_.isLightTheme();
    const int points = 128;
    processor_.getTransferCurve (kGraphBottomDb, kGraphTopDb, points, curveInDb_, curveOutDb_);
    const auto& inDb  = curveInDb_;
    const auto& outDb = curveOutDb_;

    // bottom layer: white static transfer curve (full, never moves)
    juce::Path base;
    for (int i = 0; i < points; ++i)
    {
        const float x = dbToX (inDb[(size_t) i]);
        const float y = dbToY (outDb[(size_t) i]);
        if (i == 0) base.startNewSubPath (x, y);
        else base.lineTo (x, y);
    }
    g.setColour (CrunchPalette::curveBase (light).withAlpha (0.55f));
    g.strokePath (base, juce::PathStrokeType (2.0f));

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

    juce::Path live;
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

    g.setFont (CrunchLookAndFeel::uiFont (12.0f, 500));
    g.setColour (CrunchPalette::textDim (processor_.isLightTheme()));
    g.drawText ("GR " + juce::String (grDb, 1) + " dB", plotWidth() - 132, 3, 124, 17, juce::Justification::right, false);
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
    drawGrid (g);
    drawHistoryGraph (g);
    drawGrCurve (g);

    if (kneeButton_.getToggleState())
        drawTransferCurve (g);

    drawGainReduction (g);
    drawMeters (g);
}
