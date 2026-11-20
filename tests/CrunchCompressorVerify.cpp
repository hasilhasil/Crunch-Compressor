#include <JuceHeader.h>

#include <cmath>
#include <iostream>
#include <memory>

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/core/UiComposition.h"

namespace
{
    int gPassed = 0;
    int gFailed = 0;

    void check (bool cond, const juce::String& name, const juce::String& detail = {})
    {
        if (cond)
        {
            ++gPassed;
            std::cout << "  [PASS] " << name;
            if (detail.isNotEmpty())
                std::cout << "  (" << detail << ")";
            std::cout << "\n";
        }
        else
        {
            ++gFailed;
            std::cout << "  [FAIL] " << name;
            if (detail.isNotEmpty())
                std::cout << "  (" << detail << ")";
            std::cout << "\n";
        }
    }

    void setFloat (CrunchCompressorAudioProcessor& p, const juce::String& id, float val)
    {
        if (auto* prm = dynamic_cast<juce::AudioParameterFloat*> (p.apvts.getParameter (id)))
            prm->setValueNotifyingHost (prm->convertTo0to1 (val));
    }

    void setChoice (CrunchCompressorAudioProcessor& p, const juce::String& id, int idx)
    {
        if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (id)))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) idx));
    }

    void testFourPackAndLocales()
    {
        std::cout << "\n=== 1. Four-Pack Orthogonal Composition & Locale Key Parity ===\n";

        const auto& en = UiPack::buildLocalePack (UiPack::LocaleId::EnUS);
        const auto& zh = UiPack::buildLocalePack (UiPack::LocaleId::ZhCN);

        bool parityOk = (en.entries.size() == zh.entries.size()) && ! en.entries.empty();
        for (const auto& kv : en.entries)
        {
            const auto it = zh.entries.find (kv.first);
            if (it == zh.entries.end() || kv.second.isEmpty() || it->second.isEmpty())
                parityOk = false;
        }
        check (parityOk, "en-US and zh-CN dictionaries have 100% key parity and non-empty strings",
               "keys=" + juce::String ((int) en.entries.size()));

        UiPack::UiComposition comp;
        bool all24Ok = true;
        int tested = 0;
        for (auto t : UiPack::shippedThemeIds())
        {
            for (auto p : UiPack::shippedPaletteIds())
            {
                for (auto lc : UiPack::shippedLocaleIds())
                {
                    const auto v = comp.applyPacks (t, p, UiPack::UiComposition::boundLayoutForTheme (t), lc);
                    if (! v.ok)
                        all24Ok = false;
                    ++tested;
                }
            }
        }
        check (all24Ok && tested == 24, "All 24 (Theme x Palette x Locale) combinations pass UiComposition::verifySnapshot",
               "tested=" + juce::String (tested));

        comp.applyNeumorphismPreset (UiPack::PaletteId::NeuLight);
        const auto revBefore = comp.revision();
        comp.applyClassicPreset (UiPack::PaletteId::Red);
        comp.rollback();
        check (comp.snapshot().themeId == UiPack::ThemeId::Neumorphism
               && comp.snapshot().paletteId == UiPack::PaletteId::NeuLight
               && comp.revision() > revBefore,
               "UiComposition::rollback() restores previous snapshot atomically");
    }

    void testColourModuleAndLatency()
    {
        std::cout << "\n=== 2. Migrated Crunch EQ ColorProcessor (1x + 4x Linear HQ + Crossfade) ===\n";

        // 2a. Exact 18-sample integer impulse delay in 4x HQ mode
        {
            ColorProcessor col;
            col.prepare (48000.0f, 512);
            col.setMode (0);
            col.setAmount (0.0f);
            col.setHqEnabled (true);

            check (col.getLatencySamples() == ColorProcessor::kHqLatencySamples,
                   "ColorProcessor HQ mode reports exact 18-sample linear-phase FIR latency",
                   "latency=" + juce::String (col.getLatencySamples()));

            float peakVal = 0.0f;
            int   peakIdx = -1;
            for (int n = 0; n < 64; ++n)
            {
                float l = (n == 0) ? 1.0f : 0.0f;
                float r = l;
                col.processSample (l, r);
                if (std::abs (l) > peakVal)
                {
                    peakVal = std::abs (l);
                    peakIdx = n;
                }
            }
            check (peakIdx == ColorProcessor::kHqLatencySamples && std::abs (peakVal - 1.0f) < 1.0e-5f,
                   "ColorProcessor HQ dry delay aligns to exact sample 18 with unity gain",
                   "peakIdx=" + juce::String (peakIdx) + ", peak=" + juce::String (peakVal, 6));
        }

        // 2b. Aliasing suppression on 11.25 kHz musical high-frequency sine driven into Cold saturation:
        //     At sr = 48 kHz, 3rd harmonic 3*11.25 = 33.75 kHz folds back in 1x to |48 - 33.75| = 14.25 kHz!
        auto measureFoldback14250HzDb = [] (bool useHq) -> float
        {
            constexpr double sr     = 48000.0;
            constexpr int    N      = 8192;
            constexpr int    skip   = 1024;
            constexpr double fTest  = 11250.0;
            constexpr double fAlias = 14250.0; // |3 * 11250 - 48000| = 14250 Hz

            ColorProcessor col;
            col.prepare ((float) sr, 512);
            col.setMode (2);      // Cold (+6 dB @ 6 kHz pre-emphasis -> tanh)
            col.setAmount (1.0f);
            col.setHqEnabled (useHq);

            double re = 0.0, im = 0.0;
            const int M = N - skip;
            for (int n = 0; n < N; ++n)
            {
                float l = (float) (0.55 * std::sin (juce::MathConstants<double>::twoPi * fTest * (double) n / sr));
                float r = l;
                col.processSample (l, r);
                if (n >= skip)
                {
                    const int k = n - skip;
                    const double win = 0.5 * (1.0 - std::cos (juce::MathConstants<double>::twoPi * (double) k / (double) (M - 1)));
                    const double ang = juce::MathConstants<double>::twoPi * fAlias * (double) k / sr;
                    re += (double) l * win * std::cos (ang);
                    im -= (double) l * win * std::sin (ang);
                }
            }
            const double mag = std::sqrt (re * re + im * im) * (4.0 / (double) M);
            return (float) juce::Decibels::gainToDecibels (mag + 1.0e-12);
        };

        const float alias1xDb = measureFoldback14250HzDb (false);
        const float alias4xDb = measureFoldback14250HzDb (true);
        const float suppressionDb = alias1xDb - alias4xDb;
        check (suppressionDb > 50.0f,
               "ColorProcessor 4x Linear HQ suppresses 14.25 kHz foldback alias by > 50 dB",
               "1x=" + juce::String (alias1xDb, 1) + " dB, 4x=" + juce::String (alias4xDb, 1)
               + " dB, delta=" + juce::String (suppressionDb, 1) + " dB");

        // 2c. Full processor PDC latency sum (Lookahead + Colour HQ)
        {
            CrunchCompressorAudioProcessor proc;
            proc.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            buf.clear();

            setChoice (proc, Param::lookahead, 1); // 1 ms
            proc.setColorHqEnabled (false);
            proc.processBlock (buf, midi);
            const int lat1x = proc.getLatencySamples();

            proc.setColorHqEnabled (true);
            proc.processBlock (buf, midi);
            const int latHq = proc.getLatencySamples();

            check (lat1x >= 48 && (latHq - lat1x) == ColorProcessor::kHqLatencySamples,
                   "Processor PDC latency cleanly adds exact +18 Colour HQ samples onto Lookahead",
                   "lat1x=" + juce::String (lat1x) + ", latHq=" + juce::String (latHq));
        }
    }

    void testPresetsAbAndStatePersistence()
    {
        std::cout << "\n=== 3. Factory Presets, A/B Slots & State Persistence ===\n";

        CrunchCompressorAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);

        proc.applyFactoryPreset (5); // Parallel Crush Mix (Mix = 45%, Threshold = -32 dB)
        const float mixA = proc.apvts.getRawParameterValue (Param::mix)->load();

        proc.toggleAbSlot(); // Switch to Slot B (still Default Clean, Mix = 100%)
        const float mixB = proc.apvts.getRawParameterValue (Param::mix)->load();

        proc.toggleAbSlot(); // Switch back to Slot A
        const float mixBack = proc.apvts.getRawParameterValue (Param::mix)->load();

        check (std::abs (mixA - 45.0f) < 0.2f && std::abs (mixB - 100.0f) < 0.2f && std::abs (mixBack - 45.0f) < 0.2f,
               "A/B comparison slots preserve and swap parameter states cleanly",
               "mixA=" + juce::String (mixA, 1) + ", mixB=" + juce::String (mixB, 1));

        CrunchCompressorAudioProcessor::UiPackSettings up;
        up.themeId   = 1;
        up.paletteId = 3;
        up.layoutId  = 1;
        up.localeId  = 1;
        proc.setUiPackSettings (up, false);
        proc.setEditorSize (1320, 840);
        proc.setColorHqEnabled (true);
        proc.setKneeCurveVisible (false);

        juce::MemoryBlock stateBlob;
        proc.getStateInformation (stateBlob);

        CrunchCompressorAudioProcessor restored;
        restored.setStateInformation (stateBlob.getData(), (int) stateBlob.getSize());

        check (restored.getUiPackSettings().themeId == 1
               && restored.getUiPackSettings().paletteId == 3
               && restored.getUiPackSettings().localeId == 1
               && restored.getEditorWidth() == 1320
               && restored.isColorHqEnabled()
               && ! restored.isKneeCurveVisible(),
               "State serialization round-trips UI packs, editor size, Colour HQ & Knee curve flags");
    }

    void testKneeTransferCurveTracksParametersImmediately()
    {
        std::cout << "\n=== 3b. Knee Transfer Curve Tracks APVTS Immediately ===\n";

        CrunchCompressorAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        setFloat  (proc, Param::threshold, -20.0f);
        setFloat  (proc, Param::ratio,       4.0f);
        setChoice (proc, Param::autoGain,    0);
        setChoice (proc, Param::knee,        1); // Soft

        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        buf.clear();
        proc.processBlock (buf, midi); // synchronise the audio-thread compressor once

        std::vector<float> inDb (1), outDb (1);
        proc.getTransferCurve (-20.0f, -20.0f, 1, inDb, outDb);
        check (std::abs (outDb[0] - (-21.125f)) < 0.02f,
               "Soft knee baseline applies -1.125 dB at threshold",
               "out=" + juce::String (outDb[0], 3));

        // No processBlock() here: the UI must not depend on the next audio block
        // to refresh the static transfer curve after a Knee button click.
        setChoice (proc, Param::knee, 0); // Hard
        proc.getTransferCurve (-20.0f, -20.0f, 1, inDb, outDb);
        check (std::abs (outDb[0] - (-20.0f)) < 0.02f,
               "Hard knee transfer curve follows the UI immediately",
               "out=" + juce::String (outDb[0], 3));

        setChoice (proc, Param::knee, 1); // Soft again, still no audio block
        proc.getTransferCurve (-20.0f, -20.0f, 1, inDb, outDb);
        check (std::abs (outDb[0] - (-21.125f)) < 0.02f,
               "Soft knee transfer curve follows the UI immediately",
               "out=" + juce::String (outDb[0], 3));
    }

    void pumpSyntheticDrumBusSignal (CrunchCompressorAudioProcessor& proc, double sr = 48000.0, double seconds = 4.6)
    {
        constexpr int blockSize = 256;
        const int totalBlocks   = (int) std::ceil (seconds * sr / (double) blockSize);
        juce::AudioBuffer<float> buf (2, blockSize);
        juce::MidiBuffer midi;

        long long samplePos = 0;
        for (int b = 0; b < totalBlocks; ++b)
        {
            float* l = buf.getWritePointer (0);
            float* r = buf.getWritePointer (1);
            for (int i = 0; i < blockSize; ++i, ++samplePos)
            {
                const double t      = (double) samplePos / sr;
                const double beatT  = std::fmod (t, 0.42); // ~143 BPM transient hits
                const double envHit = std::exp (-beatT * 22.0);
                const double body   = 0.16 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * t)
                                    + 0.09 * std::sin (juce::MathConstants<double>::twoPi * 330.0 * t);
                const double punch  = 0.78 * envHit * std::sin (juce::MathConstants<double>::twoPi * (68.0 + 90.0 * envHit) * t);
                const float  sig    = (float) juce::jlimit (-0.98, 0.98, body + punch);
                l[i] = sig;
                r[i] = sig * 0.96f;
            }
            proc.processBlock (buf, midi);
        }
    }

    void writeSnapshot (const juce::File& dir, const juce::String& name, juce::Component* c)
    {
        if (c == nullptr)
            return;
        juce::Image snap = c->createComponentSnapshot (c->getLocalBounds());
        if (! snap.isValid())
            return;
        auto f = dir.getChildFile (name + ".png");
        f.deleteFile();
        std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
        if (os != nullptr && os->openedOk())
        {
            juce::PNGImageFormat png;
            png.writeImageToStream (snap, *os);
            os->flush();
            std::cout << "  [SHOT] " << f.getFullPathName() << " (" << snap.getWidth() << "x" << snap.getHeight() << ")\n";
        }
    }

    void testAndExportUiSnapshots (const juce::File& outDir)
    {
        std::cout << "\n=== 4. Editor Rendering Integrity & 2x2 Matrix Snapshot Export ===\n";
        outDir.createDirectory();

        CrunchCompressorAudioProcessor proc;
        proc.prepareToPlay (48000.0, 256);
        proc.applyFactoryPreset (6); // Drum Bus Snap
        setFloat  (proc, Param::threshold,   -21.5f);
        setFloat  (proc, Param::ratio,         4.5f);
        setFloat  (proc, Param::attack,       12.0f);
        setFloat  (proc, Param::release,      85.0f);
        setChoice (proc, Param::knee,          1); // Soft
        setChoice (proc, Param::autoGain,      1); // Auto Gain On
        setChoice (proc, Param::colorMode,     1); // Warm
        setFloat  (proc, Param::colorAmount,  32.0f);
        proc.setColorHqEnabled (true);
        proc.setKneeCurveVisible (true);

        pumpSyntheticDrumBusSignal (proc, 48000.0, 5.7);

        std::unique_ptr<CrunchCompressorAudioProcessorEditor> ed (
            dynamic_cast<CrunchCompressorAudioProcessorEditor*> (proc.createEditor()));
        check (ed != nullptr && ed->getWidth() == 1200 && ed->getHeight() == 780,
               "CrunchCompressorAudioProcessorEditor instantiates at 1200x780",
               "size=" + juce::String (ed != nullptr ? ed->getWidth() : 0)
               + "x" + juce::String (ed != nullptr ? ed->getHeight() : 0));

        if (ed == nullptr)
            return;

        auto& disp  = ed->getDisplayView();
        auto& panel = ed->getControlPanel();
        const double nowMs = (double) juce::Time::getMillisecondCounter();

        // Warm up static plot cache (>= 2 paints) & knob meter state
        disp.syncHistoryImmediateForSnapshot();
        panel.tickKnobs (nowMs);

        // 1. Light Neumorphism (NeuLight Violet) + en-US + Warm Wash
        disp.setUiPacks (UiPack::ThemeId::Neumorphism, UiPack::PaletteId::NeuLight,
                         UiPack::LayoutId::Sculpted, UiPack::LocaleId::EnUS);
        panel.syncFromProcessor (true);
        panel.tickKnobs (nowMs);
        (void) ed->createComponentSnapshot (ed->getLocalBounds());
        writeSnapshot (outDir, "round1_01_neu_light_en", ed.get());

        // 2. Light Neumorphism (NeuLight Violet) + zh-CN + Warm Wash
        disp.setLocalePack (UiPack::LocaleId::ZhCN);
        panel.syncFromProcessor (true);
        panel.tickKnobs (nowMs);
        (void) ed->createComponentSnapshot (ed->getLocalBounds());
        writeSnapshot (outDir, "round1_02_neu_light_zh", ed.get());

        // 3. Dark Classic (Classic Blue) + en-US + Cold Wash
        setChoice (proc, Param::colorMode,   2); // Cold
        setFloat  (proc, Param::colorAmount, 28.0f);
        disp.setUiPacks (UiPack::ThemeId::Classic, UiPack::PaletteId::Blue,
                         UiPack::LayoutId::Standard, UiPack::LocaleId::EnUS);
        panel.syncFromProcessor (true);
        panel.tickKnobs (nowMs);
        (void) ed->createComponentSnapshot (ed->getLocalBounds());
        writeSnapshot (outDir, "round1_03_classic_blue_en", ed.get());

        // 4. Dark Classic (Classic Blue) + zh-CN
        disp.setLocalePack (UiPack::LocaleId::ZhCN);
        panel.syncFromProcessor (true);
        panel.tickKnobs (nowMs);
        (void) ed->createComponentSnapshot (ed->getLocalBounds());
        writeSnapshot (outDir, "round1_04_classic_blue_zh", ed.get());

        // 5. Light Classic (Cream) + en-US + Threshold Guide Line Active
        setChoice (proc, Param::colorMode,   1); // Warm
        setFloat  (proc, Param::colorAmount, 22.0f);
        disp.setUiPacks (UiPack::ThemeId::Classic, UiPack::PaletteId::Cream,
                         UiPack::LayoutId::Standard, UiPack::LocaleId::EnUS);
        disp.setThresholdGuideActive (true);
        juce::Thread::sleep (180);
        panel.syncFromProcessor (true);
        panel.tickKnobs ((double) juce::Time::getMillisecondCounter());
        (void) ed->createComponentSnapshot (ed->getLocalBounds());
        writeSnapshot (outDir, "round1_05_classic_cream_en", ed.get());
        disp.setThresholdGuideActive (false);

        // 6. Dark Neumorphism (NeuDark) at Minimum Size 920x580
        disp.setUiPacks (UiPack::ThemeId::Neumorphism, UiPack::PaletteId::NeuDark,
                         UiPack::LayoutId::Sculpted, UiPack::LocaleId::EnUS);
        ed->setSize (EditorLayout::minWidth, EditorLayout::minHeight);
        proc.setWindowDragging (false);
        panel.syncFromProcessor (true);
        panel.tickKnobs ((double) juce::Time::getMillisecondCounter());
        (void) ed->createComponentSnapshot (ed->getLocalBounds());
        writeSnapshot (outDir, "round1_06_neu_dark_minsize_en", ed.get());

        juce::Image checkImg = ed->createComponentSnapshot (ed->getLocalBounds());
        int nonBlack = 0;
        for (int y = 0; y < checkImg.getHeight(); y += 4)
            for (int x = 0; x < checkImg.getWidth(); x += 4)
                if (checkImg.getPixelAt (x, y).getBrightness() > 0.02f)
                    ++nonBlack;

        check (checkImg.isValid() && nonBlack > 8000,
               "All 6 matrix snapshots rendered with valid waveform, controls & typography",
               "sampledNonBlack=" + juce::String (nonBlack));
    }
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("_review/ui_snapshots_round1");
    if (argc > 1)
        outDir = juce::File (juce::String (argv[1]));

    std::cout << "=========================================================\n";
    std::cout << "  Crunch Compressor — Four-Pack UI & Colour HQ Verify\n";
    std::cout << "=========================================================\n";

    // Preserve user's global UI preferences across test runs
    const auto prefsFile  = CrunchCompressorAudioProcessor::globalUiPrefsFile();
    const bool hadPrefs   = prefsFile.existsAsFile();
    const auto savedPrefs = hadPrefs ? prefsFile.loadFileAsString() : juce::String();

    testFourPackAndLocales();
    testColourModuleAndLatency();
    testPresetsAbAndStatePersistence();
    testKneeTransferCurveTracksParametersImmediately();
    testAndExportUiSnapshots (outDir);

    if (hadPrefs)
        prefsFile.replaceWithText (savedPrefs);
    else
        prefsFile.deleteFile();

    std::cout << "\n---------------------------------------------------------\n";
    std::cout << "  Summary: " << gPassed << " PASSED, " << gFailed << " FAILED\n";
    std::cout << "---------------------------------------------------------\n";
    return (gFailed == 0) ? 0 : 1;
}
