#pragma once

#include <JuceHeader.h>
#include "CompTheme.h"
#include "Anim.h"

// ============================================================
// Knob — Hasil 通用四态旋钮家族控件（Classic / Neumorphism 自适应）
//   · 垂直拖动：220px 全行程（含对数 skew 对齐），Shift ×0.1 微调
//   · 双击：自绘内嵌数值输入（支持 "%" 等简写，绝不按空格弹出以免抢占 DAW 播放键）
//   · 滚轮：步进调节（Shift 微调，Ctrl ×5 加速）
//   · Ctrl + 单击 / Delete / Backspace：复位默认值
//   · 支持 THRESHOLD 同轨双层实时输入电平弧（青绿底弧 + 超阈值琥珀高亮层）
//   · BACK 层位图缓存 + OVERLAY 四态 Tween 动效（NORMAL / ACTIVE / DOWN / ROTATE）
// ============================================================
class Knob : public juce::Component
{
public:
    Knob()
    {
        setWantsKeyboardFocus (true);
    }

    void setRange (double minValue, double maxValue, double interval = 0.0);
    void setDefaultValue (double d)                     { default_ = juce::jlimit (min_, max_, d); }
    void setLabel (juce::String text)                   { label_ = std::move (text); repaint(); }
    void setValueSuffix (juce::String s)                { suffix_ = std::move (s); repaint(); }
    void setDecimals (int d)                            { decimals_ = d; repaint(); }
    void setAdaptiveMsDecimals (bool on)                { adaptiveMsDecimals_ = on; repaint(); }
    void setKnobDiameter (float px)                     { if (std::abs (knobDiameter_ - px) > 0.25f) { knobDiameter_ = px; invalidateBackLayer(); repaint(); } }
    void setShowLabel (bool s)                          { showLabel_ = s; repaint(); }
    void setBipolar (bool b)                            { bipolar_ = b; repaint(); }
    void setBipolarOrigin (double v)                    { bipolarOrigin_ = v; repaint(); }
    void setSkew (double centreValue)
    {
        skewCentre_ = centreValue;
        skewSet_    = true;
        updateSkew();
        repaint();
    }
    void setValuePrefix (juce::String p)                { valuePrefix_ = std::move (p); repaint(); }
    void setPalette (CompTheme::Palette p)              { pal_ = p; invalidateBackLayer(); repaint(); }
    void setThemeTokens (const UiPack::ThemeTokens& t)  { themeTokens_ = t; invalidateBackLayer(); repaint(); }
    void setSelectionFeedbackEnabled (bool enabled)     { selectionFeedback_ = enabled; }
    void setUiTextSizes (float label, float value)      { labelTextSize_ = label; valueTextSize_ = value; }
    void setAccentLabel (bool useAccent)                { accentLabel_ = useAccent; repaint(); }

    // ---- 同轨双层颜色叠加（用于 THRESHOLD 旋钮实时检测器输入电平显示）----
    void setTrackMeterEnabled (bool enabled)            { if (trackMeterEnabled_ != enabled) { trackMeterEnabled_ = enabled; repaint(); } }
    void setTrackMeterValue (double v)
    {
        if (std::abs (trackMeterValue_ - v) > 0.15)
        {
            trackMeterValue_ = v;
            repaint();
        }
    }

    bool isHoveredOrDragging() const noexcept           { return hovered_ || dragging_; }
    std::function<void (bool)> onHoverOrDragChanged;

    void focusGained (juce::Component::FocusChangeType) override { repaint(); }
    void focusLost (juce::Component::FocusChangeType) override   { if (editing_) closeEditor (true); repaint(); }

    void setValue (double v, bool sendNotification = true);
    double getValue() const                             { return value_; }
    double getNormalisedValue() const                   { return (double) toNorm (value_); }

    std::function<void (double)> onValueChange;
    std::function<void()> onDragStart, onDragEnd;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyPressedEditing (const juce::KeyPress&);

    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

    void advanceStates (double nowMs);
    bool isAnimating() const noexcept { return animating_; }
    void setEditCursorBlink (juce::uint32 nowMs);
    void refreshBackLayer() { invalidateBackLayer(); }

private:
    void commitValue (double v);
    void commitValueFine (double v);
    void openEditor();
    void closeEditor (bool apply);
    void notifyHoverOrDragIfChanged();
    float valueToAngle (double v) const;
    juce::Rectangle<float> knobBounds() const;
    float toNorm (double v) const;
    double fromNorm (double n) const;
    void updateSkew();
    juce::String formatReadout() const;

    double min_ = 0.0, max_ = 1.0, interval_ = 0.0;
    double value_ = 0.0, default_ = 0.0;
    double dragStartValue_ = 0.0;
    double dragStartNorm_  = 0.0;
    double bipolarOrigin_  = 0.0;
    float  skew_           = 1.0f;
    double skewCentre_     = 0.0;
    bool   skewSet_        = false;
    bool   hasSkew_        = false;
    int    decimals_           = 1;
    bool   adaptiveMsDecimals_ = false;
    float  knobDiameter_       = 48.0f;
    bool   showLabel_          = true;
    bool   bipolar_            = false;
    bool   accentLabel_        = false;

    bool   trackMeterEnabled_ = false;
    double trackMeterValue_   = -180.0;

    bool   dragging_       = false;
    bool   hovered_        = false;
    bool   lastActiveFlag_ = false;
    bool   mouseDownHeld_  = false;
    bool   animating_      = false;
    double lastNowMs_      = 0.0;
    Tween  hoverT_, downT_, rotateT_;

    juce::String label_, suffix_, valuePrefix_;
    CompTheme::Palette pal_ = CompTheme::palette (CompTheme::Theme::Blue);
    UiPack::ThemeTokens themeTokens_ = UiPack::buildThemePack (UiPack::ThemeId::Classic);

    bool editing_ = false;
    bool selectionFeedback_ = false;
    float labelTextSize_ = 0.0f, valueTextSize_ = 0.0f;
    juce::String editText_;
    bool editCursorOn_ = true;
    juce::uint32 editCursorLastToggleMs_ = 0;

    juce::Image backLayer_;
    float backLayerDiameter_ = 0.0f;
    void  buildBackLayer (float d);
    void  invalidateBackLayer() { backLayerDiameter_ = 0.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};
