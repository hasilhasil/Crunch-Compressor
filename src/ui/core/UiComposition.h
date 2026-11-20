#pragma once

#include "PaletteContract.h"
#include "ThemeContract.h"
#include "LayoutContract.h"
#include "LocaleRuntime.h"
#include "NeuPainter.h"
#include "../packs/palettes/BuiltInPalettes.h"
#include "../packs/themes/BuiltInThemes.h"
#include "../packs/layouts/BuiltInLayouts.h"
#include "../packs/locales/BuiltInLocales.h"

// ============================================================
// UiComposition — 四包正交组合中心（Candidate 校验门禁 + 原子切换 + 一键回滚）
//
//   1. 不可变快照（UiSnapshot）：聚合 { Theme, Palette, Layout, Locale } 与单调递增 revision
//   2. 候选门禁（verifySnapshot）：
//      · 检查色彩对比度与阴影槽位有效性
//      · 对 Neumorphism 主题强制校验物理禁忌：
//        禁止纯黑(#000000)/纯白(#ffffff)底色、禁止粗边框(border>=2px)、
//        强制圆角(radiusMd>=8px)、强制 Hover Shadowing 阴影收缩(shadowHover.offset <= shadowButton.offset)
//      · 检查双语字典完备性
//   3. 原子切换（applyPacks）：Theme 自动强绑定对应 Layout（Classic -> Standard，Neumorphism -> Sculpted）
//   4. 无条件回滚（rollback）：立即恢复到 previous_ 并递增 revision
// ============================================================

namespace UiPack
{
    struct VerificationVerdict
    {
        bool         ok = true;
        juce::String reason;
    };

    struct UiSnapshot
    {
        juce::uint32  revision  = 1;
        ThemeId       themeId   = ThemeId::Classic;
        PaletteId     paletteId = PaletteId::Blue;
        LayoutId      layoutId  = LayoutId::Standard;
        LocaleId      localeId  = LocaleId::EnUS;

        ThemeTokens   theme;
        PaletteTokens palette;
        LayoutTokens  layout;
        const LocalePack* locale = nullptr;

        juce::String t (const juce::String& key) const
        {
            if (locale != nullptr)
            {
                const auto it = locale->entries.find (key);
                if (it != locale->entries.end() && it->second.isNotEmpty())
                    return it->second;
            }
            return buildLocalePack (LocaleId::EnUS).get (key);
        }

        bool isNeumorphic() const noexcept { return theme.isNeumorphic; }
        bool isCjkLocale()  const noexcept { return locale != nullptr && locale->isCjk; }
    };

    class UiComposition
    {
    public:
        UiComposition()
        {
            current_  = makeSnapshot (ThemeId::Classic, PaletteId::Blue, LayoutId::Standard, LocaleId::EnUS, 1);
            previous_ = current_;
        }

        const UiSnapshot& snapshot() const noexcept { return current_; }
        const UiSnapshot& previousSnapshot() const noexcept { return previous_; }
        juce::uint32 revision() const noexcept { return current_.revision; }

        /** 验证任意候选组合（含新拟物物理规则门禁） */
        static VerificationVerdict verifySnapshot (const UiSnapshot& cand)
        {
            if (cand.palette.id.isEmpty() || cand.theme.id.isEmpty() || cand.layout.id.isEmpty())
                return { false, "missing pack identifier" };

            if (cand.palette.bg == cand.palette.text)
                return { false, "palette bg and text have zero contrast" };

            if (cand.locale == nullptr || cand.locale->entries.empty())
                return { false, "locale dictionary is empty" };

            if (cand.theme.isNeumorphic)
            {
                const auto bgRgb = cand.palette.bg.getARGB() & 0x00ffffffu;
                if (bgRgb == 0x000000u || bgRgb == 0xffffffu)
                    return { false, "neumorphism forbids pure black (#000000) or pure white (#ffffff) background" };
                if (cand.theme.borderWidth >= 2.0f)
                    return { false, "neumorphism forbids thick borders (border-2 or above)" };
                if (cand.theme.radiusMd < 8.0f)
                    return { false, "neumorphism requires rounded corners (radiusMd >= 8px)" };
                if (cand.theme.shadowHover.offset > cand.theme.shadowButton.offset)
                    return { false, "neumorphism hover shadow must shrink (Hover Shadowing), never grow" };
                if (cand.theme.insetFocus.offset > cand.theme.insetInput.offset)
                    return { false, "neumorphism input focus inset must relax (shrink), never deepen" };
                if (cand.palette.shadowDark == cand.palette.shadowLight)
                    return { false, "neumorphism dual shadows require distinct dark and light shadow tokens" };
            }

            return { true, {} };
        }

