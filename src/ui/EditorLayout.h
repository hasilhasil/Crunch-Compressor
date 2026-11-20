#pragma once

#include <JuceHeader.h>
#include "core/LayoutContract.h"
#include "packs/layouts/BuiltInLayouts.h"

// ============================================================
// EditorLayout — Crunch Compressor 命名槽位几何计算器（驱动于 LayoutTokens 布局包）
//   · 窗口默认尺寸 1200 × 780，最小 920 × 580，最大 2080 × 1560（对齐 Hasil UI Skill）
//   · 划分五大正交区域：
//       1. 顶栏（Top Strip：品牌头 + 二级预设 + A|B 对比 + GR 实时遥测徽章 + 主题设置键）
//       2. 中央滚动波形、压缩红线与双层拐点曲线视窗（Plot Well）
//       3. 右侧 IN/OUT 电平表栏（Meter Strip）
//       4. 底部路线 A 三舱主控台（COLOUR + DYNAMICS + I/O·MIX）+ 底部 4 格 Global Bar
//       5. 二级弹出层（二级分类预设菜单、四包主题切换卡片、同心圆角自绘下拉）
// ============================================================
struct EditorLayout
{
    static constexpr int defaultWidth  = 1200;
    static constexpr int defaultHeight = 780;
    static constexpr int minWidth      = 920;
    static constexpr int minHeight     = 580;
    static constexpr int maxWidth      = 2080;
    static constexpr int maxHeight     = 1560;

    float width, height, scale;
    UiPack::LayoutTokens tokens;

    explicit EditorLayout (juce::Rectangle<int> editorBounds,
                           const UiPack::LayoutTokens& layoutTokens = UiPack::buildLayoutPack (UiPack::LayoutId::Standard))
        : width ((float) editorBounds.getWidth()),
          height ((float) editorBounds.getHeight()),
          scale (juce::jlimit (0.90f, 2.0f, juce::jmin (width / 1040.0f, height / 780.0f))),
          tokens (layoutTokens)
    {}

    float topHeight() const         { return tokens.topBarHeight * scale; }
    float bottomPanelHeight() const { return tokens.bottomPanelHeight * scale; }
    float bottomPanelTop() const    { return height - bottomPanelHeight(); }
    float globalBarHeight() const   { return tokens.globalBarHeight * scale; }
    float meterWidth() const        { return tokens.meterWidth * scale; }
    float plotRight() const         { return width - meterWidth(); }

    /** 编辑器上半部分（DisplayView 承载：顶栏 + 中央波形视窗 + 右侧电平表 + 弹出菜单） */
    juce::Rectangle<int> displayViewBounds() const
    {
        const int botH = juce::roundToInt (bottomPanelHeight());
        return { 0, 0, juce::roundToInt (width), juce::jmax (140, juce::roundToInt (height) - botH) };
    }

    /** 编辑器下半部分（BandControlPanel 承载：三舱旋钮主控台 + 底部全局胶囊条） */
    juce::Rectangle<int> bottomPanelBounds() const
    {
        const int botH = juce::roundToInt (bottomPanelHeight());
        const int topY = juce::jmax (140, juce::roundToInt (height) - botH);
        return { 0, topY, juce::roundToInt (width), juce::roundToInt (height) - topY };
    }

    /** 中央滚动波形与压缩红线视窗槽位（DisplayView 局部坐标系，Sculpted 布局下内缩为圆角内凹槽） */
    juce::Rectangle<float> plotWellRect (float displayHeight) const
    {
        const float ix = tokens.plotInsetX * scale;
        const float iy = tokens.plotInsetY * scale;
        return { ix,
                 topHeight() + iy,
                 juce::jmax (10.0f, plotRight() - ix * 2.0f),
                 juce::jmax (10.0f, (displayHeight - topHeight()) - iy * 2.0f) };
    }

    // ---- 顶栏（Top Strip）命名槽位 ----
    juce::Rectangle<float> theme() const   { return { plotRight() + 7.0f * scale, 8.0f * scale, 30.0f * scale, 30.0f * scale }; }
    juce::Rectangle<float> grBadge() const { return { plotRight() - 142.0f * scale, 8.0f * scale, 134.0f * scale, 30.0f * scale }; }

    /** 顶栏预设选择器槽位（居中于左上品牌标识与右侧 GR 遥测徽章之间） */
    juce::Rectangle<float> presetSelector() const
    {
        const float left  = 176.0f * scale;
        const float right = grBadge().getX() - 14.0f * scale;
        const float presW = 236.0f * scale;
        const float gap   = 10.0f  * scale;
        const float abW   = 66.0f  * scale;
        const float total = presW + gap + abW;
        const float start = juce::jmax (left, left + (right - left - total) * 0.5f);
        return { start, 8.0f * scale, presW, 30.0f * scale };
    }

    /** 顶栏 A | B 状态对比胶囊槽位 */
    juce::Rectangle<float> abCompareButton() const
    {
        const auto p = presetSelector();
        return { p.getRight() + 10.0f * scale, 8.0f * scale, 66.0f * scale, 30.0f * scale };
    }

