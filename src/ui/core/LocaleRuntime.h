#pragma once

#include <JuceHeader.h>
#include <array>
#include <unordered_map>

// ============================================================
// LocaleRuntime — 语言包契约与运行时
//   · 严格要求所有 SHIPPED_LANGUAGE_PACKS 的键集合 100% 对齐且非空
//   · 支持回退链（当前语言 -> en-US -> 键名本身）
// ============================================================

namespace UiPack
{
    enum class LocaleId : int
    {
        EnUS = 0,   // 默认英文
        ZhCN = 1    // 简体中文（自动启用 CJK 思源黑体排版）
    };

    inline constexpr int kNumLocales = 2;

    struct LocalePack
    {
        LocaleId     id = LocaleId::EnUS;
        juce::String code;
        juce::String label;
        bool         isCjk = false;
        std::unordered_map<juce::String, juce::String> entries;

        juce::String get (const juce::String& key) const
        {
            const auto it = entries.find (key);
            if (it != entries.end() && it->second.isNotEmpty())
                return it->second;
            return key;
        }
    };
} // namespace UiPack
