#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <functional>
#include <vector>

#include "../PluginProcessor.h"
#include "CompTheme.h"
#include "EditorLayout.h"
#include "Anim.h"
#include "PackPopupOverlay.h"

// ============================================================
// DisplayView — Crunch Compressor 上半区主画布（Pro-C 2 级示波器时间轴 + Hasil 四包正交顶栏/电平表）
//   · 100% 保留原版 Ring-Anchored 5s 滚动输入/输出包络 + 同标尺压缩红线 + 双层拐点传递曲线
//   · 叠加 Crunch EQ 同款 IGN 蓝噪声抖动谐波染色渐变底板（Warm / Cold / Clip）
//   · 悬停或拖拽底部 THRESHOLD 旋钮时，平滑浮现水平阈值参考虚线与 dB 标签
//   · Classic 直角全出血 / Neumorphism 16px 圆角内凹屏幕槽（单图缓存 wellRimCache_，零 ID2D1Layer 开销）
// ============================================================
class DisplayView : public juce::Component,
                    public juce::Timer
{
public:
    explicit DisplayView (CrunchCompressorAudioProcessor& processor);
    ~DisplayView() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    // 四包正交组合控制 API
    const UiPack::UiSnapshot& getUiSnapshot() const noexcept { return composition_.snapshot(); }
    void reloadUiPacksFromProcessor();
    UiPack::VerificationVerdict setUiPacks (UiPack::ThemeId t, UiPack::PaletteId p,
                                            UiPack::LayoutId l, UiPack::LocaleId lc);
    UiPack::VerificationVerdict setThemePack   (UiPack::ThemeId t);
    UiPack::VerificationVerdict setPalettePack (UiPack::PaletteId p);
    UiPack::VerificationVerdict setLayoutPack  (UiPack::LayoutId l);
    UiPack::VerificationVerdict setLocalePack  (UiPack::LocaleId lc);
    UiPack::VerificationVerdict applyNeumorphismPreset (UiPack::PaletteId pal = UiPack::PaletteId::NeuLight);
    UiPack::VerificationVerdict applyClassicPreset     (UiPack::PaletteId pal = UiPack::PaletteId::Blue);
    void rollbackUiPacks();

    void parametersChanged();
    void ensureTimerFullRate();
    void setThresholdGuideActive (bool active);
    void syncHistoryImmediateForSnapshot();

    // 外部回调（由 PluginEditor 注入）
    std::function<void()>         onCompositionChanged;
    std::function<void (double)>  onTickKnobs;
    std::function<bool()>         isExternalAnimating;
    std::function<void()>         onPresetApplied;

private:
    EditorLayout makeLayout() const;
    juce::Rectangle<float> plotWellRect() const;
    void applyCompositionSnapshot (bool persistToProcessor);

    void rebuildTransferCurveIfNeeded (const juce::Rectangle<float>& well);

    void drawBackground     (juce::Graphics& g, const EditorLayout& lay);
    void drawColourWash     (juce::Graphics& g, const EditorLayout& lay);
    void drawGrid           (juce::Graphics& g, const EditorLayout& lay);
    void drawHistoryGraph   (juce::Graphics& g, const EditorLayout& lay);
    void drawGrCurve        (juce::Graphics& g, const EditorLayout& lay);
    void drawTransferCurve  (juce::Graphics& g, const EditorLayout& lay);
    void drawThresholdGuide (juce::Graphics& g, const EditorLayout& lay, float guideAlpha);
    void drawScales         (juce::Graphics& g, const EditorLayout& lay);
    void drawMeters         (juce::Graphics& g, const EditorLayout& lay);
    void drawTopStrip       (juce::Graphics& g, const EditorLayout& lay);
    void drawPackSwitcher   (juce::Graphics& g, const EditorLayout& lay);
    void drawPresetMenu     (juce::Graphics& g, const EditorLayout& lay);

    void drawHairlineH (juce::Graphics& g, float y, float x0, float x1, juce::Colour c) const;
    void drawHairlineV (juce::Graphics& g, float x, float y0, float y1, juce::Colour c) const;

    int  hitTestChromeHoverId (juce::Point<float> pos, const EditorLayout& lay) const;
    void togglePackSwitcher();
    void closePackSwitcher();
    bool handlePackSwitcherClick (juce::Point<float> p, const EditorLayout& lay);

    juce::Rectangle<float> presetPrimaryRect     (const EditorLayout& lay) const;
    juce::Rectangle<float> presetSecondaryRect   (const EditorLayout& lay) const;
    juce::Rectangle<float> presetCategoryRowRect (const EditorLayout& lay, int cat) const;
    juce::Rectangle<float> presetItemRowRect     (const EditorLayout& lay, int itemInCat) const;
    juce::Rectangle<float> presetSaveAsRect      (const EditorLayout& lay) const;
    void openPresetMenu();
    void closePresetMenu();
    bool handlePresetMenuClick (juce::Point<float> p, const EditorLayout& lay);

    float grToY (float grDb, const juce::Rectangle<float>& well, float scale) const;
    float dbToY (float db, const juce::Rectangle<float>& well, float scale) const;
    float dbToX (float db, const juce::Rectangle<float>& well) const;
    float meterY (float db, const EditorLayout& lay) const;
    float historyEndOffset() const;

    // Ring-anchored sub-pixel decimated polyline builder (zero boiling/shimmer while scrolling).
    // Returns true if >= 2 points were emitted, writing outFirstX / outLastX for single-pass fill closure.
    template <typename MapY>
    bool appendDecimated (juce::Path& path, const float* values, int n, float endOffset,
                          const juce::Rectangle<float>& well, bool useMin,
                          float& outFirstX, float& outLastX, MapY mapY) const
    {
        const float plotX0 = well.getX();
        const float plotW  = well.getWidth();
        outFirstX = plotX0;
        outLastX  = plotX0 + plotW;
        if (n < 2 || endOffset < 1.0f || plotW <= 1.0f)
            return false;

        const float pxPerPoint = plotW / (float) (kHistoryPoints - 1);
        if (pxPerPoint <= 0.0f)
            return false;

        const double pointsPerPx = 1.0 / (double) pxPerPoint;
        const double rightIdx    = (double) endOffset;
        const double baseRing    = historyBase_;
        const double scrollRing  = baseRing + rightIdx;

        const double kMinD = std::ceil ((scrollRing - (double) (kHistoryPoints - 1)) / pointsPerPx - 0.5) - 1.0;
        const double kMaxD = std::floor (scrollRing / pointsPerPx - 0.5) + 1.0;

        const long long kMin      = (long long) kMinD;
        const long long kMax      = (long long) kMaxD;
        const long long nMinus1   = (long long) n - 1;
        const long long baseRingI = (long long) std::llround (baseRing);

        int emitted = 0;

        for (long long k = kMin; k <= kMax; ++k)
        {
            const double centreRing = ((double) k + 0.5) * pointsPerPx;

            const long long a0Raw = (long long) std::ceil ((double) k * pointsPerPx) - baseRingI;
            const long long a1Raw = (long long) std::ceil (((double) k + 1.0) * pointsPerPx) - 1 - baseRingI;

            if (a1Raw < 0 || a0Raw > nMinus1)
                continue;

            const long long a0 = juce::jmax (0LL, a0Raw);
            const long long a1 = juce::jmin (nMinus1, a1Raw);
            if (a0 > a1)
                continue;

            float v = values[(size_t) a0];
            for (long long i = a0 + 1; i <= a1; ++i)
                v = useMin ? juce::jmin (v, values[(size_t) i])
                           : juce::jmax (v, values[(size_t) i]);

            const float x = plotX0 + plotW - (float) ((scrollRing - centreRing) * (double) pxPerPoint);
            const float y = mapY (v);

            if (emitted == 0)
            {
                path.startNewSubPath (x, y);
                outFirstX = x;
            }
            else
            {
                path.lineTo (x, y);
            }
            outLastX = x;
            ++emitted;
        }
        return emitted >= 2;
    }

    CrunchCompressorAudioProcessor& processor_;
    UiPack::UiComposition composition_;

    // 5s 滚动历史缓冲（含 0.5s 左缘防截断余量）
    static constexpr int    kHistoryPoints         = 10000;
    static constexpr int    kHistoryReadPoints     = 11000;
    static constexpr double kScrollTimeConstantSec = 0.06;
    static constexpr float  kHistoryStrideMs       = 0.5f;
    static constexpr float  kGrHoldMs              = 50.0f;

    std::vector<float> histIn_, histOut_, histGr_;
    int    historyPoints_ = 0;
    double scrollCursor_  = -1.0;
    double historyBase_   = 0.0;
    double lastTickMs_    = 0.0;

    // 静态与动态传递曲线缓存
    std::vector<float> curveInDb_, curveOutDb_;
    juce::Path inCurve_, inFill_, outCurve_, outFill_, grPath_, livePath_;
    juce::Path transferBasePath_;
    bool  transferDirty_ = true;
    juce::Rectangle<float> transferWellBuilt_;

    // 稳态背景+网格位图缓存 + IGN 染色渐变缓存 + 新拟物圆角内凹视窗边框缓存
    juce::Image  staticPlotBgCache_;
    int          staticPlotBgW_        = 0;
    int          staticPlotBgH_        = 0;
    juce::uint32 staticPlotBgRev_      = 0;
    int          staticPlotStableHits_ = 0;

    juce::Image  colourWashCache_;
    float        colourWashAmount_ = -1000.0f;
    int          colourWashMode_   = -1;
    juce::uint32 colourWashRev_    = 0;
    int          colourWashW_      = 0;
    int          colourWashH_      = 0;

    juce::Image  wellRimCache_;
    juce::uint32 wellRimRev_ = 0;
    int          wellRimW_   = 0;
    int          wellRimH_   = 0;

    // 品牌宽度缓存
    float brandCrunchWidth_ = 0.0f;
    float brandCrunchScale_ = 0.0f;

    // 顶栏 A/B 滑块动画、阈值引导线淡入淡出与弹出层状态
    Tween abSlideTween_;
    Tween threshGuideTween_;
    bool  threshGuideTarget_   = false;
    bool  lastAbSlotB_         = false;
    bool  packMenuOpen_        = false;
    bool  presetMenuOpen_      = false;
    int   presetHoverCategory_ = 0;
    int   presetHoverItem_     = -1;
    bool  presetHoverSaveAs_   = false;
    int   lastHoveredChromeId_ = -1;
    juce::Point<float> lastMousePos_ { -100.0f, -100.0f };

    // 右侧 IN/OUT 电平表弹道（2000ms 峰值保持 + >0dB 纯红告警 + 250ms 读数节流）
    static constexpr int   kPeakHoldMs       = 2000;
    static constexpr float kPeakFallDbPerSec = 30.0f;
    float meterInDb_  = -60.0f, meterOutDb_  = -60.0f;
    float peakInDb_   = -60.0f, peakOutDb_   = -60.0f;
    float labelInDb_  = -60.0f, labelOutDb_  = -60.0f;
    juce::uint32 peakInHoldUntilMs_  = 0;
    juce::uint32 peakOutHoldUntilMs_ = 0;
    juce::uint32 nextMeterLabelMs_   = 0;
    juce::uint32 lastMeterMs_        = 0;

    void updatePeakHold (float& peakDb, juce::uint32& holdUntilMs,
                         float levelDb, juce::uint32 nowMs, double dt);

    // 帧间重绘收敛与 60Hz <-> 15Hz 动态空闲降频
    unsigned lastPaintedCounter_ = 0;
    double   lastPaintedCursor_  = -1.0;
    float    lastPaintedGr_      = -1000.0f;
    bool     lastPaintedKnee_    = true;

    static constexpr int kIdleHz         = 15;
    static constexpr int kIdleHoldFrames = 30;
    int  idleFrames_     = 0;
    bool uiStatePending_ = true;
    std::atomic<bool> paramChangedFlag_ { true };

    static constexpr float kGraphTopDb    = 0.0f;
    static constexpr float kGraphBottomDb = -60.0f;
    static constexpr float kAxisStepDb    = 6.0f;
    static constexpr float kMeterTopDb    = 6.0f;
    static constexpr float kMeterBottomDb = -60.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DisplayView)
};
