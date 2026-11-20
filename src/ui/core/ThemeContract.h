#pragma once

#include <JuceHeader.h>

// ============================================================
// ThemeContract — Theme 包（Shape & Surface Physics）契约
//
// 架构铁律：
//   · 本文件与所有 ThemePack 实现中 **严禁出现任何颜色字面量**
//     （禁止 0xff... / #hex / rgb / rgba / Colours::）
//   · 只定义圆角梯度、间距梯度、描边宽度、动效时长与双重阴影的几何度量
//   · 阴影与表面颜色一律在渲染时从当前激活的 PaletteTokens 语义槽位解析
// ============================================================

namespace UiPack
{
    enum class ThemeId : int
    {
        Classic     = 0,    // 默认平面毛玻璃皮肤
        Neumorphism = 1     // 新拟物双阴影凹凸物理皮肤
    };

    inline constexpr int kNumThemes = 2;

    /** 双重阴影几何表达式（左上负偏移亮高光 + 右下正偏移暗阴影，无颜色值）。 */
    struct DualShadowSpec
    {
        float offset     = 0.0f;   // 阴影偏移量（px）：正值表示右下暗影 (+d,+d) 与左上高光 (-d,-d)
        float blur       = 0.0f;   // 羽化半径（px）
        float darkAlpha  = 0.0f;   // 乘到 PaletteTokens::shadowDark / insetDark 的不透明度权重
        float lightAlpha = 0.0f;   // 乘到 PaletteTokens::shadowLight / insetLight 的不透明度权重
        bool  inset      = false;  // false = 外凸（extruded），true = 内凹（intruded）
    };

    struct ThemeTokens
    {
        juce::String id;
        juce::String name;
        bool isNeumorphic = false;

        // ---- 圆角梯度（px @ scale=1.0）----
        float radiusSm   = 4.0f;
        float radiusMd   = 6.0f;
        float radiusLg   = 12.0f;
        float radiusXl   = 16.0f;
        float radiusPill = 999.0f;

        // ---- 间距梯度（px @ scale=1.0）----
        float space1 = 4.0f;
        float space2 = 8.0f;
        float space3 = 12.0f;
        float space4 = 16.0f;
        float space6 = 24.0f;

        // ---- 边框与表面特征 ----
        float borderWidth     = 1.0f;
        float cardBorderAlpha = 0.55f;
        float surfaceSheen    = 0.0f;   // 135° 微光泽强度

        // ---- 8 级双重阴影深度阶梯 ----
        DualShadowSpec shadowBadge;     // 小徽章/胶囊
        DualShadowSpec shadowButton;    // 常规外凸按钮
        DualShadowSpec shadowHover;     // Hover Shadowing：手指靠近遮光，外阴影收缩
        DualShadowSpec shadowCard;      // 浮层卡片 / Popover / 旋钮帽
        DualShadowSpec insetButton;     // Active / Pressed：从外凸转为内凹，零位移
        DualShadowSpec insetInput;      // 输入框默认深凹槽
        DualShadowSpec insetFocus;      // 输入框聚焦松弛：内阴影收缩
        DualShadowSpec insetWell;       // 中央主视窗 / 电平表内凹槽

        // ---- 动效时间常数（ms）----
        double motionFastMs = 160.0;
        double motionBaseMs = 220.0;
    };
} // namespace UiPack
