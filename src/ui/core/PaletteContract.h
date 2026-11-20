#pragma once

#include <JuceHeader.h>
#include <array>

// ============================================================
// PaletteContract — 配色包（Palette Pack）语义颜色槽位契约
//
// 架构纪律：
//   · 所有配色包实现完全相同的 PaletteTokens 语义槽位集合（等集契约）
//   · 组件与 Theme 包、NeuPainter 严禁硬编码颜色字面量，一律引用本契约槽位
//   · 同时提供新拟物双阴影槽位、谐波染色底板槽位与压缩波形语义槽位，
//     使任意 PalettePack 与任意 ThemePack 可正交混搭、原子替换
// ============================================================

namespace UiPack
{
    enum class PaletteId : int
    {
        Blue     = 0,
        Red      = 1,
        Cream    = 2,
        NeuLight = 3,   // 新拟物冷灰软塑底板 + 电光紫强调色
        NeuTeal  = 4,   // 新拟物冷灰底板 + 青绿强调色
        NeuDark  = 5    // 暗色新拟物深石板灰 + 霓虹紫强调色
    };

    inline constexpr int kNumPalettes = 6;

    struct PaletteTokens
    {
        juce::String id;
        juce::String name;

        // ---- 基础与表面槽位 ----
        juce::Colour bg;            // 主画布/机身底色
        juce::Colour bgLight;       // 次级浅亮表面
        juce::Colour panel;         // 侧栏/表头/控制面板底板
        juce::Colour surface;       // 实心卡片表面
        juce::Colour surfaceA;      // 顶/底栏半透明表面
        juce::Colour popA;          // 浮层面板表面
        juce::Colour track;         // 轨道/凹槽底色
        juce::Colour knob;          // 旋钮帽基色

        // ---- 文本与网格槽位 ----
        juce::Colour text;          // 主文本
        juce::Colour dim;           // 次级/静默文本
        juce::Colour grid;          // 次网格线
        juce::Colour gridZ;         // 0 dB 基准线 / 细描边

        // ---- 电平与压缩波形专属槽位 ----
        juce::Colour meterBg;        // 电平表凹槽底色
        juce::Colour inFill;         // 输入包络剪影 / 输入电平填充
        juce::Colour outLevel;       // 输出电平峰值 / 亮部
        juce::Colour waveOutline;    // 滚动输出包络 2px 轮廓线（暗底亮白 / 浅底深对比主题色）
        juce::Colour grTrace;        // 同标尺压缩增益衰减红线（GR Curve）
        juce::Colour transferActive; // 双层拐点传递曲线的动态滑行亮层

        // ---- 强调色与状态槽位 ----
        juce::Colour accent;        // 主强调色
        juce::Colour accent2;       // 告警色（纯红 #ff0000 过载告警）
        juce::Colour accent3;       // 第三强调色（青绿辅助状态 / 旋钮输入电平底弧）
        juce::Colour accent4;       // 第四强调色（暖金旋转反馈光 / 超阈值高亮弧）
        juce::Colour onAccent;      // 强调色表面上的文字色
        juce::Colour halo;          // 环境遮罩 / 光晕

        // ---- 谐波染色渐变底板槽位 ----
        juce::Colour washWarm;      // Warm 染色渐变（左上 -> 右下，橙）
        juce::Colour washCold;      // Cold 染色渐变（右上 -> 左下，蓝）
        juce::Colour washClip;      // Clip 染色渐变（下 -> 上，白 / 浅色主题石板灰）

        // ---- 新拟物双重阴影语义槽位 ----
        juce::Colour shadowDark;    // 外凸右下正偏移暗影
        juce::Colour shadowLight;   // 外凸左上负偏移高光
        juce::Colour insetDark;     // 内凹左上暗影
        juce::Colour insetLight;    // 内凹右下高光

        bool light = false;         // 是否为浅色系底板
    };
} // namespace UiPack
