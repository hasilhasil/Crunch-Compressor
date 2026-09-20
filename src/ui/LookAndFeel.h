#pragma once

#include <JuceHeader.h>

#include <BinaryData.h>

#include <mutex>

// Single source of truth for the UI palette, shared by the LookAndFeel,
// the editor background and the DisplayView plot colours.
//
// The light ("Cream") theme is modelled on the reference reverb UI: a warm
// cream surface (#F6F0E7) with a coral accent (#E07856), near-white knob
// caps and soft warm-grey rings/tracks. The dark Blue/Red themes keep their
// original values but reuse the same knob/widget rendering.
struct CrunchPalette
{
    static juce::Colour background  (bool light) { return light ? juce::Colour (0xfff6f0e7) : juce::Colour (0xff101214); }
    static juce::Colour panel       (bool light) { return light ? juce::Colour (0xffefe8db) : juce::Colour (0xff1a1d20); }
    static juce::Colour surface     (bool light) { return light ? juce::Colour (0xfffffdf8) : juce::Colour (0xff22262b); }
    static juce::Colour track       (bool light) { return light ? juce::Colour (0xffded4c4) : juce::Colour (0xff2a2e33); }
    static juce::Colour knobBody    (bool light) { return light ? juce::Colour (0xffffffff) : juce::Colour (0xfff2efe8); }
    static juce::Colour text        (bool light) { return light ? juce::Colour (0xff463f38) : juce::Colour (0xffd8d8d8); }
    static juce::Colour textDim     (bool light) { return light ? juce::Colour (0xff8d8375) : juce::Colour (0xff9aa0a6); }
    static juce::Colour grid        (bool light) { return light ? juce::Colour (0xffe7dfcf) : juce::Colour (0xff22262b); }
    static juce::Colour gridZero    (bool light) { return light ? juce::Colour (0xffc8bdac) : juce::Colour (0xff3c4249); }
    static juce::Colour meterBg     (bool light) { return light ? juce::Colour (0xfff2ebdf) : juce::Colour (0xff16181b); }
    static juce::Colour inputFill   (bool light) { return light ? juce::Colour (0xffb5aa98) : juce::Colour (0xff6a7076); }
    static juce::Colour outputLevel (bool light) { return light ? juce::Colour (0xff463f38) : juce::Colour (0xffe8eaed); }
    static juce::Colour grLine      (bool light) { return light ? juce::Colour (0xffd95f43) : juce::Colour (0xffe05b5b); }
    static juce::Colour curveBase   (bool light) { return light ? juce::Colour (0xff463f38) : juce::Colour (0xffffffff); }
    static juce::Colour curveLine   (bool light) { return light ? juce::Colour (0xffc07a3e) : juce::Colour (0xffe0a95a); }
};

class CrunchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrunchLookAndFeel()
    {
        setTheme (2);
    }

    // --- embedded typefaces (Poppins, SIL OFL 1.1) ---------------------------
    // Loaded once from the plugin's binary data so the rounded UI font is
    // available on any machine, independent of installed system fonts.
    static juce::Typeface::Ptr typefaceForWeight (int weight)
    {
        static juce::Typeface::Ptr reg, med, sem, bold;
        static std::once_flag once;
        std::call_once (once, []
        {
            reg  = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf,  BinaryData::PoppinsRegular_ttfSize);
            med  = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf,   BinaryData::PoppinsMedium_ttfSize);
            sem  = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsSemiBold_ttf, BinaryData::PoppinsSemiBold_ttfSize);
            bold = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf,     BinaryData::PoppinsBold_ttfSize);
        });

        switch (weight)
        {
            case 500: return med;
            case 600: return sem;
            case 700: return bold;
            default:  return reg;
        }
    }

    // UI font in pixels (JUCE height, not point size) with a Poppins weight.
    static juce::Font uiFont (float heightPx, int weight = 400)
    {
        juce::FontOptions options (typefaceForWeight (weight));
        return juce::Font (options.withHeight (heightPx));
    }

    // Single source of truth for the per-theme accent colour; the processor
    // delegates here so the UI and the plot can never drift apart.
    static juce::Colour accentForTheme (int theme)
    {
        switch (theme)
        {
            case 1:  return juce::Colour (0xffe05b5b);
            case 2:  return juce::Colour (0xffe07856);   // coral (reference UI)
            default: return juce::Colour (0xff2f9dff);
        }
    }

    void setTheme (int theme)
    {
        accentColour_ = accentForTheme (theme);
        lightTheme_   = (theme == 2);

        const auto bg      = CrunchPalette::background (lightTheme_);
        const auto panel   = CrunchPalette::panel      (lightTheme_);
        const auto surface = CrunchPalette::surface    (lightTheme_);
        const auto track   = CrunchPalette::track      (lightTheme_);
        const auto text    = CrunchPalette::text       (lightTheme_);
        const auto textDim = CrunchPalette::textDim    (lightTheme_);

        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::Slider::backgroundColourId,          panel);
        setColour (juce::Slider::thumbColourId,               accentColour_);
        setColour (juce::Slider::trackColourId,               track);
        setColour (juce::Slider::rotarySliderFillColourId,    accentColour_);
        setColour (juce::Slider::rotarySliderOutlineColourId, track);
        setColour (juce::Slider::textBoxTextColourId,         text);
        setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colour (0x00000000));
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colour (0x00000000));
        setColour (juce::ComboBox::backgroundColourId,        surface);
        setColour (juce::ComboBox::textColourId,              text);
        setColour (juce::ComboBox::outlineColourId,           track);
        setColour (juce::ComboBox::arrowColourId,             accentColour_);
        setColour (juce::ComboBox::buttonColourId,            surface);
        setColour (juce::TextButton::buttonColourId,          track);
        setColour (juce::TextButton::buttonOnColourId,        accentColour_);
        setColour (juce::TextButton::textColourOffId,         text);
        setColour (juce::TextButton::textColourOnId,          juce::Colour (0xfffffff8));
        setColour (juce::Label::textColourId,                 textDim);
        setColour (juce::PopupMenu::backgroundColourId,             surface);
        setColour (juce::PopupMenu::textColourId,                  text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour_);
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colour (0xfffffff8));
    }

    juce::Colour getAccentColour() const { return accentColour_; }
    bool isLightTheme() const            { return lightTheme_; }

    // -----------------------------------------------------------------------
    // Knobs: warm-grey ring track + accent value arc + near-white cap with a
    // soft drop shadow and a dark pointer, mirroring the reference UI.
    // -----------------------------------------------------------------------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        const float d      = (float) juce::jmin (width, height);
        const float radius = d * 0.5f - 2.0f;
        const float cx     = (float) x + (float) width  * 0.5f;
        const float cy     = (float) y + (float) height * 0.5f;

        const auto trackColour = slider.findColour (juce::Slider::rotarySliderOutlineColourId);
        const auto fillColour  = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // ring: full track, then the accent arc up to the current value
        const float ringW = juce::jmax (3.0f, radius * 0.17f);
        const float ringR = juce::jmax (1.0f, radius - ringW * 0.5f);

        juce::Path ring;
        ring.addCentredArc (cx, cy, ringR, ringR, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (trackColour);
        g.strokePath (ring, juce::PathStrokeType (ringW, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        if (sliderPosProportional > 0.0f)
        {
            const float toAngle = rotaryStartAngle
                                + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
            juce::Path value;
            value.addCentredArc (cx, cy, ringR, ringR, 0.0f,
                                 rotaryStartAngle, toAngle, true);
            g.setColour (fillColour);
            g.strokePath (value, juce::PathStrokeType (ringW, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }

        // knob cap: near-white disc with a soft drop shadow (concentric
        // offset ellipses approximate the blur without offscreen passes)
        const float capR      = juce::jmax (1.5f, ringR - ringW * 0.5f - juce::jmin (1.5f, radius * 0.03f));
        const float shadowDy  = juce::jmax (1.0f, radius * 0.05f);
        for (int i = 4; i >= 1; --i)
        {
            const float rr = capR + (float) i * juce::jmax (0.6f, capR * 0.05f);
            g.setColour (juce::Colours::black.withAlpha (0.012f + 0.016f * (float) (5 - i)));
            g.fillEllipse (cx - rr, cy - rr + shadowDy + (float) i * 0.6f,
                           rr * 2.0f, rr * 2.0f);
        }

        g.setColour (CrunchPalette::knobBody (lightTheme_));
        g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.06f));
        g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

        // pointer: always a dark warm grey — the knob cap is near-white in
        // every theme, so a theme-dependent (light-grey in the dark themes)
        // pointer had poor contrast and was hard to read.
        const float angle   = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float ptrLen  = capR * 0.86f;
        const float ptrW    = juce::jmax (1.8f, capR * 0.10f);
        g.setColour (juce::Colour (0xff463f38));
        juce::Path ptr;
        ptr.startNewSubPath (cx, cy);
        ptr.lineTo (cx + ptrLen * std::sin (angle), cy - ptrLen * std::cos (angle));
        g.strokePath (ptr, juce::PathStrokeType (ptrW, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // -----------------------------------------------------------------------
    // Combos: light rounded selectors with an accent caret (reference style).
    // -----------------------------------------------------------------------
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f, 0.5f);
        const float corner = juce::jmin (8.0f, bounds.getHeight() * 0.5f);

        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, corner, 1.0f);

        const float cw = 7.0f, ch = 4.5f;
        const float ccx = bounds.getRight() - 13.0f;
        const float ccy = bounds.getCentreY();
        juce::Path caret;
        caret.startNewSubPath (ccx - cw * 0.5f, ccy - ch * 0.5f);
        caret.lineTo (ccx + cw * 0.5f, ccy - ch * 0.5f);
        caret.lineTo (ccx, ccy + ch * 0.5f);
        caret.closeSubPath();
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (caret);
    }

    // -----------------------------------------------------------------------
    // Buttons: rounded pills; accent-filled while a toggle is engaged.
    // -----------------------------------------------------------------------
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (backgroundColour);

        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f, 0.5f);
        if (bounds.isEmpty())
            return;

        const float corner = juce::jmin (14.0f, bounds.getHeight() * 0.5f);
        auto bg = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId)
                                          : button.findColour (juce::TextButton::buttonColourId);

        if (shouldDrawButtonAsDown)
            bg = bg.brighter (0.12f);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter (0.06f);

        g.setColour (bg);
        g.fillRoundedRectangle (bounds, corner);
    }

    // Default label font (slider value readouts + combo text): Poppins.
    juce::Font getLabelFont (juce::Label& label) override
    {
        juce::ignoreUnused (label);
        return uiFont (13.5f, 400);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return uiFont (buttonHeight >= 26 ? 14.0f : 13.0f, 600);
    }

    juce::Font getPopupMenuFont() override
    {
        return uiFont (12.5f, 500);
    }

private:
    juce::Colour accentColour_ { 0xffe07856 };
    bool lightTheme_ = true;
};
