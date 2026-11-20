#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/CompTheme.h"

CrunchCompressorAudioProcessor::CrunchCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", Param::createParameterLayout())
{
    enabledParam_       = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter (Param::enabled));
    inputGainParam_     = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::inputGain));
    outputGainParam_    = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::outputGain));
    thresholdParam_     = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::threshold));
    ratioParam_         = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::ratio));
    attackParam_        = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::attack));
    releaseParam_       = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::release));
    kneeParam_          = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::knee));
    detectorParam_      = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::detector));
    lookaheadParam_     = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::lookahead));
    autoGainParam_      = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::autoGain));
    colorModeParam_     = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::colorMode));
    colorAmountParam_   = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::colorAmount));
    colorPositionParam_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::colorPosition));
    mixParam_           = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::mix));

    loadGlobalUiPrefs();

    abSlotState_[0]       = apvts.copyState();
    abSlotState_[1]       = apvts.copyState();
    abSlotColorHq_[0]     = false;
    abSlotColorHq_[1]     = false;
    abSlotInitialized_[0] = true;
    abSlotInitialized_[1] = true;
}

CrunchCompressorAudioProcessor::~CrunchCompressorAudioProcessor() = default;

bool CrunchCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void CrunchCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    compressor_.prepare (sampleRate, getTotalNumInputChannels());
    color_.prepare ((float) sampleRate, samplesPerBlock);
    color_.setHqEnabled (isColorHqEnabled());
    cacheValid_      = false;
    reportedLatency_ = -1;

    histIn_.assign  ((size_t) kHistoryCapacity, kIdleLevelDb);
    histOut_.assign ((size_t) kHistoryCapacity, kIdleLevelDb);
    histGr_.assign  ((size_t) kHistoryCapacity, 0.0f);
    historyStride_        = juce::jmax (1, (int) (sampleRate * 0.0005));
    historySampleCounter_ = 0;
    historyCounter_.store (kHistoryCapacity);

    displayCoeff_     = 1.0f - std::exp (-1.0f / (float) (sampleRate * 0.004));
    displayInEnvDb_   = kIdleLevelDb;
    displayOutEnvDb_  = kIdleLevelDb;

    mixSmoothCoeff_ = 1.0f - std::exp (-1.0f / (float) (sampleRate * 0.010));
    mixSmoothed_    = 1.0f;
}

void CrunchCompressorAudioProcessor::releaseResources()
{
    compressor_.reset();
    color_.reset();
    displayInEnvDb_  = -100.0f;
    displayOutEnvDb_ = -100.0f;
    detectorLevelDb_.store (-100.0f, std::memory_order_relaxed);
}

void CrunchCompressorAudioProcessor::updateParameters()
{
    const float inDb = inputGainParam_->get();
    if (! cacheValid_ || inDb != cachedInGain_) { cachedInGain_ = inDb; }

    const float outDb = outputGainParam_->get();
    if (! cacheValid_ || outDb != cachedOutGain_) { cachedOutGain_ = outDb; }

    const float threshold = thresholdParam_->get();
    const float ratio     = ratioParam_->get();
    const float attack    = attackParam_->get();
    const float release   = releaseParam_->get();
    const float knee      = (float) kneeParam_->getIndex();
    const float detector  = (float) detectorParam_->getIndex();
    const float lookahead = (float) lookaheadParam_->getIndex();
    const float autoGain  = (float) autoGainParam_->getIndex();

    if (! cacheValid_ || threshold != cachedThreshold_ || ratio != cachedRatio_
        || attack != cachedAttack_ || release != cachedRelease_ || knee != cachedKnee_
        || detector != cachedDetector_ || lookahead != cachedLookahead_ || autoGain != cachedAutoGain_)
    {
        cachedThreshold_ = threshold; cachedRatio_    = ratio;
        cachedAttack_    = attack;    cachedRelease_  = release;
        cachedKnee_      = knee;      cachedDetector_ = detector;
        cachedLookahead_ = lookahead; cachedAutoGain_ = autoGain;

        compressor_.setParameters (threshold, ratio, attack, release,
                                   (float) Param::kneeIndexToDb ((int) knee),
                                   Param::detectorIsRms ((int) detector),
                                   Param::lookaheadIndexToMs ((int) lookahead),
                                   autoGain != 0.0f);
    }

    const float enabled = enabledParam_->get() ? 1.0f : 0.0f;
    if (! cacheValid_ || enabled != cachedEnabled_)
        cachedEnabled_ = enabled;

    const float colorMode = (float) colorModeParam_->getIndex();
    if (! cacheValid_ || colorMode != cachedColorMode_)
    {
        cachedColorMode_ = colorMode;
        color_.setMode ((int) colorMode);
    }

    const float colorAmount = colorAmountParam_->get();
    if (! cacheValid_ || colorAmount != cachedColorAmount_)
    {
        cachedColorAmount_ = colorAmount;
        color_.setAmount (colorAmount / 100.0f);
    }

    const bool colorHq = isColorHqEnabled();
    if (! cacheValid_ || colorHq != cachedColorHq_)
    {
        cachedColorHq_ = colorHq;
        color_.setHqEnabled (colorHq);
    }

    const float mix = mixParam_->get();
    if (! cacheValid_ || mix != cachedMix_)
        cachedMix_ = mix;

    cacheValid_ = true;
}

void CrunchCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    updateParameters();

    const bool bypassed    = cachedEnabled_ < 0.5f;
    const int  wantLatency = ! bypassed ? (compressor_.getLookaheadSamples() + color_.getLatencySamples()) : 0;
    if (wantLatency != reportedLatency_)
    {
        setLatencySamples (wantLatency);
        reportedLatency_ = wantLatency;
    }

    const float inGain    = bypassed ? 1.0f : juce::Decibels::decibelsToGain (cachedInGain_);
    const float outGain   = bypassed ? 1.0f : juce::Decibels::decibelsToGain (cachedOutGain_);
    const float mixTarget = bypassed ? 1.0f : cachedMix_ * 0.01f;

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    const bool colourPre = (colorPositionParam_->getIndex() == 0);
    const bool compOn    = ! bypassed;

    const bool historyReady = histIn_.size()  >= (size_t) kHistoryCapacity
                           && histOut_.size() >= (size_t) kHistoryCapacity
                           && histGr_.size()  >= (size_t) kHistoryCapacity;

    float inPeak    = 0.0f, outPeak = 0.0f;
    float lastDetDb = -100.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        float l = left[s];
        float r = right != nullptr ? right[s] : l;

        const float dryL = l;
        const float dryR = r;

        const float a0 = std::abs (l);
        const float a1 = std::abs (r);
        if (a0 > inPeak) inPeak = a0;
        if (a1 > inPeak) inPeak = a1;

        l *= inGain;
        r *= inGain;

        if (colourPre && ! bypassed)
            color_.processSample (l, r);

        float inDb = 0.0f;
        if (compOn)
        {
            compressor_.processSample (l, r);
            inDb = compressor_.getLastDetectorDb();
        }
        else
        {
            inDb = juce::Decibels::gainToDecibels (std::max (std::abs (l), std::abs (r)) + 1.0e-9f);
        }
        lastDetDb = inDb;

        displayInEnvDb_ += (inDb - displayInEnvDb_) * displayCoeff_;

        if (! colourPre && ! bypassed)
            color_.processSample (l, r);

        const float outDbInstant = juce::Decibels::gainToDecibels (std::max (std::abs (l), std::abs (r)) + 1.0e-9f);
        displayOutEnvDb_ += (outDbInstant - displayOutEnvDb_) * displayCoeff_;

        l *= outGain;
        r *= outGain;

        mixSmoothed_ += (mixTarget - mixSmoothed_) * mixSmoothCoeff_;
        const float wet = mixSmoothed_;
        l = l * wet + dryL * (1.0f - wet);
        r = r * wet + dryR * (1.0f - wet);

        left[s] = l;
        if (right != nullptr)
            right[s] = r;

        const float a2 = std::abs (l);
        const float a3 = std::abs (r);
        if (a2 > outPeak) outPeak = a2;
        if (a3 > outPeak) outPeak = a3;

        if (historyReady && ++historySampleCounter_ >= historyStride_)
        {
            historySampleCounter_ = 0;
            const size_t idx = (size_t) ((unsigned) historyCounter_.load (std::memory_order_relaxed)
                                         % (unsigned) kHistoryCapacity);
            histIn_[idx]  = displayInEnvDb_;
            histOut_[idx] = displayOutEnvDb_;
            histGr_[idx]  = compOn ? compressor_.getGainReductionDb() : 0.0f;
            historyCounter_.fetch_add (1, std::memory_order_release);
        }
    }

    inputPeakDb_.store     (juce::Decibels::gainToDecibels (inPeak  + 1.0e-9f), std::memory_order_relaxed);
    outputPeakDb_.store    (juce::Decibels::gainToDecibels (outPeak + 1.0e-9f), std::memory_order_relaxed);
    gainReduction_.store   (compOn ? compressor_.getGainReductionDb() : 0.0f,   std::memory_order_relaxed);
    detectorLevelDb_.store (lastDetDb,                                          std::memory_order_relaxed);
}