        static constexpr LayoutId boundLayoutForTheme (ThemeId t) noexcept
        {
            return (t == ThemeId::Neumorphism) ? LayoutId::Sculpted : LayoutId::Standard;
        }

        /** 原子替换四包组合：Theme 自动强绑定对应 Layout（Classic -> Standard，Neumorphism -> Sculpted） */
        VerificationVerdict applyPacks (ThemeId themeId,
                                        PaletteId paletteId,
                                        LayoutId /*ignoredLayoutId*/,
                                        LocaleId localeId)
        {
            const LayoutId effectiveLayout = boundLayoutForTheme (themeId);
            if (themeId == current_.themeId && paletteId == current_.paletteId
                && effectiveLayout == current_.layoutId && localeId == current_.localeId)
            {
                return { true, "unchanged" };
            }

            UiSnapshot cand = makeSnapshot (themeId, paletteId, effectiveLayout, localeId, nextRevision_);
            const auto verdict = verifySnapshot (cand);
            if (! verdict.ok)
                return verdict;

            previous_ = current_;
            current_  = cand;
            ++nextRevision_;
            return verdict;
        }

        VerificationVerdict setThemePack   (ThemeId t)   { return applyPacks (t, current_.paletteId, boundLayoutForTheme (t), current_.localeId); }
        VerificationVerdict setPalettePack (PaletteId p) { return applyPacks (current_.themeId, p, current_.layoutId, current_.localeId); }
        VerificationVerdict setLayoutPack  (LayoutId l)  { return applyPacks (l == LayoutId::Sculpted ? ThemeId::Neumorphism : ThemeId::Classic, current_.paletteId, l, current_.localeId); }
        VerificationVerdict setLocalePack  (LocaleId lc) { return applyPacks (current_.themeId, current_.paletteId, current_.layoutId, lc); }

        /** 一键应用新拟物套装（Neumorphism Theme + NeuLight/指定配色 + Sculpted 内凹视窗布局） */
        VerificationVerdict applyNeumorphismPreset (PaletteId pal = PaletteId::NeuLight)
        {
            return applyPacks (ThemeId::Neumorphism, pal, LayoutId::Sculpted, current_.localeId);
        }

        /** 一键应用经典套装（Classic Theme + Blue 配色 + Standard 布局） */
        VerificationVerdict applyClassicPreset (PaletteId pal = PaletteId::Blue)
        {
            return applyPacks (ThemeId::Classic, pal, LayoutId::Standard, current_.localeId);
        }

        /** 无条件原子回滚到上一次生效的四包快照 */
        void rollback() noexcept
        {
            UiSnapshot restored = previous_;
            restored.revision   = nextRevision_++;
            previous_           = current_;
            current_            = restored;
        }

    private:
        static UiSnapshot makeSnapshot (ThemeId t, PaletteId p, LayoutId l, LocaleId lc, juce::uint32 rev)
        {
            UiSnapshot s;
            s.revision  = rev;
            s.themeId   = t;
            s.paletteId = p;
            s.layoutId  = l;
            s.localeId  = lc;
            s.theme     = buildThemePack   (t);
            s.palette   = buildPalettePack (p);
            s.layout    = buildLayoutPack  (l);
            s.locale    = &buildLocalePack (lc);
            return s;
        }

        UiSnapshot   current_;
        UiSnapshot   previous_;
        juce::uint32 nextRevision_ = 2;
    };
} // namespace UiPack
