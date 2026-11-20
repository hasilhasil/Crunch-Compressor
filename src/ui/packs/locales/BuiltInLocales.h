#pragma once

#include "../../core/LocaleRuntime.h"

// ============================================================
// BuiltInLocales — 内置双语语言包（en-US / zh-CN，键集 100% 对齐）
//   涵盖 Crunch Compressor 顶栏、三舱主控台、全局条、预设与四包切换卡片
// ============================================================

namespace UiPack
{
    inline const LocalePack& buildLocalePack (LocaleId id)
    {
        static const LocalePack enPack = [] {
            LocalePack p;
            p.id    = LocaleId::EnUS;
            p.code  = "en-US";
            p.label = "English";
            p.isCjk = false;
            p.entries = {
                // 顶栏
                { "top.grPrefix",         "GR " },

                // 左舱：谐波染色（COLOUR）
                { "color.section",        "COLOUR" },
                { "color.warm",           "Warm" },
                { "color.cold",           "Cold" },
                { "color.clip",           "Clip" },
                { "color.pre",            "Pre" },
                { "color.post",           "Post" },

                // 中舱：动态压缩（DYNAMICS）
                { "comp.section",         "DYNAMICS" },
                { "comp.hard",            "Hard" },
                { "comp.soft",            "Soft" },
                { "comp.peak",            "Peak" },
                { "comp.rms",             "RMS" },

                // 右舱：输入 / 混音 / 输出
                { "io.section",           juce::String::fromUTF8 ("I/O \xC2\xB7 MIX") },

                // 旋钮标题
                { "knob.amount",          "AMOUNT" },
                { "knob.threshold",       "THRESHOLD" },
                { "knob.ratio",           "RATIO" },
                { "knob.attack",          "ATTACK" },
                { "knob.release",         "RELEASE" },
                { "knob.inputGain",       "INPUT" },
                { "knob.mix",             "MIX" },
                { "knob.outputGain",      "OUTPUT" },

                // 底部全局参数条（Global Strip）
                { "global.lookahead",     "LOOKAHEAD" },
                { "global.laOff",         "Off" },
                { "global.kneeCurve",     "CURVE" },
                { "global.bypass",        "BYPASS" },
                { "global.on",            "Shown" },
                { "global.off",           "Hidden" },

                // 四包切换卡片
                { "pack.title",           "Theme" },
                { "pack.presetNeu",       "Neu Preset" },
                { "pack.presetClassic",   "Classic Preset" },
                { "pack.theme",           "THEME" },
                { "pack.palette",         "PALETTE" },
                { "pack.layout",          "LAYOUT" },
                { "pack.locale",          "LANGUAGE" },
                { "pack.rollback",        "Rollback Last Swap" },
                { "pack.theme.classic",   "Classic" },
                { "pack.theme.neu",       "Neumorphism" },
                { "pack.layout.standard", "Standard" },
                { "pack.layout.sculpted", "Sculpted" },

                // 顶栏二级分类预设（4 分类 × 3 项 = 12 项）
                { "preset.cat.0",         "Basic & Utility" },
                { "preset.cat.1",         "Mastering & Bus" },
                { "preset.cat.2",         "Drums & Bass" },
                { "preset.cat.3",         "Vocals & Mix" },
                { "preset.item.0",        "Default Clean" },
                { "preset.item.1",        "Gentle Bus Glue (2:1)" },
                { "preset.item.2",        "Transparent Leveler" },
                { "preset.item.3",        "Warm Mastering Glue" },
                { "preset.item.4",        "Punchy Loudness" },
                { "preset.item.5",        "Parallel Crush Mix" },
                { "preset.item.6",        "Drum Bus Snap" },
                { "preset.item.7",        "Tight Sub & Bass" },
                { "preset.item.8",        "Fast Transient Tamer" },
                { "preset.item.9",        "Upfront Vocal Control" },
                { "preset.item.10",       "Crisp Acoustic Sheen" },
                { "preset.item.11",       "Aggressive Grit Pump" },
                { "preset.saveAs",        "Save As..." }
            };
            return p;
        }();

        static const LocalePack zhPack = [] {
            LocalePack p;
            p.id    = LocaleId::ZhCN;
            p.code  = "zh-CN";
            p.label = juce::String::fromUTF8 ("\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87");
            p.isCjk = true;
            auto u8 = [] (const char* s) { return juce::String::fromUTF8 (s); };
            p.entries = {
                // 顶栏
                { "top.grPrefix",         u8 ("衰减 ") },

                // 左舱：谐波染色（COLOUR）
                { "color.section",        u8 ("谐波染色") },
                { "color.warm",           u8 ("温暖") },
                { "color.cold",           u8 ("冷峻") },
                { "color.clip",           u8 ("削波") },
                { "color.pre",            u8 ("前级") },
                { "color.post",           u8 ("后级") },

                // 中舱：动态压缩（DYNAMICS）
                { "comp.section",         u8 ("动态压缩") },
                { "comp.hard",            u8 ("硬拐点") },
                { "comp.soft",            u8 ("软拐点") },
                { "comp.peak",            u8 ("峰值") },
                { "comp.rms",             u8 ("均方根") },

                // 右舱：输入 / 混音 / 输出
                { "io.section",           u8 ("输出与混音") },

                // 旋钮标题
                { "knob.amount",          u8 ("染色量") },
                { "knob.threshold",       u8 ("阈值") },
                { "knob.ratio",           u8 ("压缩比") },
                { "knob.attack",          u8 ("起音") },
                { "knob.release",         u8 ("释音") },
                { "knob.inputGain",       u8 ("输入") },
                { "knob.mix",             u8 ("干湿比") },
                { "knob.outputGain",      u8 ("输出") },

                // 底部全局参数条（Global Strip）
                { "global.lookahead",     u8 ("前瞻") },
                { "global.laOff",         u8 ("关闭") },
                { "global.kneeCurve",     u8 ("传递曲线") },
                { "global.bypass",        u8 ("全局旁通") },
                { "global.on",            u8 ("显示") },
                { "global.off",           u8 ("隐藏") },

                // 四包切换卡片
                { "pack.title",           u8 ("主题") },
                { "pack.presetNeu",       u8 ("一键新拟物") },
                { "pack.presetClassic",   u8 ("一键经典") },
                { "pack.theme",           u8 ("主题包") },
                { "pack.palette",         u8 ("配色包") },
                { "pack.layout",          u8 ("布局包") },
                { "pack.locale",          u8 ("语言包") },
                { "pack.rollback",        u8 ("原子回滚上一次组合") },
                { "pack.theme.classic",   u8 ("经典平面") },
                { "pack.theme.neu",       u8 ("新拟物") },
                { "pack.layout.standard", u8 ("标准全出血") },
                { "pack.layout.sculpted", u8 ("新拟物内凹槽") },

                // 顶栏二级分类预设
                { "preset.cat.0",         u8 ("基础与通用") },
                { "preset.cat.1",         u8 ("母带与总线") },
                { "preset.cat.2",         u8 ("鼓组与低频") },
                { "preset.cat.3",         u8 ("人声与乐器") },
                { "preset.item.0",        u8 ("默认纯净初始化") },
                { "preset.item.1",        u8 ("温和总线粘合 (2:1)") },
                { "preset.item.2",        u8 ("透明自动稳压") },
                { "preset.item.3",        u8 ("温暖母带粘合") },
                { "preset.item.4",        u8 ("冲击感响度塑形") },
                { "preset.item.5",        u8 ("纽约式并联重压") },
                { "preset.item.6",        u8 ("鼓组总线清脆咬合") },
                { "preset.item.7",        u8 ("紧致超低与贝斯") },
                { "preset.item.8",        u8 ("极速瞬态驯服") },
                { "preset.item.9",        u8 ("人声贴耳动态控制") },
                { "preset.item.10",       u8 ("原声乐器清亮润色") },
                { "preset.item.11",       u8 ("激进染色抽吸") },
                { "preset.saveAs",        u8 ("另存为...") }
            };
            return p;
        }();

        return (id == LocaleId::ZhCN) ? zhPack : enPack;
    }

    inline const std::array<LocaleId, kNumLocales>& shippedLocaleIds() noexcept
    {
        static const std::array<LocaleId, kNumLocales> ids = {
            LocaleId::EnUS, LocaleId::ZhCN
        };
        return ids;
    }
} // namespace UiPack