int CrunchCompressorAudioProcessor::readHistory (int numPoints, float* inDb, float* outDb, float* grDb,
                                                 unsigned* counterOut) const noexcept
{
    if (inDb == nullptr || outDb == nullptr || grDb == nullptr)
        return 0;

    const unsigned counter = (unsigned) historyCounter_.load (std::memory_order_acquire);
    if (counterOut != nullptr)
        *counterOut = counter;

    const unsigned want = (unsigned) juce::jmin (juce::jmax (0, numPoints), kHistoryCapacity);
    const unsigned have = juce::jmin (want, counter);
    if (have == 0)
        return 0;

    const unsigned start = counter - have;
    unsigned idx = start % (unsigned) kHistoryCapacity;
    for (unsigned i = 0; i < have; ++i)
    {
        inDb[i]  = histIn_[(size_t) idx];
        outDb[i] = histOut_[(size_t) idx];
        grDb[i]  = histGr_[(size_t) idx];
        idx = (idx + 1 == (unsigned) kHistoryCapacity) ? 0u : idx + 1;
    }
    return (int) have;
}

void CrunchCompressorAudioProcessor::getTransferCurve (float minDb, float maxDb, int points,
                                                       std::vector<float>& inDb, std::vector<float>& outDb) const
{
    const int n = juce::jmax (1, points);
    inDb.resize ((size_t) n);
    outDb.resize ((size_t) n);

    // The UI can repaint before the audio thread has consumed the new APVTS values.
    // Build the static curve from the authoritative parameters, not the compressor cache.
    const float thresholdDb = thresholdParam_ != nullptr ? thresholdParam_->get() : Param::kThresholdDefault;
    const float ratio       = ratioParam_     != nullptr ? ratioParam_->get()     : Param::kRatioDefault;
    const float kneeDb      = kneeParam_      != nullptr ? (float) Param::kneeIndexToDb (kneeParam_->getIndex()) : 0.0f;
    const float makeupDb    = autoGainParam_  != nullptr && autoGainParam_->getIndex() != 0
                                ? compressor_.getMakeupDb() : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float t  = n > 1 ? (float) i / (float) (n - 1) : 0.0f;
        const float in = minDb + t * (maxDb - minDb);
        inDb[(size_t) i]  = in;
        outDb[(size_t) i] = in + Compressor::gainForLevel (in, thresholdDb, ratio, kneeDb) + makeupDb;
    }
}

// ---------------- 四包 UI 全局偏好持久化 ----------------

juce::File CrunchCompressorAudioProcessor::globalUiPrefsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Hasil")
        .getChildFile ("CrunchCompressor")
        .getChildFile ("ui_preferences.xml");
}

void CrunchCompressorAudioProcessor::loadGlobalUiPrefs() noexcept
{
    auto file = globalUiPrefsFile();
    if (! file.existsAsFile())
    {
        const auto legacy = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                .getChildFile ("Kilo")
                                .getChildFile ("CrunchCompressor")
                                .getChildFile ("ui_preferences.xml");
        if (legacy.existsAsFile())
            file = legacy;
        else
            return;
    }
    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (xml->hasTagName ("CrunchCompressorUiPrefs"))
        {
            UiPackSettings up;
            up.themeId   = juce::jlimit (0, 1, xml->getIntAttribute ("uiThemePack",   0));
            up.paletteId = juce::jlimit (0, 5, xml->getIntAttribute ("uiPalettePack", 0));
            up.layoutId  = juce::jlimit (0, 1, xml->getIntAttribute ("uiLayoutPack",  0));
            up.localeId  = juce::jlimit (0, 1, xml->getIntAttribute ("uiLocalePack",  0));
            uiPackSettings_ = up;
        }
    }
}

