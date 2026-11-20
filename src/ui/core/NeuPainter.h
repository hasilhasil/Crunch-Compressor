#pragma once

#include "PaletteContract.h"
#include "ThemeContract.h"

// ============================================================
// NeuPainter — 新拟物（Neumorphism）/ 经典平面（Classic）双模表面绘制原语
//
// 严格执行物理规则：
//   1. 固定左上光源（Fixed Illuminant）：
//      负偏移 (-X, -Y) 恒用 pal.shadowLight，正偏移 (+X, +Y) 恒用 pal.shadowDark
//   2. Hover Shadowing（悬停遮光）：
//      hovered 时使用 theme.shadowHover（偏移缩小），绝不放大阴影、绝不 translate 位移
//   3. Extrude to Intrude（外凸转内凹）：
//      pressed / active 时从外凸双阴影直接切换为内凹双阴影（inset），位置保持原位
//   4. Focus Relaxation（聚焦松弛）：
//      输入框 focused 时使用 theme.insetFocus（内凹深度收缩）
//   5. 零颜色字面量：所有色彩均从 PaletteTokens 语义槽位读取
// ============================================================

namespace UiPack
{
    struct NeuPainter
    {
        /** 绘制外凸（Extruded / Raised）圆角矩形：右下暗影 + 左上高光 + 同色系底面 */
        static void drawRaisedRect (juce::Graphics& g,
                                    juce::Rectangle<float> rect,
                                    float cornerRadius,
                                    const PaletteTokens& pal,
                                    const DualShadowSpec& spec,
                                    float scale = 1.0f,
                                    float opacity = 1.0f,
                                    bool fillAccent = false)
        {
            const float d    = spec.offset * scale;
            const float blur = spec.blur   * scale;
            const int   passes = 3;

            if (d > 0.1f && opacity > 0.01f)
            {
                constexpr float kNormSum = 1.555556f;
                for (int i = passes; i >= 1; --i)
                {
                    const float t      = (float) i / (float) passes;
                    const float fall   = (float) (passes + 1 - i) / (float) passes;
                    const float expand = blur * 0.44f * t;
                    const float shift  = d * (0.30f + 0.70f * t);
                    const float weight = (fall * fall) / kNormSum;

                    if (spec.lightAlpha > 0.001f)
                    {
                        g.setColour (pal.shadowLight.withMultipliedAlpha (spec.lightAlpha * weight * opacity));
                        g.fillRoundedRectangle (rect.translated (-shift * 0.88f, -shift * 0.88f).expanded (expand * 1.05f),
                                                cornerRadius + expand * 0.65f);
                    }
                    if (spec.darkAlpha > 0.001f)
                    {
                        g.setColour (pal.shadowDark.withMultipliedAlpha (spec.darkAlpha * weight * opacity));
                        g.fillRoundedRectangle (rect.translated (shift, shift).expanded (expand),
                                                cornerRadius + expand * 0.6f);
                    }
                }
            }

            g.setColour ((fillAccent ? pal.accent : pal.bg).withMultipliedAlpha (opacity));
            g.fillRoundedRectangle (rect, cornerRadius);

            if (! fillAccent)
            {
                juce::ColourGradient sheen (pal.bgLight.withMultipliedAlpha (0.15f * opacity),
                                            rect.getX(), rect.getY(),
                                            pal.bg.withMultipliedAlpha (0.0f),
                                            rect.getRight(), rect.getBottom(), false);
                g.setGradientFill (sheen);
                g.fillRoundedRectangle (rect, cornerRadius);
            }
        }

        /** 绘制内凹（Intruded / Inset）圆角矩形：精确圆角裁剪 + 左上暗影 + 右下高光 */
        static void drawInsetRect (juce::Graphics& g,
                                   juce::Rectangle<float> rect,
                                   float cornerRadius,
                                   const PaletteTokens& pal,
                                   const DualShadowSpec& spec,
                                   float scale = 1.0f,
                                   float opacity = 1.0f,
                                   bool useWellTint = false)
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (rect, cornerRadius);
            g.reduceClipRegion (clip);

            const juce::Colour base = useWellTint
                ? pal.bg.interpolatedWith (pal.track, 0.28f)
                : pal.bg;
            g.setColour (base.withMultipliedAlpha (opacity));
            g.fillRoundedRectangle (rect, cornerRadius);

