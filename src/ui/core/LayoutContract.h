#pragma once

#include <JuceHeader.h>
#include <array>

// ============================================================
// LayoutContract — 布局包（Layout Pack）命名槽位与几何契约
// ============================================================

namespace UiPack
{
    enum class LayoutId : int
    {
        Standard = 0,   // 默认全出血直角视窗与平面分栏布局
        Sculpted = 1    // 新拟物雕塑内凹圆角视窗与分舱卡片布局
    };

    inline constexpr int kNumLayouts = 2;

    struct LayoutTokens
    {
        juce::String id;
        juce::String name;

        float topBarHeight      = 46.0f;
        float bottomPanelHeight = 208.0f;   // 底部控制面板总高度（三舱主控台 + 底部全局胶囊条，px @ scale=1.0）
        float globalBarHeight   = 46.0f;    // 底部全局胶囊参数条高度（px @ scale=1.0）
        float meterWidth        = 44.0f;    // 右侧 IN/OUT 电平表栏宽度（px @ scale=1.0）
        float plotInsetX        = 0.0f;     // 中央视窗左右内缩边距（px @ scale=1.0）
        float plotInsetY        = 0.0f;     // 中央视窗上下内缩边距（px @ scale=1.0）
        float plotCornerRadius  = 0.0f;     // 中央内凹视窗圆角
        float deckGap           = 0.0f;     // 底部三舱卡片间距
        float deckInset         = 0.0f;     // 底部控制舱外边距
        bool  sunkenPlotWell    = false;    // 是否把中央画布绘制为内凹屏幕槽
    };
} // namespace UiPack
