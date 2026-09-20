#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/LookAndFeel.h"

CrunchCompressorAudioProcessor::CrunchCompressorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", Param::createParameterLayout())
{
    enabledParam_     = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter (Param::enabled));
    inputGainParam_   = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::inputGain));
    outputGainParam_  = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::outputGain));
    thresholdParam_   = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::threshold));
    ratioParam_       = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::ratio));
    attackParam_      = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::attack));
    releaseParam_     = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::release));
    kneeParam_        = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::knee));
    detectorParam_    = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::detector));
    lookaheadParam_   = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::lookahead));
    autoGainParam_    = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::autoGain));
    colorModeParam_   = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::colorMode));
    colorAmountParam_ = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::colorAmount));
    colorPositionParam_ = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (Param::colorPosition));
    mixParam_        = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (Param::mix));
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
    juce::ignoreUnused (samplesPerBlock);

    compressor_.prepare (sampleRate, getTotalNumInputChannels());
    color_.prepare ((float) sampleRate);
    cacheValid_ = false;
    reportedLatency_ = -1;

    // Level history: one point per ~0.5 ms, ~16 s of ring capacity. The
    // buffers are allocated exactly once here and NEVER resized afterwards
    // (the UI thread reads them concurrently with the audio thread).
    // The ring is seeded with the idle level and the counter starts "full":
    // the display then has a complete window of history from the very first
    // UI tick, so it opens showing a full flat line (silence) instead of the
    // red/white curves slowly sweeping in from the right while it fills up.
    histIn_.assign  ((size_t) kHistoryCapacity, kIdleLevelDb);
    histOut_.assign ((size_t) kHistoryCapacity, kIdleLevelDb);
    histGr_.assign  ((size_t) kHistoryCapacity, 0.0f);
    historyStride_ = juce::jmax (1, (int) (sampleRate * 0.0005));
    historySampleCounter_ = 0;
    historyCounter_.store (kHistoryCapacity);

    // ~4 ms one-pole envelope for the scrolling display graph
    displayCoeff_ = 1.0f - std::exp (-1.0 / (sampleRate * 0.004));
    displayInEnvDb_ = kIdleLevelDb;
    displayOutEnvDb_ = kIdleLevelDb;

    // ~10 ms one-pole smoothing for the dry/wet crossfade
    mixSmoothCoeff_ = 1.0f - std::exp (-1.0 / (sampleRate * 0.010));
    mixSmoothed_ = 1.0f;
}

void CrunchCompressorAudioProcessor::releaseResources()
{
    compressor_.reset();
    color_.reset();
    displayInEnvDb_ = -100.0f;
    displayOutEnvDb_ = -100.0f;
}

void CrunchCompressorAudioProcessor::updateParameters()
{
    // NOTE: read denormalised values via the typed parameter views; the APVTS
    // raw values are normalised 0..1 and unusable directly.
    const float inDb = inputGainParam_->get();
    if (!cacheValid_ || inDb != cachedInGain_) { cachedInGain_ = inDb; }

    const float outDb = outputGainParam_->get();
    if (!cacheValid_ || outDb != cachedOutGain_) { cachedOutGain_ = outDb; }

    const float threshold = thresholdParam_->get();
    const float ratio     = ratioParam_->get();
    const float attack    = attackParam_->get();
    const float release   = releaseParam_->get();
    const float knee      = (float) kneeParam_->getIndex();
    const float detector  = (float) detectorParam_->getIndex();
    const float lookahead = (float) lookaheadParam_->getIndex();
    const float autoGain  = (float) autoGainParam_->getIndex();

    if (!cacheValid_ || threshold != cachedThreshold_ || ratio != cachedRatio_
        || attack != cachedAttack_ || release != cachedRelease_ || knee != cachedKnee_
        || detector != cachedDetector_ || lookahead != cachedLookahead_ || autoGain != cachedAutoGain_)
    {
        cachedThreshold_ = threshold; cachedRatio_ = ratio;
        cachedAttack_ = attack;       cachedRelease_ = release;
        cachedKnee_ = knee;           cachedDetector_ = detector;
        cachedLookahead_ = lookahead; cachedAutoGain_ = autoGain;

        compressor_.setParameters (threshold, ratio, attack, release,
                                   (float) Param::kneeIndexToDb ((int) knee),
                                   Param::detectorIsRms ((int) detector),
                                   Param::lookaheadIndexToMs ((int) lookahead),
                                   autoGain != 0.0f);
    }

    const float enabled = enabledParam_->get() ? 1.0f : 0.0f;
    if (!cacheValid_ || enabled != cachedEnabled_)
        cachedEnabled_ = enabled;

    const float colorMode = (float) colorModeParam_->getIndex();
    if (!cacheValid_ || colorMode != cachedColorMode_)
    {
        cachedColorMode_ = colorMode;
        color_.setMode ((int) colorMode);
    }

    const float colorAmount = colorAmountParam_->get();
    if (!cacheValid_ || colorAmount != cachedColorAmount_)
    {
        cachedColorAmount_ = colorAmount;
        color_.setAmount (colorAmount / 100.0f);
    }

    const float mix = mixParam_->get();
    if (!cacheValid_ || mix != cachedMix_)
        cachedMix_ = mix;

    cacheValid_ = true;
}

void CrunchCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    updateParameters();

    const int wantLatency = cachedEnabled_ >= 0.5f ? compressor_.getLookaheadSamples() : 0;
    if (wantLatency != reportedLatency_)
    {
        setLatencySamples (wantLatency);
        reportedLatency_ = wantLatency;
    }

    // Bypass makes the plugin fully transparent: no compressor, no colour,
    // no input/output gain. Meters/history still record so the display shows
    // the dry signal (in == out, GR = 0).
    const bool bypassed = cachedEnabled_ < 0.5f;

    const float inGain  = bypassed ? 1.0f : juce::Decibels::decibelsToGain (cachedInGain_);
    const float outGain = bypassed ? 1.0f : juce::Decibels::decibelsToGain (cachedOutGain_);
    const float mixTarget = bypassed ? 1.0f : cachedMix_ * 0.01f;

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    const bool colourPre = (colorPositionParam_->getIndex() == 0);
    const bool compOn    = ! bypassed;

    // The history ring must be allocated (see prepareToPlay) before any write;
    // if it somehow is not, skip recording rather than write out of bounds.
    const bool historyReady = histIn_.size()  >= (size_t) kHistoryCapacity
                           && histOut_.size() >= (size_t) kHistoryCapacity
                           && histGr_.size()  >= (size_t) kHistoryCapacity;

    float inPeak = 0.0f, outPeak = 0.0f;

    // Single per-sample pass: input gain -> colour(pre) -> compressor ->
    // colour(post) -> output gain -> dry/wet mix, while recording the level
    // history the display scrolls. "Input level" is what the detector sees
    // (post input gain); "output level" is pre output gain, so both match the
    // transfer curve the UI draws. The dry/wet crossfade uses the RAW input
    // (pre input gain) as the dry side, i.e. classic parallel mixing.
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

        // smooth the raw (2 kHz) level samples into a ~4 ms envelope; the
        // graph draws the envelope so its decimated path stays stable while
        // scrolling instead of boiling with per-sample waveform ripple
        displayInEnvDb_ += (inDb - displayInEnvDb_) * displayCoeff_;

        if (! colourPre && ! bypassed)
            color_.processSample (l, r);

        const float outDbInstant = juce::Decibels::gainToDecibels (std::max (std::abs (l), std::abs (r)) + 1.0e-9f);
        displayOutEnvDb_ += (outDbInstant - displayOutEnvDb_) * displayCoeff_;

        l *= outGain;
        r *= outGain;

        // Dry/wet: wet = processed chain (post output gain), dry = raw input.
        // The crossfade amount is smoothed so automated changes click-free.
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
            histGr_[idx] = compOn ? compressor_.getGainReductionDb() : 0.0f;
            historyCounter_.fetch_add (1, std::memory_order_release);
        }
    }

    inputPeakDb_.store (juce::Decibels::gainToDecibels (inPeak + 1.0e-9f));
    outputPeakDb_.store (juce::Decibels::gainToDecibels (outPeak + 1.0e-9f));
    gainReduction_.store (compOn ? compressor_.getGainReductionDb() : 0.0f);
}

int CrunchCompressorAudioProcessor::readHistory (int numPoints, float* inDb, float* outDb, float* grDb,
                                                 unsigned* counterOut) const noexcept
{
    if (inDb == nullptr || outDb == nullptr || grDb == nullptr)
        return 0;

    // unsigned arithmetic keeps this wrap-safe for the lifetime of the process
    const unsigned counter = (unsigned) historyCounter_.load (std::memory_order_acquire);
    if (counterOut != nullptr)
        *counterOut = counter;

    const unsigned want    = (unsigned) juce::jmin (juce::jmax (0, numPoints), kHistoryCapacity);
    const unsigned have    = juce::jmin (want, counter);
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

    for (int i = 0; i < n; ++i)
    {
        const float t = n > 1 ? (float) i / (float) (n - 1) : 0.0f;
        const float in = minDb + t * (maxDb - minDb);
        inDb[(size_t) i] = in;
        outDb[(size_t) i] = in + compressor_.getAppliedGainDb (in);
    }
}

juce::Colour CrunchCompressorAudioProcessor::getThemeAccent() const
{
    return CrunchLookAndFeel::accentForTheme (theme_.load());
}

void CrunchCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CrunchCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CrunchCompressorAudioProcessor::createEditor()
{
    return new CrunchCompressorAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrunchCompressorAudioProcessor();
}
