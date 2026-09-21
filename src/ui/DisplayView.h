#pragma once

#include <JuceHeader.h>

#include <functional>
#include <vector>

#include "../PluginProcessor.h"
#include "LookAndFeel.h"

// Settings (gear) button drawn as a vector icon: the embedded Poppins
// typeface has no gear glyph, and a hand-drawn icon keeps the rounded-pill
// look consistent with the rest of the widgets.
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("Settings") {}

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override
    {
        const bool light = isLightTheme();

        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const float corner = juce::jmin (14.0f, bounds.getHeight() * 0.5f);
        auto bg = CrunchPalette::track (light);
        if (shouldDrawButtonAsDown)
            bg = bg.brighter (0.12f);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter (0.06f);
        g.setColour (bg);
        g.fillRoundedRectangle (bounds, corner);

        const auto c = getLocalBounds().toFloat().getCentre();
        const float r = 5.0f;
        g.setColour (CrunchPalette::text (light));
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / 8.0f;
            juce::Path spoke;
            spoke.startNewSubPath (c.x + (r - 1.0f) * std::cos (a), c.y + (r - 1.0f) * std::sin (a));
            spoke.lineTo (c.x + (r + 3.0f) * std::cos (a), c.y + (r + 3.0f) * std::sin (a));
            g.strokePath (spoke, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 2.0f);
    }

private:
    bool isLightTheme() const
    {
        if (auto* laf = dynamic_cast<CrunchLookAndFeel*> (&getLookAndFeel()))
            return laf->isLightTheme();
        return true;
    }
};