            const float d    = juce::jmax (1.0f, spec.offset * scale);
            const float blur = juce::jmax (2.0f, spec.blur   * scale);
            const int   passes = 3;

            for (int i = 1; i <= passes; ++i)
            {
                const float t       = (float) i / (float) passes;
                const float shift   = d * t;
                const float strokeW = blur * 1.35f + shift * 2.0f;
                const float w       = (1.15f - 0.65f * t) / (float) passes;
                const auto  outer   = rect.expanded (strokeW * 0.5f);

                if (spec.darkAlpha > 0.001f)
                {
                    g.setColour (pal.insetDark.withMultipliedAlpha (spec.darkAlpha * w * opacity));
                    g.drawRoundedRectangle (outer.translated (shift, shift),
                                            cornerRadius + strokeW * 0.35f, strokeW);
                }
                if (spec.lightAlpha > 0.001f)
                {
                    g.setColour (pal.insetLight.withMultipliedAlpha (spec.lightAlpha * w * opacity));
                    g.drawRoundedRectangle (outer.translated (-shift, -shift),
                                            cornerRadius + strokeW * 0.35f, strokeW);
                }
            }
        }

        /** 绘制四个外角圆角反向遮罩与内凹边框（高帧率零 ID2D1Layer 方案） */
        static void drawRoundedWellCornerMaskAndRim (juce::Graphics& g,
                                                     juce::Rectangle<float> well,
                                                     float cornerRadius,
                                                     const PaletteTokens& pal,
                                                     const DualShadowSpec& spec,
                                                     float scale = 1.0f)
        {
            if (cornerRadius <= 0.5f)
                return;

            {
                juce::Path mask;
                mask.setUsingNonZeroWinding (false); // Even-Odd 奇偶规则：外框减圆角内框 = 四个角块
                mask.addRectangle (well.expanded (2.0f));
                mask.addRoundedRectangle (well, cornerRadius);
                g.setColour (pal.bg);
                g.fillPath (mask);
            }

            {
                juce::Graphics::ScopedSaveState ss (g);
                juce::Path clip;
                clip.addRoundedRectangle (well, cornerRadius);
                g.reduceClipRegion (clip);

                const float d    = juce::jmax (1.0f, spec.offset * scale);
                const float blur = juce::jmax (2.0f, spec.blur   * scale);
                const int   passes = 3;
                for (int i = 1; i <= passes; ++i)
                {
                    const float t       = (float) i / (float) passes;
                    const float shift   = d * t;
                    const float strokeW = blur * 1.35f + shift * 2.0f;
                    const float w       = (1.15f - 0.65f * t) / (float) passes;
                    const auto  outer   = well.expanded (strokeW * 0.5f);

                    if (spec.darkAlpha > 0.001f)
                    {
                        g.setColour (pal.insetDark.withMultipliedAlpha (spec.darkAlpha * w));
                        g.drawRoundedRectangle (outer.translated (shift, shift),
                                                cornerRadius + strokeW * 0.35f, strokeW);
                    }
                    if (spec.lightAlpha > 0.001f)
                    {
                        g.setColour (pal.insetLight.withMultipliedAlpha (spec.lightAlpha * w));
                        g.drawRoundedRectangle (outer.translated (-shift, -shift),
                                                cornerRadius + strokeW * 0.35f, strokeW);
                    }
                }
            }
        }

        /** 绘制按钮/胶囊（自动处理 Raised -> Hover Shadowing -> Pressed/Active Inset） */
        static void drawButton (juce::Graphics& g,
                                juce::Rectangle<float> rect,
                                const PaletteTokens& pal,
                                const ThemeTokens& theme,
                                bool activeOrPressed,
                                bool hovered,
                                float scale = 1.0f,
                                float opacity = 1.0f,
                                float customRadius = -1.0f)
        {
            const float radius = (customRadius >= 0.0f) ? customRadius : (theme.radiusMd * scale);
            if (! theme.isNeumorphic)
            {
                juce::Colour fill = activeOrPressed
                    ? pal.accent.withAlpha (0.24f)
                    : (hovered ? pal.track.brighter (0.12f) : pal.track);
                g.setColour (fill.withMultipliedAlpha (opacity));
                g.fillRoundedRectangle (rect, radius);
                if (theme.borderWidth > 0.0f)
                {
                    g.setColour ((activeOrPressed ? pal.accent : pal.gridZ).withMultipliedAlpha (opacity));
                    g.drawRoundedRectangle (rect, radius, theme.borderWidth * scale);
                }
                return;
            }

            if (activeOrPressed)
            {
                drawInsetRect (g, rect, radius, pal, theme.insetButton, scale, opacity, false);
                g.setColour (pal.accent.withMultipliedAlpha (0.14f * opacity));
                g.fillRoundedRectangle (rect.reduced (1.5f * scale), juce::jmax (2.0f, radius - 1.5f * scale));
            }
            else
            {
                const auto& spec = hovered ? theme.shadowHover : theme.shadowButton;
                drawRaisedRect (g, rect, radius, pal, spec, scale, opacity, false);
            }
        }