void CrunchCompressorAudioProcessor::saveGlobalUiPrefs() const noexcept
{
    const auto file = globalUiPrefsFile();
    file.getParentDirectory().createDirectory();
    juce::XmlElement xml ("CrunchCompressorUiPrefs");
    xml.setAttribute ("uiThemePack",   uiPackSettings_.themeId);
    xml.setAttribute ("uiPalettePack", uiPackSettings_.paletteId);
    xml.setAttribute ("uiLayoutPack",  uiPackSettings_.layoutId);
    xml.setAttribute ("uiLocalePack",  uiPackSettings_.localeId);
    xml.writeTo (file);
}

void CrunchCompressorAudioProcessor::setUiPackSettings (const UiPackSettings& s, bool persistGlobal) noexcept
{
    uiPackSettings_ = s;
    if (persistGlobal)
        saveGlobalUiPrefs();
}

void CrunchCompressorAudioProcessor::setTheme (int paletteIdx) noexcept
{
    uiPackSettings_.paletteId = juce::jlimit (0, 5, paletteIdx);
    const bool isNeu = (uiPackSettings_.paletteId >= 3);
    uiPackSettings_.themeId  = isNeu ? 1 : 0;
    uiPackSettings_.layoutId = isNeu ? 1 : 0;
    saveGlobalUiPrefs();
}

juce::Colour CrunchCompressorAudioProcessor::getThemeAccent() const
{
    return UiPack::buildPalettePack ((UiPack::PaletteId) juce::jlimit (0, 5, uiPackSettings_.paletteId)).accent;
}

bool CrunchCompressorAudioProcessor::isLightTheme() const noexcept
{
    return UiPack::buildPalettePack ((UiPack::PaletteId) juce::jlimit (0, 5, uiPackSettings_.paletteId)).light;
}

bool CrunchCompressorAudioProcessor::isPluginBypassed() const noexcept
{
    if (enabledParam_ != nullptr)
        return ! enabledParam_->get();
    if (auto* v = apvts.getRawParameterValue (Param::enabled))
        return v->load() < 0.5f;
    return false;
}