// Upper display, Pro-C style:
//  - top strip: red gain-reduction curve (compression amount over time)
//  - main graph: scrolling level history (grey input area, light output line)
//  - two-layer knee transfer curve: white static layer + green layer that
//    bounces with the live gain reduction
//  - input/output peak meters on the right + GR readout
class DisplayView : public juce::Component,
                    public juce::Timer
{
public:
    explicit DisplayView (CrunchCompressorAudioProcessor& processor);
    ~DisplayView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    std::function<void()> onThemeChanged;

private:
    void drawGrid (juce::Graphics& g);
    void drawHistoryGraph (juce::Graphics& g);
    void drawGrCurve (juce::Graphics& g);
    void drawTransferCurve (juce::Graphics& g);
    void drawGainReduction (juce::Graphics& g);
    void drawMeters (juce::Graphics& g);
    void showSettingsMenu();

    float grToY (float grDb) const;
    float dbToY (float db) const;
    float dbToX (float db) const;
    int   plotWidth() const;

    // Theme accent (same source of truth as the EQ plugin).
    juce::Colour accentColour() const;

    // Top inset reserved for the GR readout / settings button; the level
    // graph starts below it. The GR curve shares the same dB scale as the
    // graph (0 dB at the top inset, -60 dB at the bottom), so its amount can
    // be read directly against the left axis instead of a separate strip
    // scale that used to clip at -24 dB.
    float topInset() const;

    // Cursor position of the newest drawn history point, in array units
    // (see scrollCursor_ / historyBase_). 0 when no history is available.
    float historyEndOffset() const;

    // Decimated polyline helper using a FIXED time-based x mapping: one
    // history point always spans pxPerPoint pixels and the newest drawn
    // point sits at the right edge, so the graph scrolls by pure translation
    // at a constant px/ms — no zoom-out while the history fills up, and no
    // residual per-frame scaling at steady state. Each pixel column takes the
    // extreme value inside it (no path bloat).
    //
    // The columns are anchored to a FIXED grid in ring-position (absolute
    // time) space, NOT to the sliding right edge. The previous version
    // derived every column's data window from the fractional scroll offset,
    // so with ~11 samples per column and a ~3 px/frame advance at 60 Hz the
    // exact set of samples in each column changed every frame: the selected
    // peak hopped to a neighbouring sample, moving the vertex up/down by
    // several dB and sideways by up to a pixel every frame. On the thin
    // output stroke that read as an abnormal shimmer/jitter (the grey input
    // fill hid the same artifact).
    //
    // With a ring-anchored grid a column's value only depends on the ring
    // samples it covers, so the decimated shape is frozen and the scroll is a
    // pure sub-pixel translation: every vertex moves by exactly the same
    // amount while new columns merely enter/leave at the edges. No vertex is
    // ever re-selected mid-scroll, so nothing boils.
    // NOTE: uses an explicit "started" flag, NOT Path::isEmpty() — a path
    // containing only move markers still reports isEmpty() == true.
    template <typename MapY>
    void appendDecimated (juce::Path& path, const float* values, int n, float endOffset, bool useMin, MapY mapY) const
    {
        const float plotW = (float) plotWidth();
        if (n < 2 || endOffset < 1.0f || plotW <= 1.0f)
            return;

        const float pxPerPoint = plotW / (float) (kHistoryPoints - 1);
        if (pxPerPoint <= 0.0f)
            return;

        const double pointsPerPx = 1.0 / (double) pxPerPoint;   // ring points per pixel column
        const double rightIdx = (double) endOffset;             // array index at the right edge
        const double baseRing = historyBase_;                   // ring position of array index 0
        const double scrollRing = baseRing + rightIdx;          // ring position at the right edge

        // Visible columns: k covers ring positions [k*P, (k+1)*P); it is on
        // screen while its centre x lies in [0, plotW]. One column of margin
        // is kept on each side so partial columns are drawn, not popped in.
        const double kMinD = std::ceil ((scrollRing - (double) (kHistoryPoints - 1)) / pointsPerPx - 0.5) - 1.0;
        const double kMaxD = std::floor (scrollRing / pointsPerPx - 0.5) + 1.0;

        const long long kMin = (long long) kMinD;
        const long long kMax = (long long) kMaxD;
        const long long nMinus1 = (long long) n - 1;
        const long long baseRingI = (long long) std::llround (baseRing);

        bool started = false;

        for (long long k = kMin; k <= kMax; ++k)
        {
            const double centreRing = ((double) k + 0.5) * pointsPerPx;

            // frozen data range of this column, in array-index units
            const long long a0Raw = (long long) std::ceil ((double) k * pointsPerPx) - baseRingI;
            const long long a1Raw = (long long) std::ceil (((double) k + 1.0) * pointsPerPx) - 1 - baseRingI;

            if (a1Raw < 0 || a0Raw > nMinus1)
                continue;                       // column is outside the available history

            const long long a0 = juce::jmax (0LL, a0Raw);
            const long long a1 = juce::jmin (nMinus1, a1Raw);
            if (a0 > a1)
                continue;

            float v = values[(size_t) a0];
            for (long long i = a0 + 1; i <= a1; ++i)
                v = useMin ? juce::jmin (v, values[(size_t) i])
                           : juce::jmax (v, values[(size_t) i]);

            const float x = plotW - (float) ((scrollRing - centreRing) * (double) pxPerPoint);
            const float y = mapY (v);

            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                path.lineTo (x, y);
            }
        }
    }

    CrunchCompressorAudioProcessor& processor_;

    // Level history scratch buffers, filled from the processor each tick.
    // kHistoryPoints is the DISPLAYED time span (~5 s at 0.5 ms per point): it
    // sets the px/point scale. The buffers deliberately hold — and readHistory
    // is asked for — extra OLDER points beyond that span. The left edge of the
    // visible window is then always well inside the available history, so the
    // first drawn column is never clipped by the ring's oldest sample. Without
    // that margin the endpoint column's data range gets truncated as samples
    // age out, so its value and cell identity change every few ms and the left
    // tip of the curves visibly jitters.
    static constexpr int kHistoryPoints     = 10000;   // ~5 s display span
    static constexpr int kHistoryReadPoints = 11000;   // span + ~0.5 s older margin
    std::vector<float> histIn_, histOut_, histGr_;
    int historyPoints_ = 0;

    // Scroll cursor: the drawn window ends at scrollCursor_ (ring-point
    // units), exponentially smoothed toward the newest recorded point.
    // Host message loops deliver timer ticks irregularly, which would make
    // the scroll step jitter (2-4 px per frame); the smoothing absorbs that
    // so the waveform glides at a steady rate (~60 ms lag, invisible on a
    // multi-second history). The smoothing is time-based so the glide rate
    // does not change with the host's message-loop tick rate.
    // historyBase_ is the ring position of array index 0 of the last read.
    // Both are double: the scroll is a sub-pixel translation and float
    // rounding at ~10^7 ring points would show up as a visible wobble.
    static constexpr double kScrollTimeConstantSec = 0.06;
    double scrollCursor_ = -1.0;
    double historyBase_ = 0.0;
    double lastTickMs_ = 0.0;

    // transfer-curve scratch (members, so no per-frame heap allocation)
    std::vector<float> curveInDb_, curveOutDb_;

    // meter ballistics (peak-hold with 1 dB/tick decay)
    float meterInDb_ = -120.0f;
    float meterOutDb_ = -120.0f;
    float meterInDbLabel_ = -120.0f;
    float meterOutDbLabel_ = -120.0f;

    // Peak-hold for the meter top strokes, ported from the Crunch EQ plugin:
    // the stroke rides the peak and holds it for kPeakHoldMs before falling
    // back, so a short peak stays readable (Pro-Q 3 style). The bar itself
    // keeps its fast (instant up / 60 dB per second down) ballistics.
    static constexpr int kPeakHoldMs = 2000;          // 1-3 s, as requested
    static constexpr float kPeakFallDbPerSec = 30.0f; // after the hold expires

    float peakInDb_  = -120.0f, peakOutDb_  = -120.0f;
    juce::uint32 peakInHoldUntilMs_ = 0, peakOutHoldUntilMs_ = 0;
    juce::uint32 lastMeterMs_ = 0;

    void updatePeakHold (float& peakDb, juce::uint32& holdUntilMs,
                         float levelDb, juce::uint32 nowMs, double dt);

    GearButton settingsButton_;
    juce::TextButton kneeButton_;
    juce::Label kneeLabel_;

    static constexpr int   kMeterWidth = 60;
    static constexpr float kGraphTopDb = 0.0f;    // left axis: 0 dB at the top
    static constexpr float kGraphBottomDb = -60.0f;
    static constexpr float kAxisStepDb = 6.0f;    // grid + label every 6 dB
    static constexpr float kMeterTopDb = 6.0f;    // meters keep headroom above 0 dB
    static constexpr float kMeterBottomDb = -60.0f;
    static constexpr float kHistoryStrideMs = 0.5f;   // one history point per 0.5 ms
    static constexpr float kGrHoldMs = 50.0f;         // green curve peak-hold time

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DisplayView)
};
