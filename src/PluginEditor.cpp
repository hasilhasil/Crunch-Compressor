#include "PluginEditor.h"

CrunchCompressorAudioProcessorEditor::CrunchCompressorAudioProcessorEditor (CrunchCompressorAudioProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor_ (p),
      display_ (p),
      panel_ (p)
{
    setLookAndFeel (&lookAndFeel_);
    CompTheme::setTextScale (CompTheme::kUiTextScale);

    addAndMakeVisible (display_);
    addAndMakeVisible (panel_);
    addChildComponent (popupOverlay_);

    panel_.onGlobalStateChanged = [this]
    {
        display_.parametersChanged();
        display_.ensureTimerFullRate();
        display_.repaint();
    };

    panel_.onThresholdFocusChanged = [this] (bool active)
    {
        display_.setThresholdGuideActive (active);
    };

    display_.onTickKnobs = [this] (double nowMs)
    {
        panel_.tickKnobs (nowMs);
    };

    display_.isExternalAnimating = [this]
    {
        return panel_.isKnobAnimating();
    };

    display_.onCompositionChanged = [this]
    {
        applySnapshotToAll();
    };

    display_.onPresetApplied = [this]
    {
        panel_.syncFromProcessor (false);
        panel_.repaint();
        display_.parametersChanged();
        display_.ensureTimerFullRate();
    };

    panel_.onRequestPopup = [this] (const juce::String& tag,
                                    juce::Rectangle<float> anchorInPanel,
                                    const juce::StringArray& items,
                                    int currentIndex,
                                    bool useTranslatedFont,
                                    std::function<void (int)> onSelect)
    {
        const auto anchorInEditor = anchorInPanel.translated ((float) panel_.getX(), (float) panel_.getY());
        popupOverlay_.open (tag, anchorInEditor, items, currentIndex, useTranslatedFont,
                            std::move (onSelect),
                            [this] { display_.repaint(); panel_.repaint(); });
    };

    // 先 setSize，再 setResizable + setResizeLimits，确保右下角缩放手柄 z-order 正确
    const int savedW = processor_.getEditorWidth();
    const int savedH = processor_.getEditorHeight();
    const int initW  = (savedW >= EditorLayout::minWidth  && savedW <= EditorLayout::maxWidth)  ? savedW : EditorLayout::defaultWidth;
    const int initH  = (savedH >= EditorLayout::minHeight && savedH <= EditorLayout::maxHeight) ? savedH : EditorLayout::defaultHeight;
    setSize (initW, initH);
    setResizable (true, true);
    setResizeLimits (EditorLayout::minWidth, EditorLayout::minHeight, EditorLayout::maxWidth, EditorLayout::maxHeight);

    applySnapshotToAll();

    for (auto* param : processor_.getParameters())
    {
        if (auto* idp = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor_.apvts.addParameterListener (idp->paramID, this);
    }

    forceSoftwareRenderer();
}

CrunchCompressorAudioProcessorEditor::~CrunchCompressorAudioProcessorEditor()
{
    stopTimer();
    processor_.setWindowDragging (false);

    for (auto* param : processor_.getParameters())
    {
        if (auto* idp = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor_.apvts.removeParameterListener (idp->paramID, this);
    }

    setLookAndFeel (nullptr);
}

void CrunchCompressorAudioProcessorEditor::applySnapshotToAll()
{
    const auto& snap = display_.getUiSnapshot();
    const EditorLayout lay (getLocalBounds(), snap.layout);

    lookAndFeel_.setUiScale (lay.scale);
    lookAndFeel_.applyUiSnapshot (snap);
    panel_.applyUiSnapshot (snap);
    popupOverlay_.applyUiSnapshot (snap);
    popupOverlay_.close();

    layoutComponents();
    sendLookAndFeelChange();
    display_.repaint();
    panel_.repaint();
    repaint();
}

void CrunchCompressorAudioProcessorEditor::layoutComponents()
{
    const auto& snap = display_.getUiSnapshot();
    const EditorLayout lay (getLocalBounds(), snap.layout);

    lookAndFeel_.setUiScale (lay.scale);
    display_.setBounds (lay.displayViewBounds());
    panel_.setBounds   (lay.bottomPanelBounds());
    popupOverlay_.setBounds (getLocalBounds());
}

void CrunchCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (display_.getUiSnapshot().palette.bg);
}

void CrunchCompressorAudioProcessorEditor::resized()
{
    processor_.setEditorSize (getWidth(), getHeight());
    noteWindowMove();
    popupOverlay_.close();
    layoutComponents();
}

void CrunchCompressorAudioProcessorEditor::moved()
{
    noteWindowMove();
}

void CrunchCompressorAudioProcessorEditor::noteWindowMove()
{
    if (! isShowing())
        return;
    processor_.setWindowDragging (true);
    lastMoveMs_ = juce::Time::getMillisecondCounter();
    startTimer (50);
}

void CrunchCompressorAudioProcessorEditor::timerCallback()
{
    if (juce::Time::getMillisecondCounter() - lastMoveMs_ >= 150)
    {
        processor_.setWindowDragging (false);
        stopTimer();
        display_.repaint();
        panel_.repaint();
    }
}

void CrunchCompressorAudioProcessorEditor::parentHierarchyChanged()
{
    forceSoftwareRenderer();
}

void CrunchCompressorAudioProcessorEditor::forceSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    const auto engines  = peer->getAvailableRenderingEngines();
    const int  software = engines.indexOf (juce::String ("Software Renderer"));

    if (software < 0 || peer->getCurrentRenderingEngine() == software)
        return;

    peer->setCurrentRenderingEngine (software);
    repaint();
}

void CrunchCompressorAudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    // 严格遵守 60fps 铁律：仅置无锁原子脏标记，绝不在参数回调线程直接调 repaint()
    display_.parametersChanged();
}
