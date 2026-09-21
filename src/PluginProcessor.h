#pragma once

#include <JuceHeader.h>

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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // --- display API (safe to poll from the UI thread) ----------------------

    // Copies the most recent numPoints history entries (oldest first) into the
    // caller-provided buffers. One point is recorded every ~0.5 ms; the ring
    // holds ~16 s. Returns the number of points written (<= numPoints).
    // If counterOut is non-null it receives the SAME ring counter value the
    // copy was based on, so the UI can align its scroll position to the data
    // exactly instead of re-reading a (possibly newer) counter afterwards.
    int readHistory (int numPoints, float* inDb, float* outDb, float* grDb,
                     unsigned* counterOut = nullptr) const noexcept;

    // Ring write counter (history points recorded since prepareToPlay). The
    // UI polls it to know how far the newest data has advanced, so it can
    // render the scroll at a steady rate instead of stepping with the host's
    // irregular message-loop ticks.
    unsigned getHistoryCounter() const noexcept
    {
        return (unsigned) historyCounter_.load (std::memory_order_acquire);
    }

    // Static input->output transfer curve (dB) for the current parameters:
    // inDb[i] is linearly spaced from minDb to maxDb, outDb[i] = inDb[i] +
    // applied gain (incl. auto-gain makeup).
    void getTransferCurve (float minDb, float maxDb, int points,
                           std::vector<float>& inDb, std::vector<float>& outDb) const;

    float getInputPeakDb() const noexcept  { return inputPeakDb_.load(); }
    float getOutputPeakDb() const noexcept { return outputPeakDb_.load(); }
    float getGainReductionDb() const noexcept { return gainReduction_.load(); }

    int getTheme() const noexcept { return theme_.load(); }
    void setTheme (int theme) { theme_.store (juce::jlimit (0, 2, theme)); }
    juce::Colour getThemeAccent() const;
    bool isLightTheme() const noexcept { return theme_.load() == 2; }

    // True while the editor window is being dragged or resized. Hosts that
    // move the plugin editor as its own top-level window (FL Studio) send a
    // window-position change per mouse sample, each of which invalidates the
    // whole window; combined with the display's own 60 Hz repaints that
    // saturates the message loop and shows up as stutter and smearing while
    // dragging. The display suspends its animation repaints while this is set.
    bool isWindowDragging() const noexcept { return windowDragging_.load(); }
    void setWindowDragging (bool dragging) noexcept { windowDragging_.store (dragging); }

    int getEditorWidth() const noexcept  { return editorWidth_.load(); }
    int getEditorHeight() const noexcept { return editorHeight_.load(); }
    void setEditorSize (int w, int h)    { editorWidth_.store (w); editorHeight_.store (h); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void updateParameters();

    Compressor compressor_;
    ColorProcessor color_;

    // Level history ring (audio thread writes, UI thread reads): pre-allocated
    // in prepareToPlay, lock-free via an atomic write counter.
    static constexpr int kHistoryCapacity = 32768;   // ~16 s at 0.5 ms per point

    // Level recorded for the pre-roll the ring is seeded with in
    // prepareToPlay, so the display opens as a full flat line at the bottom
    // of the graph (silence) rather than filling up from the right.
    static constexpr float kIdleLevelDb = -100.0f;
    std::vector<float> histIn_, histOut_, histGr_;
    std::atomic<int> historyCounter_ { 0 };
    int historyStride_ = 24;             // samples between history points
    int historySampleCounter_ = 0;

    // Display envelopes: the raw level samples run at 2 kHz (waveform-rate),
    // which makes a per-column decimated graph "boil" while scrolling. The
    // graph records a ~4 ms envelope of the input/output levels instead, so
    // the scrolling animation is stable; meters keep using the block peaks.
    float displayInEnvDb_ = -100.0f;
    float displayOutEnvDb_ = -100.0f;
    float displayCoeff_ = 0.0f;

    std::atomic<float> inputPeakDb_ { -100.0f };
    std::atomic<float> outputPeakDb_ { -100.0f };
    std::atomic<float> gainReduction_ { 0.0f };
    std::atomic<int> theme_ { 0 };
    std::atomic<bool> windowDragging_ { false };
    std::atomic<int> editorWidth_ { 0 };
    std::atomic<int> editorHeight_ { 0 };

    // Typed parameter views. IMPORTANT: AudioProcessorValueTreeState raw
    // values are NORMALISED (0..1) and must never be fed to the DSP as
    // dB/ms/ratio/index values; always read the denormalised value here.
    juce::AudioParameterBool*    enabledParam_ = nullptr;
    juce::AudioParameterFloat*   inputGainParam_ = nullptr;
    juce::AudioParameterFloat*   outputGainParam_ = nullptr;
    juce::AudioParameterFloat*   thresholdParam_ = nullptr;
    juce::AudioParameterFloat*   ratioParam_ = nullptr;
    juce::AudioParameterFloat*   attackParam_ = nullptr;
    juce::AudioParameterFloat*   releaseParam_ = nullptr;
    juce::AudioParameterChoice*  kneeParam_ = nullptr;
    juce::AudioParameterChoice*  detectorParam_ = nullptr;
    juce::AudioParameterChoice*  lookaheadParam_ = nullptr;
    juce::AudioParameterChoice*  autoGainParam_ = nullptr;
    juce::AudioParameterChoice*  colorModeParam_ = nullptr;
    juce::AudioParameterFloat*   colorAmountParam_ = nullptr;
    juce::AudioParameterChoice*  colorPositionParam_ = nullptr;
    juce::AudioParameterFloat*   mixParam_ = nullptr;

    float cachedInGain_ = 0.0f, cachedOutGain_ = 0.0f;
    float cachedThreshold_ = 0.0f, cachedRatio_ = 0.0f;
    float cachedAttack_ = 0.0f, cachedRelease_ = 0.0f;
    float cachedKnee_ = -1.0f, cachedDetector_ = -1.0f;
    float cachedLookahead_ = -1.0f, cachedAutoGain_ = -1.0f, cachedEnabled_ = -1.0f;
    float cachedColorMode_ = -1.0f, cachedColorAmount_ = -1.0f;
    float cachedMix_ = Param::kMixDefault;
    bool cacheValid_ = false;

    // Dry/wet mix: the target comes from the parameter (cachedMix_ = 0..1);
    // mixSmoothed_ is a ~10 ms smoothed version applied per sample, so host
    // automation / knob drags do not produce zipper noise at the crossfade.
    float mixSmoothed_ = 1.0f;
    float mixSmoothCoeff_ = 0.0f;

    int reportedLatency_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrunchCompressorAudioProcessor)
};