        /** 绘制紧凑按键组内的微凸键帽（用于内凹底座内的分段切换键与 A/B 滑块） */
        static void drawCompactCap (juce::Graphics& g,
                                    juce::Rectangle<float> rect,
                                    const PaletteTokens& pal,
                                    const ThemeTokens& theme,
                                    bool activeOrPressed,
                                    bool hovered,
                                    float scale = 1.0f,
                                    float opacity = 1.0f,
                                    float customRadius = -1.0f,
                                    bool fillAccentWhenActive = false)
        {
            const float radius = (customRadius >= 0.0f) ? customRadius : (theme.radiusSm * scale);
            if (! theme.isNeumorphic)
            {
                if (activeOrPressed)
                {
                    g.setColour ((fillAccentWhenActive ? pal.accent.withAlpha (0.88f)
                                                       : pal.accent.withAlpha (0.28f)).withMultipliedAlpha (opacity));
                    g.fillRoundedRectangle (rect, radius);
                    g.setColour (pal.accent.withAlpha (0.75f * opacity));
                    g.drawRoundedRectangle (rect, radius, 1.0f * scale);
                }
                else
                {
                    g.setColour ((hovered ? pal.track.brighter (0.12f) : pal.surfaceA).withMultipliedAlpha (opacity));
                    g.fillRoundedRectangle (rect, radius);
                    if (hovered)
                    {
                        g.setColour (pal.gridZ.withMultipliedAlpha (0.85f * opacity));
                        g.drawRoundedRectangle (rect, radius, 1.0f * scale);
                    }
                }
                return;
            }

            if (activeOrPressed)
            {
                if (fillAccentWhenActive)
                {
                    drawRaisedRect (g, rect, radius, pal, theme.shadowBadge, scale, opacity, true);
                }
                else
                {
                    drawInsetRect (g, rect, radius, pal, theme.insetFocus, scale, opacity, false);
                    g.setColour (pal.accent.withMultipliedAlpha (0.18f * opacity));
                    g.fillRoundedRectangle (rect.reduced (1.0f * scale), juce::jmax (1.5f, radius - 1.0f * scale));
                }
            }
            else
            {
                DualShadowSpec micro = hovered ? theme.shadowHover : theme.shadowBadge;
                micro.offset     *= 0.72f;
                micro.blur       *= 0.75f;
                micro.lightAlpha *= 0.80f;
                drawRaisedRect (g, rect, radius, pal, micro, scale, opacity, false);
            }
        }

        /** 绘制圆形外凸表面（用于新拟物旋钮帽与选中频点） */
        static void drawRaisedCircle (juce::Graphics& g,
                                      juce::Rectangle<float> circleBounds,
                                      const PaletteTokens& pal,
                                      const DualShadowSpec& spec,
                                      float scale = 1.0f)
        {
            const float r = circleBounds.getWidth() * 0.5f;
            drawRaisedRect (g, circleBounds, r, pal, spec, scale, 1.0f, false);
        }

        /** 绘制圆形内凹槽（用于旋钮外环底座） */
        static void drawInsetCircle (juce::Graphics& g,
                                     juce::Rectangle<float> circleBounds,
                                     const PaletteTokens& pal,
                                     const DualShadowSpec& spec,
                                     float scale = 1.0f)
        {
            const float r = circleBounds.getWidth() * 0.5f;
            drawInsetRect (g, circleBounds, r, pal, spec, scale, 1.0f, true);
        }
    };
} // namespace UiPack