    /** 右上角设置按钮展开的四包主题切换卡片 */
    juce::Rectangle<float> packSwitcherCard() const
    {
        const float w = 340.0f * scale;
        const float h = 292.0f * scale;
        const float x = juce::jmax (10.0f * scale, width - w - 8.0f * scale);
        const float y = topHeight() + 4.0f * scale;
        return { x, y, w, h };
    }

    juce::Rectangle<float> packOptionRect (int section, int optionIndex) const
    {
        const auto c = packSwitcherCard();
        const float pad = 12.0f * scale;
        const float innerW = c.getWidth() - pad * 2.0f;
        const float x0 = c.getX() + pad;
        const float y0 = c.getY();
        const float btnH = 24.0f * scale;

        if (section == 0)
        {
            const float cellW = (innerW - 6.0f * scale) * 0.5f;
            return { x0 + optionIndex * (cellW + 6.0f * scale), y0 + 36.0f * scale, cellW, 26.0f * scale };
        }
        if (section == 1)
        {
            const float cellW = (innerW - 6.0f * scale) * 0.5f;
            return { x0 + optionIndex * (cellW + 6.0f * scale), y0 + 82.0f * scale, cellW, btnH };
        }
        if (section == 2)
        {
            const int col = optionIndex % 3;
            const int row = optionIndex / 3;
            const float cellW = (innerW - 12.0f * scale) / 3.0f;
            return { x0 + col * (cellW + 6.0f * scale),
                     y0 + (128.0f + row * 28.0f) * scale,
                     cellW, btnH };
        }
        if (section == 4)
        {
            const float cellW = (innerW - 6.0f * scale) * 0.5f;
            return { x0 + optionIndex * (cellW + 6.0f * scale), y0 + 204.0f * scale, cellW, btnH };
        }
        if (section == 5)
            return { x0, y0 + 248.0f * scale, innerW, 28.0f * scale };

        return {};
    }

    // ---- 底部面板（BandControlPanel 局部坐标系）命名槽位 ----
    struct BottomSlots
    {
        juce::Rectangle<float> colourDeck;      // 左侧：COLOUR 谐波染色控制舱（含 4x HQ）
        juce::Rectangle<float> compDeck;        // 中央：DYNAMICS 压缩四大主旋钮 + Knee/Detector 滑块舱
        juce::Rectangle<float> ioDeck;          // 右侧：INPUT / MIX / OUTPUT 与 [AUTO] 增益舱
        juce::Rectangle<float> globalBar;       // 底部：4 槽位全局胶囊参数条
    };

    BottomSlots bottomSlots (float panelW, float panelH) const
    {
        BottomSlots bs;
        const float gBarH     = juce::jmin (globalBarHeight(), panelH * 0.28f);
        const float deckAreaH = juce::jmax (60.0f, panelH - gBarH);
        const float padX      = tokens.deckInset * scale;
        const float padY      = 6.0f * scale;
        const float gap       = tokens.deckGap * scale;

        bs.globalBar = { 0.0f, deckAreaH, panelW, gBarH };

        const float innerW = juce::jmax (120.0f, panelW - padX * 2.0f - gap * 2.0f);
        const float colW   = juce::jlimit (152.0f * scale, 196.0f * scale, innerW * 0.158f);
        const float ioW    = juce::jlimit (222.0f * scale, 278.0f * scale, innerW * 0.232f);
        const float compW  = juce::jmax   (320.0f * scale, innerW - colW - ioW);
        const float cardH  = juce::jmax   (40.0f, deckAreaH - padY * 2.0f);

        bs.colourDeck = { padX,                          padY, colW,  cardH };
        bs.compDeck   = { bs.colourDeck.getRight() + gap, padY, compW, cardH };
        bs.ioDeck     = { bs.compDeck.getRight()   + gap, padY, ioW,   cardH };
        return bs;
    }

    /** 底部全局参数条 3 个自适应权重胶囊槽位（LOOKAHEAD / CURVE / BYPASS） */
    static constexpr int kNumGlobalCells = 3;

    juce::Rectangle<float> globalCell (float panelW, float panelH, int index) const
    {
        const float weights[kNumGlobalCells] = { 1.35f, 1.35f, 0.92f };
        const float gBarH = juce::jmin (globalBarHeight(), panelH * 0.28f);
        const float topY  = panelH - gBarH;
        const float pad   = 10.0f * scale;
        const float gap   = 8.0f  * scale;
        float sumW = 0.0f;
        for (float w : weights) sumW += w;
        const float unit = juce::jmax (10.0f, (panelW - 2.0f * pad - 2.0f * gap) / sumW);
        float x = pad;
        const int idx = juce::jlimit (0, kNumGlobalCells - 1, index);
        for (int i = 0; i < idx; ++i)
            x += weights[i] * unit + gap;
        const float cellH = juce::jmin (32.0f * scale, gBarH - 10.0f * scale);
        return { x, topY + (gBarH - cellH) * 0.5f, weights[idx] * unit, cellH };
    }
};
