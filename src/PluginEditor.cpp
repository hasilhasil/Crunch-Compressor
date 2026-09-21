#include "PluginEditor.h"

CrunchCompressorAudioProcessorEditor::CrunchCompressorAudioProcessorEditor (CrunchCompressorAudioProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor_ (p),
      display_ (p),
      panel_ (p)
{
    setLookAndFeel (&lookAndFeel_);
    lookAndFeel_.setTheme (processor_.getTheme());

    addAndMakeVisible (display_);
    addAndMakeVisible (panel_);

    const int savedW = processor_.getEditorWidth();
    const int savedH = processor_.getEditorHeight();
    setSize (savedW > 0 ? savedW : 920, savedH > 0 ? savedH : 620);
    setResizable (true, true);
    setResizeLimits (700, 500, 1840, 1240);

    sendLookAndFeelChange();

    for (auto* param : processor_.getParameters())
    {
        if (auto* idp = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor_.apvts.addParameterListener (idp->paramID, this);
    }

    // Theme changes must repaint every component: JUCE buttons/combos do not
    // reliably repaint on lookAndFeelChanged(), so panels would keep stale
    // (e.g. black-on-white-theme) colours.
    display_.onThemeChanged = [this]
    {
        sendLookAndFeelChange();
        display_.repaint();
        panel_.repaint();
        repaint();
    };

    // The peer may already exist (standalone / some hosts); otherwise
    // parentHierarchyChanged() applies this once the host attaches the editor.
    forceSoftwareRenderer();
}

CrunchCompressorAudioProcessorEditor::~CrunchCompressorAudioProcessorEditor()
{
    stopTimer();

    for (auto* param : processor_.getParameters())
    {
        if (auto* idp = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor_.apvts.removeParameterListener (idp->paramID, this);
    }

    setLookAndFeel (nullptr);
}

void CrunchCompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (CrunchPalette::background (lookAndFeel_.isLightTheme()));
}

void CrunchCompressorAudioProcessorEditor::resized()
{
    processor_.setEditorSize (getWidth(), getHeight());

    // A resize drag also runs a modal loop that invalidates the window per
    // step, so the display should stand down for it exactly like a move.
    noteWindowMove();

    auto area = getLocalBounds();
    panel_.setBounds (area.removeFromBottom (280));
    display_.setBounds (area);
}

// JUCE calls this for every window-position change (WM_WINDOWPOSCHANGED ->
// handleMovedOrResized -> sendMovedResizedMessages), i.e. once per mouse
// sample while the user drags the editor window.
void CrunchCompressorAudioProcessorEditor::moved()
{
    noteWindowMove();
}

void CrunchCompressorAudioProcessorEditor::noteWindowMove()
{
    processor_.setWindowDragging (true);
    lastMoveMs_ = juce::Time::getMillisecondCounter();
    startTimer (50);   // watches for the end of the drag loop
}

void CrunchCompressorAudioProcessorEditor::timerCallback()
{
    // No move event for a while: the drag loop has finished, so the display
    // may animate again. The very next display tick repaints everything.
    if (juce::Time::getMillisecondCounter() - lastMoveMs_ >= 150)
    {
        processor_.setWindowDragging (false);
        stopTimer();
    }
}

// Called when the host attaches (or re-attaches) the editor, i.e. exactly when
// the native peer becomes available. Switching the rendering engine is
// idempotent, so calling it from here and from the constructor is safe.
void CrunchCompressorAudioProcessorEditor::parentHierarchyChanged()
{
    forceSoftwareRenderer();
}

void CrunchCompressorAudioProcessorEditor::forceSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    const auto engines = peer->getAvailableRenderingEngines();
    const int software = engines.indexOf (juce::String ("Software Renderer"));

    if (software < 0 || peer->getCurrentRenderingEngine() == software)
        return;

    peer->setCurrentRenderingEngine (software);
    repaint();
}

void CrunchCompressorAudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    // The static transfer curve depends on the parameters, so mark it stale
    // instead of recomputing it on every repaint.
    display_.parametersChanged();
    display_.repaint();
    panel_.repaint();
}