void CrunchCompressorAudioProcessor::setPluginBypassed (bool bypassed)
{
    if (auto* p = apvts.getParameter (Param::enabled))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (bypassed ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
}

// ---------------- 12 项出厂压缩预设与 A/B 对比 ----------------

void CrunchCompressorAudioProcessor::applyFactoryPreset (int index)
{
    currentPresetIndex_ = juce::jlimit (0, kNumFactoryPresets - 1, index);

    auto setFloat = [this] (const juce::String& id, float val)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (val));
    };
    auto setChoice = [this] (const juce::String& id, int idx)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
    };
    auto setBool = [this] (const juce::String& id, bool on)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (id)))
            p->setValueNotifyingHost (on ? 1.0f : 0.0f);
    };

    // Reset clean baseline
    setBool   (Param::enabled,       true);
    setFloat  (Param::inputGain,     0.0f);
    setFloat  (Param::outputGain,    0.0f);
    setFloat  (Param::threshold,     Param::kThresholdDefault);
    setFloat  (Param::ratio,         Param::kRatioDefault);
    setFloat  (Param::attack,        Param::kAttackDefault);
    setFloat  (Param::release,       Param::kReleaseDefault);
    setChoice (Param::knee,          1); // Soft
    setChoice (Param::detector,      0); // Peak
    setChoice (Param::lookahead,     0); // Off
    setChoice (Param::autoGain,      0); // Off
    setChoice (Param::colorMode,     0); // Off
    setFloat  (Param::colorAmount,   0.0f);
    setChoice (Param::colorPosition, 1); // Post
    setFloat  (Param::mix,           100.0f);
    setColorHqEnabled (false);

    switch (currentPresetIndex_)
    {
        case 0: // Default Clean
            break;

        case 1: // Gentle Bus Glue (2:1)
            setFloat  (Param::threshold, -16.0f);
            setFloat  (Param::ratio,      2.0f);
            setFloat  (Param::attack,    25.0f);
            setFloat  (Param::release,  180.0f);
            setChoice (Param::knee,       1);
            setChoice (Param::detector,   1); // RMS
            setChoice (Param::colorMode,  1); // Warm
            setFloat  (Param::colorAmount, 18.0f);
            break;

        case 2: // Transparent Leveler
            setFloat  (Param::threshold, -24.0f);
            setFloat  (Param::ratio,      2.5f);
            setFloat  (Param::attack,    15.0f);
            setFloat  (Param::release,  240.0f);
            setChoice (Param::knee,       1);
            setChoice (Param::detector,   1); // RMS
            setChoice (Param::lookahead,  1); // 1 ms
            setChoice (Param::autoGain,   1); // On
            break;

        case 3: // Warm Mastering Glue
            setFloat  (Param::threshold, -14.0f);
            setFloat  (Param::ratio,      2.0f);
            setFloat  (Param::attack,    30.0f);
            setFloat  (Param::release,  120.0f);
            setChoice (Param::knee,       1);
            setChoice (Param::detector,   1); // RMS
            setChoice (Param::colorMode,  1); // Warm
            setFloat  (Param::colorAmount, 22.0f);
            setColorHqEnabled (true);
            break;

        case 4: // Punchy Loudness
            setFloat  (Param::threshold, -18.0f);
            setFloat  (Param::ratio,      3.5f);
            setFloat  (Param::attack,    18.0f);
            setFloat  (Param::release,   85.0f);
            setChoice (Param::knee,       0); // Hard
            setChoice (Param::detector,   0); // Peak
            setChoice (Param::autoGain,   1); // On
            setChoice (Param::colorMode,  3); // Clip
            setFloat  (Param::colorAmount, 16.0f);
            setColorHqEnabled (true);
            break;

        case 5: // Parallel Crush Mix
            setFloat  (Param::threshold,   -32.0f);
            setFloat  (Param::ratio,        10.0f);
            setFloat  (Param::attack,        2.5f);
            setFloat  (Param::release,      90.0f);
            setChoice (Param::knee,          1);
            setChoice (Param::detector,      1); // RMS
            setFloat  (Param::mix,          45.0f);
            setChoice (Param::colorMode,     1); // Warm
            setChoice (Param::colorPosition, 0); // Pre
            setFloat  (Param::colorAmount,  30.0f);
            break;

        case 6: // Drum Bus Snap
            setFloat  (Param::threshold, -18.0f);
            setFloat  (Param::ratio,      4.5f);
            setFloat  (Param::attack,    22.0f);
            setFloat  (Param::release,   65.0f);
            setChoice (Param::knee,       0); // Hard
            setChoice (Param::detector,   0); // Peak
            setChoice (Param::colorMode,  3); // Clip
            setFloat  (Param::colorAmount, 24.0f);
            break;

        case 7: // Tight Sub & Bass
            setFloat  (Param::threshold,   -22.0f);
            setFloat  (Param::ratio,         6.0f);
            setFloat  (Param::attack,        8.0f);
            setFloat  (Param::release,     110.0f);
            setChoice (Param::knee,          1);
            setChoice (Param::detector,      1); // RMS
            setChoice (Param::lookahead,     1); // 1 ms
            setChoice (Param::colorMode,     1); // Warm
            setChoice (Param::colorPosition, 0); // Pre
            setFloat  (Param::colorAmount,  20.0f);
            break;

        case 8: // Fast Transient Tamer
            setFloat  (Param::threshold, -15.0f);
            setFloat  (Param::ratio,      8.0f);
            setFloat  (Param::attack,     0.4f);
            setFloat  (Param::release,   45.0f);
            setChoice (Param::knee,       0); // Hard
            setChoice (Param::detector,   0); // Peak
            setChoice (Param::lookahead,  2); // 3 ms
            break;

        case 9: // Upfront Vocal Control
            setFloat  (Param::threshold, -21.0f);
            setFloat  (Param::ratio,      3.8f);
            setFloat  (Param::attack,     6.0f);
            setFloat  (Param::release,  115.0f);
            setChoice (Param::knee,       1);
            setChoice (Param::detector,   0); // Peak
            setChoice (Param::autoGain,   1); // On
            setChoice (Param::colorMode,  1); // Warm
            setFloat  (Param::colorAmount, 15.0f);
            break;

        case 10: // Crisp Acoustic Sheen
            setFloat  (Param::threshold, -19.0f);
            setFloat  (Param::ratio,      2.8f);
            setFloat  (Param::attack,    14.0f);
            setFloat  (Param::release,  160.0f);
            setChoice (Param::knee,       1);
            setChoice (Param::detector,   1); // RMS
            setChoice (Param::colorMode,  2); // Cold
            setFloat  (Param::colorAmount, 18.0f);
            setColorHqEnabled (true);
            break;

        case 11: // Aggressive Grit Pump
        default:
            setFloat  (Param::threshold,   -28.0f);
            setFloat  (Param::ratio,        12.0f);
            setFloat  (Param::attack,        5.0f);
            setFloat  (Param::release,      55.0f);
            setChoice (Param::knee,          0); // Hard
            setChoice (Param::detector,      0); // Peak
            setChoice (Param::colorMode,     3); // Clip
            setChoice (Param::colorPosition, 0); // Pre
            setFloat  (Param::colorAmount,  38.0f);
            setFloat  (Param::mix,          75.0f);
            break;
    }

    cacheValid_ = false;
}

