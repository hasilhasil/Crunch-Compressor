#pragma once

#include <JuceHeader.h>
#include <BinaryData.h>
#include "core/UiComposition.h"

// ============================================================
// CompTheme — 桥接门面（委托给 src/ui/core/ 四包正交架构）
//   · 双字体分离法则：
//       - font(...)      : 仅用于随语言包翻译的文本（zh-CN 下自动切换内嵌 SIL OFL 思源黑体并做 1.05x 字号补偿）
//       - latinFont(...) : 用于所有不需要翻译的英文、dB 刻度、旋钮数值读数与符号（始终保持 Google Sans Flex）
//       - brandTitleFont : 专用于左上角 CRUNCH COMP 品牌标识（不受语言包缩放影响）
// ============================================================
namespace CompTheme
{
    enum class Theme : int
    {
        Blue     = (int) UiPack::PaletteId::Blue,
        Red      = (int) UiPack::PaletteId::Red,
        Cream    = (int) UiPack::PaletteId::Cream,
        NeuLight = (int) UiPack::PaletteId::NeuLight,
        NeuTeal  = (int) UiPack::PaletteId::NeuTeal,
        NeuDark  = (int) UiPack::PaletteId::NeuDark
    };

    using Palette = UiPack::PaletteTokens;

    inline Palette palette (Theme t)
    {
        return UiPack::buildPalettePack ((UiPack::PaletteId) t);
    }

    inline juce::Typeface::Ptr regular()
    {
        static const juce::Typeface::Ptr face =
            juce::Typeface::createSystemTypefaceFor (BinaryData::GoogleSansFlexRegular_ttf,
                                                     BinaryData::GoogleSansFlexRegular_ttfSize);
        return face;
    }
    inline juce::Typeface::Ptr medium()
    {
        static const juce::Typeface::Ptr face =
            juce::Typeface::createSystemTypefaceFor (BinaryData::GoogleSansFlexMedium_ttf,
                                                     BinaryData::GoogleSansFlexMedium_ttfSize);
        return face;
    }
    inline juce::Typeface::Ptr semiBold()
    {
        static const juce::Typeface::Ptr face =
            juce::Typeface::createSystemTypefaceFor (BinaryData::GoogleSansFlexSemiBold_ttf,
                                                     BinaryData::GoogleSansFlexSemiBold_ttfSize);
        return face;
    }
    inline juce::Typeface::Ptr bold()
    {
        static const juce::Typeface::Ptr face =
            juce::Typeface::createSystemTypefaceFor (BinaryData::GoogleSansFlexBold_ttf,
                                                     BinaryData::GoogleSansFlexBold_ttfSize);
        return face;
    }

    inline juce::Typeface::Ptr cjkRegular()
    {
        static const juce::Typeface::Ptr face =
            juce::Typeface::createSystemTypefaceFor (BinaryData::NotoSansSCRegular_ttf,
                                                     BinaryData::NotoSansSCRegular_ttfSize);
        return face;
    }
    inline juce::Typeface::Ptr cjkMedium()
    {
        static const juce::Typeface::Ptr face =
            juce::Typeface::createSystemTypefaceFor (BinaryData::NotoSansSCMedium_ttf,
                                                     BinaryData::NotoSansSCMedium_ttfSize);
        return face;
    }
    inline juce::Typeface::Ptr cjkBold()
    {
        static const juce::Typeface::Ptr face =
            juce::Typeface::createSystemTypefaceFor (BinaryData::NotoSansSCBold_ttf,
                                                     BinaryData::NotoSansSCBold_ttfSize);
        return face;
    }

    inline juce::Typeface::Ptr uiRegular() { return regular(); }
    inline juce::Typeface::Ptr uiMedium()  { return medium(); }

    inline bool& cjkModeRef() noexcept { static bool cjk = false; return cjk; }
    inline void  setCjkMode (bool on) noexcept { cjkModeRef() = on; }
    inline bool  isCjkMode() noexcept { return cjkModeRef(); }

    inline constexpr float kUiTextScale = 1.0f;
    inline float& textScaleRef() { static float s = 1.0f; return s; }
    inline void  setTextScale (float s) { textScaleRef() = juce::jlimit (0.5f, 3.0f, s); }
    inline float textScale() { return textScaleRef(); }

    inline juce::Font font (juce::Typeface::Ptr face, float height, int style = juce::Font::plain)
    {
        float h = height * textScaleRef();
        juce::Typeface::Ptr primary = (face != nullptr) ? face : regular();

        if (cjkModeRef())
        {
            h = juce::jmax (12.6f, h * 1.05f);
            juce::Typeface::Ptr cjkFace = cjkRegular();
            if (primary == bold() || (style & juce::Font::bold) != 0)
                cjkFace = cjkBold();
            else if (primary == semiBold() || primary == medium())
                cjkFace = cjkMedium();

            if (cjkFace != nullptr)
                return juce::Font (juce::FontOptions (h, juce::Font::plain).withTypeface (cjkFace).withFallbackEnabled (true));
        }
        else
        {
            h = juce::jmax (6.0f, h);
        }

        if (primary != nullptr)
            return juce::Font (juce::FontOptions (h, style).withTypeface (primary));
        return juce::Font (juce::FontOptions (h, style));
    }

    /** 专用于不需要翻译的纯英文标签、dB 刻度、旋钮数值读数：始终保持原版 Google Sans Flex */
    inline juce::Font latinFont (juce::Typeface::Ptr face, float height, int style = juce::Font::plain)
    {
        const float h = juce::jmax (6.0f, height * textScaleRef());
        const juce::Typeface::Ptr primary = (face != nullptr) ? face : regular();
        if (primary != nullptr)
            return juce::Font (juce::FontOptions (h, style).withTypeface (primary));
        return juce::Font (juce::FontOptions (h, style));
    }

    /** 左上角 CRUNCH COMP 品牌标题专用字体 */
    inline juce::Font brandTitleFont (float height)
    {
        const float h = juce::jmax (6.0f, height * textScaleRef());
        const auto primary = uiMedium();
        if (primary != nullptr)
            return juce::Font (juce::FontOptions (h, juce::Font::plain).withTypeface (primary));
        return juce::Font (juce::FontOptions (h, juce::Font::plain));
    }

    /** 浅色底板上可读的警示/动态文字色（浅色压暗 45% 得到深琥珀色） */
    inline juce::Colour warnText (const Palette& pal) noexcept
    {
        return pal.light ? pal.accent4.darker (0.45f) : pal.accent4;
    }
} // namespace CompTheme

// 与 Hasil 插件家族共享控件（Knob / PackPopupOverlay）保持命名空间零成本兼容
namespace EQTheme = CompTheme;
namespace MBTheme = CompTheme;
