#pragma once

#include <JuceHeader.h>
#include "CompTheme.h"

// ============================================================
// CrunchLookAndFeel — 桥接 JUCE LookAndFeel 到 UiPack 四包架构
//   · 消除全部硬编码色值，统一从 UiPack::UiSnapshot 解析语义颜色与圆角
//   · 默认挂载内嵌 Google Sans Flex / Noto Sans SC 字体
// ============================================================
class CrunchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CrunchLookAndFeel()
    {
        if (auto face = CompTheme::regular())
            setDefaultSansSerifTypeface (face);
        applyUiSnapshot (defaultComposition_.snapshot());
    }

    void applyUiSnapshot (const UiPack::UiSnapshot& snap)
    {
        pal_   = snap.palette;
        theme_ = snap.theme;

        setColour (juce::ResizableWindow::backgroundColourId,       pal_.bg);
        setColour (juce::Slider::backgroundColourId,                pal_.panel);
        setColour (juce::Slider::thumbColourId,                     pal_.accent);
        setColour (juce::Slider::trackColourId,                     pal_.track);
        setColour (juce::Slider::rotarySliderFillColourId,          pal_.accent);
        setColour (juce::Slider::rotarySliderOutlineColourId,       pal_.track);
        setColour (juce::Slider::textBoxTextColourId,               pal_.text);
        setColour (juce::Slider::textBoxBackgroundColourId,         pal_.bg.withAlpha (0.0f));
        setColour (juce::Slider::textBoxOutlineColourId,            pal_.bg.withAlpha (0.0f));
        setColour (juce::ComboBox::backgroundColourId,              pal_.surface);
        setColour (juce::ComboBox::textColourId,                    pal_.text);
        setColour (juce::ComboBox::outlineColourId,                 theme_.isNeumorphic ? pal_.bg.withAlpha (0.0f) : pal_.gridZ);
        setColour (juce::ComboBox::arrowColourId,                   pal_.accent);
        setColour (juce::ComboBox::buttonColourId,                  pal_.surface);
        setColour (juce::TextButton::buttonColourId,                pal_.track);
        setColour (juce::TextButton::buttonOnColourId,              pal_.accent);
        setColour (juce::TextButton::textColourOffId,               pal_.text);
        setColour (juce::TextButton::textColourOnId,                pal_.onAccent);
        setColour (juce::Label::textColourId,                       pal_.dim);
        setColour (juce::PopupMenu::backgroundColourId,             pal_.surface);
        setColour (juce::PopupMenu::textColourId,                  pal_.text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, pal_.accent);
        setColour (juce::PopupMenu::highlightedTextColourId,       pal_.onAccent);
    }

    const UiPack::PaletteTokens& palette() const noexcept { return pal_; }
    const UiPack::ThemeTokens& themeTokens() const noexcept { return theme_; }

    juce::Font getLabelFont (juce::Label&) override
    {
        return CompTheme::font (CompTheme::regular(), 13.0f * scale_);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return CompTheme::font (CompTheme::medium(), 13.0f * scale_);
    }

    juce::Font getPopupMenuFont() override
    {
        return CompTheme::font (CompTheme::regular(), 13.0f * scale_);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return CompTheme::font (CompTheme::medium(), 13.0f * scale_);
    }

    void setUiScale (float s) noexcept { scale_ = juce::jlimit (0.8f, 2.5f, s); }

private:
    UiPack::UiComposition defaultComposition_;
    UiPack::PaletteTokens pal_;
    UiPack::ThemeTokens   theme_;
    float scale_ = 1.0f;
};
