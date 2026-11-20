#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <functional>

#include "../PluginProcessor.h"
#include "CompTheme.h"
#include "EditorLayout.h"
#include "Anim.h"
#include "Knob.h"
#include "PackPopupOverlay.h"

// ============================================================
// BandControlPanel — Crunch Compressor 路线 A 单层三舱主控台 + 底部 4 格全局胶囊参数条
//
//   · 上层路线 A 三舱：
//       1. 左舱 COLOUR   : 标题 + [Pre|Post] 滑块 + [HQ] 4x 抗混叠微发光开关 + AMOUNT 旋钮 + [Warm|Cold|Clip] 滑块
//       2. 中舱 DYNAMICS : 左栈双滑块（[Hard|Soft] 拐点 + [Peak|RMS] 检测器）+ 四大主旋钮（THRESHOLD 同轨电平弧 / RATIO / ATTACK / RELEASE）
//       3. 右舱 I/O·MIX  : INPUT / MIX / OUTPUT 三颗旋钮 + OUTPUT 正下方 [AUTO] 自动增益幽灵微徽章
//   · 下层全局条（Global Bar）：
//       单行 4 个自适应胶囊：LOOKAHEAD / CURVE / COLOUR HQ / BYPASS
// ============================================================
class BandControlPanel : public juce::Component,
                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit BandControlPanel (CrunchCompressorAudioProcessor& processor);
    ~BandControlPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void applyUiSnapshot (const UiPack::UiSnapshot& snap);
    void syncFromProcessor (bool snapSlides = false);

    void tickKnobs (double nowMs);
    bool isKnobAnimating() const noexcept;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    std::function<void (const juce::String& tag,
                        juce::Rectangle<float> anchorInPanel,
                        const juce::StringArray& items,
                        int currentIndex,
                        bool useTranslatedFont,
                        std::function<void (int)> onSelect)> onRequestPopup;
    std::function<void()>     onGlobalStateChanged;
    std::function<void (bool)> onThresholdFocusChanged;

private:
    EditorLayout makeLayout() const;
    EditorLayout::BottomSlots slots() const;

    struct StageGeometry
    {
        juce::Rectangle<float> leftStack;
        juce::Rectangle<float> heroCluster;
        float heroCellW = 100.0f;
        float heroDiam  = 58.0f;
    };
    StageGeometry computeStageGeometry (const juce::Rectangle<float>& cd, float s) const noexcept;

    // 左舱 COLOUR 矩形
    juce::Rectangle<float> colorPosTrayRect  (const juce::Rectangle<float>& deck, float s) const;
    juce::Rectangle<float> colorPosCellRect  (const juce::Rectangle<float>& deck, float s, int idx) const;
    juce::Rectangle<float> colorHqBtnRect    (const juce::Rectangle<float>& deck, float s) const;
    juce::Rectangle<float> colorModeTrayRect (const juce::Rectangle<float>& deck, float s) const;
    juce::Rectangle<float> colorModeCellRect (const juce::Rectangle<float>& deck, float s, int idx) const;

    // 中舱左控制栈（Knee & Detector 双滑块）矩形
    juce::Rectangle<float> kneeTrayRect      (const juce::Rectangle<float>& ls, float s) const;
    juce::Rectangle<float> kneeCellRect      (const juce::Rectangle<float>& ls, float s, int idx) const;
    juce::Rectangle<float> detectorTrayRect  (const juce::Rectangle<float>& ls, float s) const;
    juce::Rectangle<float> detectorCellRect  (const juce::Rectangle<float>& ls, float s, int idx) const;

    // 右舱 I/O·MIX 矩形
    juce::Rectangle<float> autoGainBtnRect   (const juce::Rectangle<float>& deck, float s) const;

    int hitTestHoverId (juce::Point<float> p) const;

    float getFloatParam  (const juce::String& id) const;
    int   getChoiceParam (const juce::String& id) const;
    void  setFloatParam  (const juce::String& id, float value);
    void  setChoiceParam (const juce::String& id, int index);
    void  beginGesture   (const juce::String& id);
    void  endGesture     (const juce::String& id);

    void layoutKnobs();

    void updateColorPosSlide  (int newIdx, bool snap);
    void updateColorModeSlide (int newMode, bool snap);
    void updateKneeSlide      (int newIdx, bool snap);
    void updateDetectorSlide  (int newIdx, bool snap);

    CrunchCompressorAudioProcessor& processor_;
    UiPack::UiSnapshot snap_ = UiPack::UiComposition().snapshot();

    // 8 颗自绘四态旋钮
    Knob colorAmountKnob_;
    Knob thresholdKnob_;
    Knob ratioKnob_;
    Knob attackKnob_;
    Knob releaseKnob_;
    Knob inKnob_;
    Knob mixKnob_;
    Knob outKnob_;

    // 4 组 cubic-bezier(0.22, 1, 0.36, 1) 分段滑动高光胶囊
    Tween colorPosSlideTween_;
    int   colorPosSlideFromIdx_  = 1;
    int   colorPosSlideToIdx_    = 1;

    Tween colorModeSlideTween_;
    int   colorModeSlideFromIdx_ = -1;
    int   colorModeSlideToIdx_   = -1;

    Tween kneeSlideTween_;
    int   kneeSlideFromIdx_      = 1;
    int   kneeSlideToIdx_        = 1;

    Tween detSlideTween_;
    int   detSlideFromIdx_       = 0;
    int   detSlideToIdx_         = 0;

    bool  segMoving_     = false;
    int   lastHoveredId_ = -1;
    bool  syncingKnobs_  = false;

    // 新拟物三舱静态底座单图缓存 + 60Hz 原子脏标记合并
    juce::Image       deckBackdropCache_;
    juce::uint32      deckBackdropRev_ = 0;
    int               deckBackdropW_   = 0;
    int               deckBackdropH_   = 0;
    std::atomic<bool> paramDirty_ { false };

    std::array<float, EditorLayout::kNumGlobalCells> globalLabelWidths_ {};
    float globalLabelScale_  = 0.0f;
    int   globalLabelLocale_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandControlPanel)
};