void CrunchCompressorAudioProcessor::toggleAbSlot()
{
    setAbSlot (activeAbSlot_ == 0);
}

void CrunchCompressorAudioProcessor::setAbSlot (bool useSlotB)
{
    const int targetSlot = useSlotB ? 1 : 0;
    if (targetSlot == activeAbSlot_)
        return;

    abSlotState_[activeAbSlot_]       = apvts.copyState();
    abSlotColorHq_[activeAbSlot_]     = isColorHqEnabled();
    abSlotInitialized_[activeAbSlot_] = true;

    activeAbSlot_ = targetSlot;
    if (abSlotInitialized_[activeAbSlot_] && abSlotState_[activeAbSlot_].isValid())
    {
        apvts.replaceState (abSlotState_[activeAbSlot_].createCopy());
        setColorHqEnabled (abSlotColorHq_[activeAbSlot_]);
    }
    else
    {
        abSlotState_[activeAbSlot_]       = apvts.copyState();
        abSlotColorHq_[activeAbSlot_]     = isColorHqEnabled();
        abSlotInitialized_[activeAbSlot_] = true;
    }
    cacheValid_ = false;
}

void CrunchCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        xml->setAttribute ("uiThemePack",      uiPackSettings_.themeId);
        xml->setAttribute ("uiPalettePack",    uiPackSettings_.paletteId);
        xml->setAttribute ("uiLayoutPack",     uiPackSettings_.layoutId);
        xml->setAttribute ("uiLocalePack",     uiPackSettings_.localeId);
        xml->setAttribute ("editorWidth",      getEditorWidth());
        xml->setAttribute ("editorHeight",     getEditorHeight());
        xml->setAttribute ("colorHqEnabled",   isColorHqEnabled());
        xml->setAttribute ("kneeCurveVisible", isKneeCurveVisible());
        xml->setAttribute ("presetIndex",      currentPresetIndex_);
        copyXmlToBinary (*xml, destData);
    }
}

void CrunchCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        if (xml->hasAttribute ("uiThemePack"))
        {
            UiPackSettings up;
            up.themeId   = juce::jlimit (0, 1, xml->getIntAttribute ("uiThemePack",   uiPackSettings_.themeId));
            up.paletteId = juce::jlimit (0, 5, xml->getIntAttribute ("uiPalettePack", uiPackSettings_.paletteId));
            up.layoutId  = juce::jlimit (0, 1, xml->getIntAttribute ("uiLayoutPack",  uiPackSettings_.layoutId));
            up.localeId  = juce::jlimit (0, 1, xml->getIntAttribute ("uiLocalePack",  uiPackSettings_.localeId));
            setUiPackSettings (up, false);
        }
        if (xml->hasAttribute ("editorWidth") && xml->hasAttribute ("editorHeight"))
            setEditorSize (xml->getIntAttribute ("editorWidth", 0),
                           xml->getIntAttribute ("editorHeight", 0));
        if (xml->hasAttribute ("colorHqEnabled"))
            setColorHqEnabled (xml->getBoolAttribute ("colorHqEnabled", false));
        if (xml->hasAttribute ("kneeCurveVisible"))
            setKneeCurveVisible (xml->getBoolAttribute ("kneeCurveVisible", true));
        if (xml->hasAttribute ("presetIndex"))
            currentPresetIndex_ = juce::jlimit (0, kNumFactoryPresets - 1, xml->getIntAttribute ("presetIndex", 0));

        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        abSlotState_[activeAbSlot_]       = apvts.copyState();
        abSlotColorHq_[activeAbSlot_]     = isColorHqEnabled();
        abSlotInitialized_[activeAbSlot_] = true;
        cacheValid_ = false;
    }
}

juce::AudioProcessorEditor* CrunchCompressorAudioProcessor::createEditor()
{
    return new CrunchCompressorAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrunchCompressorAudioProcessor();
}
