#pragma once

#include "../../core/ThemeContract.h"
#include <array>

// ============================================================
// BuiltInThemes — 内置 Theme 包（Classic vs Neumorphism）
//   严格遵循零颜色字面量守卫：此处仅包含几何、圆角、阴影偏移与动效常数
// ============================================================

namespace UiPack
{
    inline ThemeTokens buildThemePack (ThemeId id)
    {
        ThemeTokens t;
        switch (id)
        {
            case ThemeId::Neumorphism:
                t.id              = "neumorphism";
                t.name            = "Neumorphism";
                t.isNeumorphic    = true;
                t.radiusSm        = 8.0f;
                t.radiusMd        = 12.0f;
                t.radiusLg        = 16.0f;
                t.radiusXl        = 20.0f;
                t.radiusPill      = 999.0f;
                t.space1          = 4.0f;
                t.space2          = 8.0f;
                t.space3          = 12.0f;
                t.space4          = 16.0f;
                t.space6          = 24.0f;
                t.borderWidth     = 0.0f;
                t.cardBorderAlpha = 0.0f;
                t.surfaceSheen    = 0.14f;
                t.shadowBadge     = { 2.2f,  5.0f, 0.68f, 0.44f, false };
                t.shadowButton    = { 3.5f,  8.0f, 0.74f, 0.48f, false };
                t.shadowHover     = { 2.0f,  5.0f, 0.64f, 0.40f, false };
                t.shadowCard      = { 6.0f, 14.0f, 0.78f, 0.28f, false };
                t.insetButton     = { 3.5f,  7.0f, 0.76f, 0.62f, true  };
                t.insetInput      = { 4.5f,  9.0f, 0.80f, 0.68f, true  };
                t.insetFocus      = { 2.0f,  4.0f, 0.62f, 0.56f, true  };
                t.insetWell       = { 6.5f, 13.0f, 0.82f, 0.72f, true  };
                t.motionFastMs    = 200.0;
                t.motionBaseMs    = 300.0;
                break;

            case ThemeId::Classic:
            default:
                t.id              = "classic";
                t.name            = "Classic Flat";
                t.isNeumorphic    = false;
                t.radiusSm        = 4.0f;
                t.radiusMd        = 6.0f;
                t.radiusLg        = 12.0f;
                t.radiusXl        = 16.0f;
                t.radiusPill      = 999.0f;
                t.space1          = 4.0f;
                t.space2          = 8.0f;
                t.space3          = 12.0f;
                t.space4          = 16.0f;
                t.space6          = 24.0f;
                t.borderWidth     = 1.0f;
                t.cardBorderAlpha = 0.55f;
                t.surfaceSheen    = 0.0f;
                t.shadowBadge     = { 0.0f,  0.0f, 0.0f,  0.0f,  false };
                t.shadowButton    = { 0.0f,  0.0f, 0.0f,  0.0f,  false };
                t.shadowHover     = { 0.0f,  0.0f, 0.0f,  0.0f,  false };
                t.shadowCard      = { 6.0f, 14.0f, 0.35f, 0.0f,  false };
                t.insetButton     = { 0.0f,  0.0f, 0.0f,  0.0f,  true  };
                t.insetInput      = { 0.0f,  0.0f, 0.0f,  0.0f,  true  };
                t.insetFocus      = { 0.0f,  0.0f, 0.0f,  0.0f,  true  };
                t.insetWell       = { 0.0f,  0.0f, 0.0f,  0.0f,  true  };
                t.motionFastMs    = 160.0;
                t.motionBaseMs    = 220.0;
                break;
        }
        return t;
    }

    inline const std::array<ThemeId, kNumThemes>& shippedThemeIds() noexcept
    {
        static const std::array<ThemeId, kNumThemes> ids = {
            ThemeId::Classic, ThemeId::Neumorphism
        };
        return ids;
    }
} // namespace UiPack
