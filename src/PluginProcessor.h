#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <vector>

#include "dsp/Compressor.h"
#include "dsp/ColorProcessor.h"
#include "Parameters.h"

class CrunchCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    CrunchCompressorAudioProcessor();
    ~CrunchCompressorAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::VST3ClientExtensions* getVST3ClientExtensions() override { return &vst3Extensions_; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // --- display & live telemetry API (lock-free, safe to poll from UI thread) ---

    int readHistory (int numPoints, float* inDb, float* outDb, float* grDb,
                     unsigned* counterOut = nullptr) const noexcept;

    unsigned getHistoryCounter() const noexcept
    {
        return (unsigned) historyCounter_.load (std::memory_order_acquire);
    }

    void getTransferCurve (float minDb, float maxDb, int points,
                           std::vector<float>& inDb, std::vector<float>& outDb) const;

    float getInputPeakDb() const noexcept     { return inputPeakDb_.load (std::memory_order_relaxed); }
    float getOutputPeakDb() const noexcept    { return outputPeakDb_.load (std::memory_order_relaxed); }
    float getGainReductionDb() const noexcept { return gainReduction_.load (std::memory_order_relaxed); }
    float getDetectorLevelDb() const noexcept { return detectorLevelDb_.load (std::memory_order_relaxed); }

    // ---------- 四包 UI 组合持久化（默认：Classic + Blue + Standard + EnUS） ----------
    struct UiPackSettings
    {
        int themeId   = 0;   // 0=Classic, 1=Neumorphism
        int paletteId = 0;   // 0=Blue, 1=Red, 2=Cream, 3=NeuLight, 4=NeuTeal, 5=NeuDark
        int layoutId  = 0;   // 0=Standard, 1=Sculpted
        int localeId  = 0;   // 0=en-US, 1=zh-CN
    };
    const UiPackSettings& getUiPackSettings() const noexcept { return uiPackSettings_; }
    void setUiPackSettings (const UiPackSettings& s, bool persistGlobal = true) noexcept;
    static juce::File globalUiPrefsFile();
    void loadGlobalUiPrefs() noexcept;
    void saveGlobalUiPrefs() const noexcept;

    // 兼容旧接口
    int getTheme() const noexcept { return uiPackSettings_.paletteId; }
    void setTheme (int paletteIdx) noexcept;
    juce::Colour getThemeAccent() const;
    bool isLightTheme() const noexcept;

    // ---------- 全局旁通（桥接 Param::enabled）+ Colour [HQ] 4x 母带抗混叠 + Knee 曲线显隐 ----------
    bool isPluginBypassed() const noexcept;
    void setPluginBypassed (bool bypassed);

    bool isColorHqEnabled() const noexcept { return colorHqEnabled_.load (std::memory_order_relaxed); }
    void setColorHqEnabled (bool enable) noexcept { colorHqEnabled_.store (enable, std::memory_order_relaxed); }
    int  getColorLatencySamples() const noexcept { return color_.getLatencySamples(); }

    bool isKneeCurveVisible() const noexcept { return kneeCurveVisible_.load (std::memory_order_relaxed); }
    void setKneeCurveVisible (bool visible) noexcept { kneeCurveVisible_.store (visible, std::memory_order_relaxed); }

    // ---------- 顶栏预设系统（4 分类 × 3 预设 = 12 项二级菜单预设）与 A/B 状态对比 ----------
    static constexpr int kNumPresetCategories = 4;
    static constexpr int kPresetsPerCategory  = 3;
    static constexpr int kNumFactoryPresets   = kNumPresetCategories * kPresetsPerCategory;

    int  getCurrentPresetIndex() const noexcept { return currentPresetIndex_; }
    void applyFactoryPreset (int index);

    bool isAbSlotB() const noexcept { return activeAbSlot_ == 1; }
    void toggleAbSlot();
    void setAbSlot (bool useSlotB);

    // ---------- 宿主窗口拖动/缩放模态避让 ----------
    bool isWindowDragging() const noexcept { return windowDragging_.load (std::memory_order_relaxed); }
    void setWindowDragging (bool dragging) noexcept { windowDragging_.store (dragging, std::memory_order_relaxed); }

    int getEditorWidth() const noexcept  { return editorWidth_.load (std::memory_order_relaxed); }
    int getEditorHeight() const noexcept { return editorHeight_.load (std::memory_order_relaxed); }
    void setEditorSize (int w, int h) noexcept
    {
        editorWidth_.store (w, std::memory_order_relaxed);
        editorHeight_.store (h, std::memory_order_relaxed);
    }

    juce::AudioProcessorValueTreeState apvts;

private:
    void updateParameters();

    struct LegacyVst3Extensions final : public juce::VST3ClientExtensions
    {
        std::vector<juce::String> getCompatibleClasses() const override
        {
            return { "56535443434D506372756E636820636F" };
        }
    };
    LegacyVst3Extensions vst3Extensions_;

    Compressor     compressor_;
    ColorProcessor color_;

    static constexpr int   kHistoryCapacity = 32768;   // ~16 s at 0.5 ms per point
    static constexpr float kIdleLevelDb     = -100.0f;
    std::vector<float> histIn_, histOut_, histGr_;
    std::atomic<int>   historyCounter_ { 0 };
    int historyStride_        = 24;
    int historySampleCounter_ = 0;

    float displayInEnvDb_  = -100.0f;
    float displayOutEnvDb_ = -100.0f;
    float displayCoeff_    = 0.0f;

    std::atomic<float> inputPeakDb_      { -100.0f };
    std::atomic<float> outputPeakDb_     { -100.0f };
    std::atomic<float> gainReduction_    { 0.0f };
    std::atomic<float> detectorLevelDb_  { -100.0f };
    std::atomic<bool>  colorHqEnabled_   { false };
    std::atomic<bool>  kneeCurveVisible_ { true };
    std::atomic<bool>  windowDragging_   { false };
    std::atomic<int>   editorWidth_      { 0 };
    std::atomic<int>   editorHeight_     { 0 };

    UiPackSettings uiPackSettings_;
    int currentPresetIndex_ = 0;
    int activeAbSlot_       = 0;
    std::array<juce::ValueTree, 2> abSlotState_;
    std::array<bool, 2>            abSlotColorHq_     { false, false };
    std::array<bool, 2>            abSlotInitialized_ { false, false };

    juce::AudioParameterBool*   enabledParam_       = nullptr;
    juce::AudioParameterFloat*  inputGainParam_     = nullptr;
    juce::AudioParameterFloat*  outputGainParam_    = nullptr;
    juce::AudioParameterFloat*  thresholdParam_     = nullptr;
    juce::AudioParameterFloat*  ratioParam_         = nullptr;
    juce::AudioParameterFloat*  attackParam_        = nullptr;
    juce::AudioParameterFloat*  releaseParam_       = nullptr;
    juce::AudioParameterChoice* kneeParam_          = nullptr;
    juce::AudioParameterChoice* detectorParam_      = nullptr;
    juce::AudioParameterChoice* lookaheadParam_     = nullptr;
    juce::AudioParameterChoice* autoGainParam_      = nullptr;
    juce::AudioParameterChoice* colorModeParam_     = nullptr;
    juce::AudioParameterFloat*  colorAmountParam_   = nullptr;
    juce::AudioParameterChoice* colorPositionParam_ = nullptr;
    juce::AudioParameterFloat*  mixParam_           = nullptr;

    float cachedInGain_ = 0.0f, cachedOutGain_ = 0.0f;
    float cachedThreshold_ = 0.0f, cachedRatio_ = 0.0f;
    float cachedAttack_ = 0.0f, cachedRelease_ = 0.0f;
    float cachedKnee_ = -1.0f, cachedDetector_ = -1.0f;
    float cachedLookahead_ = -1.0f, cachedAutoGain_ = -1.0f, cachedEnabled_ = -1.0f;
    float cachedColorMode_ = -1.0f, cachedColorAmount_ = -1.0f;
    bool  cachedColorHq_   = false;
    float cachedMix_       = Param::kMixDefault;
    bool  cacheValid_      = false;

    float mixSmoothed_    = 1.0f;
    float mixSmoothCoeff_ = 0.0f;

    int reportedLatency_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrunchCompressorAudioProcessor)
};
