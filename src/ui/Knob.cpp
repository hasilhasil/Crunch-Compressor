#include "Knob.h"

void Knob::setRange (double minValue, double maxValue, double interval)
{
    min_ = minValue;
    max_ = std::max (minValue, maxValue);
    interval_ = interval;
    value_   = juce::jlimit (min_, max_, value_);
    default_ = juce::jlimit (min_, max_, default_);
    updateSkew();
    repaint();
}

void Knob::updateSkew()
{
    skew_    = 1.0f;
    hasSkew_ = false;
    if (! skewSet_)
        return;
    const double span = max_ - min_;
    const double c    = (span > 0.0) ? (skewCentre_ - min_) / span : 0.5;
    if (c > 0.0 && c < 1.0)
    {
        skew_    = (float) (std::log (0.5) / std::log (c));
        hasSkew_ = std::abs (skew_ - 1.0f) > 1.0e-4f;
    }
}

float Knob::toNorm (double v) const
{
    const double span = max_ - min_;
    if (span <= 0.0)
        return 0.0f;
    double prop = juce::jlimit (0.0, 1.0, (v - min_) / span);
    if (! hasSkew_)
        return (float) prop;
    return (float) std::pow (prop, (double) skew_);
}

double Knob::fromNorm (double n) const
{
    const double span = max_ - min_;
    n = juce::jlimit (0.0, 1.0, n);
    if (! hasSkew_)
        return min_ + n * span;
    return min_ + std::pow (n, 1.0 / (double) skew_) * span;
}

void Knob::setValue (double v, bool sendNotification)
{
    const double nv = juce::jlimit (min_, max_, v);
    if (std::abs (nv - value_) < 1.0e-9)
        return;
    value_ = nv;
    repaint();
    if (sendNotification && onValueChange)
        onValueChange (value_);
}

void Knob::notifyHoverOrDragIfChanged()
{
    const bool nowActive = hovered_ || dragging_;
    if (nowActive != lastActiveFlag_)
    {
        lastActiveFlag_ = nowActive;
        if (onHoverOrDragChanged)
            onHoverOrDragChanged (nowActive);
    }
}

juce::Rectangle<float> Knob::knobBounds() const
{
    const float w = (float) getWidth();
    const float d = juce::jmin (knobDiameter_, juce::jmax (16.0f, w - 4.0f));
    const float topOff = showLabel_
        ? juce::jmax (labelTextSize_ > 0.0f ? labelTextSize_ + 4.0f : 18.0f, d * 0.28f)
        : ((float) getHeight() - d) * 0.5f - 6.0f;
    return juce::Rectangle<float> (w * 0.5f - d * 0.5f, topOff, d, d);
}

float Knob::valueToAngle (double v) const
{
    const float norm  = toNorm (v);
    const float start = juce::MathConstants<float>::pi * 1.25f;
    const float end   = juce::MathConstants<float>::pi * 2.75f;
    return start + juce::jlimit (0.0f, 1.0f, norm) * (end - start);
}

void Knob::commitValue (double v)
{
    if (interval_ > 0.0)
        v = std::round (v / interval_) * interval_;
    setValue (v, true);
}

void Knob::commitValueFine (double v)
{
    setValue (v, true);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;
    if (editing_)
    {
        closeEditor (true);
        return;
    }
    if (selectionFeedback_)
    {
        grabKeyboardFocus();
        repaint();
    }

    dragStartValue_ = value_;
    dragStartNorm_  = (double) toNorm (value_);
    dragging_       = true;
    mouseDownHeld_  = true;

    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        if (onDragStart) onDragStart();
        setValue (default_, true);
        if (onDragEnd) onDragEnd();
        dragging_      = false;
        mouseDownHeld_ = false;
        notifyHoverOrDragIfChanged();
        return;
    }
    notifyHoverOrDragIfChanged();
    if (onDragStart)
        onDragStart();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! isEnabled() || ! dragging_)
        return;

    const float dy   = (float) (e.getMouseDownY() - e.getPosition().getY());
    const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;

    const double span = max_ - min_;
    if (span <= 0.0)
        return;

    const double newNorm = dragStartNorm_ + (double) dy / 220.0 * (double) fine;
    if (e.mods.isShiftDown())
        commitValueFine (fromNorm (newNorm));
    else
        commitValue (fromNorm (newNorm));
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (dragging_ && onDragEnd)
        onDragEnd();
    dragging_      = false;
    mouseDownHeld_ = false;
    notifyHoverOrDragIfChanged();
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled())
        return;
    openEditor();
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! isEnabled() || editing_ || std::abs (wheel.deltaY) < 1.0e-5f)
        return;

    if (onDragStart) onDragStart();
    if (hasSkew_)
    {
        double dn = (wheel.deltaY < 0.0f) ? -0.02 : 0.02;
        if (e.mods.isShiftDown()) dn *= 0.1;
        if (e.mods.isCtrlDown() || e.mods.isCommandDown()) dn *= 5.0;
        setValue (fromNorm ((double) toNorm (value_) + dn), true);
    }
    else
    {
        const double span = max_ - min_;
        double step = (interval_ > 0.0) ? interval_ : span * 0.02;
        if (wheel.deltaY < 0.0f) step = -step;
        if (e.mods.isShiftDown())
            commitValueFine (value_ + step * 0.1);
        else
        {
            if (e.mods.isCtrlDown() || e.mods.isCommandDown())
                step *= 5.0;
            commitValue (value_ + step);
        }
    }
    if (onDragEnd) onDragEnd();
}

void Knob::mouseEnter (const juce::MouseEvent&)
{
    if (isEnabled())
    {
        hovered_ = true;
        notifyHoverOrDragIfChanged();
    }
}

void Knob::mouseExit (const juce::MouseEvent&)
{
    hovered_ = false;
    notifyHoverOrDragIfChanged();
}

void Knob::advanceStates (double nowMs)
{
    if (! isEnabled())
    {
        hovered_       = false;
        mouseDownHeld_ = false;
        dragging_      = false;
        notifyHoverOrDragIfChanged();
    }
    const bool wasAnimating = animating_;
    const double ms = Anim::kKnobMs;
    tweenTo (hoverT_,  nowMs, hovered_       ? 1.0f : 0.0f, ms);
    tweenTo (downT_,   nowMs, mouseDownHeld_ ? 1.0f : 0.0f, ms);
    tweenTo (rotateT_, nowMs, dragging_      ? 1.0f : 0.0f, ms);

    animating_ = ! (hoverT_.done (nowMs) && downT_.done (nowMs) && rotateT_.done (nowMs));
    lastNowMs_ = nowMs;
    if ((animating_ || wasAnimating) && isVisible())
        repaint();
}

void Knob::openEditor()
{
    editing_      = true;
    editText_.clear();
    editCursorOn_ = true;
    editCursorLastToggleMs_ = juce::Time::getMillisecondCounter();
    setWantsKeyboardFocus (true);
    grabKeyboardFocus();
    repaint();
}

void Knob::setEditCursorBlink (juce::uint32 nowMs)
{
    if (! editing_)
        return;
    if (editCursorLastToggleMs_ == 0)
        editCursorLastToggleMs_ = nowMs;
    if ((int) (nowMs - editCursorLastToggleMs_) >= (int) Anim::kBlinkMs)
    {
        editCursorOn_ = ! editCursorOn_;
        editCursorLastToggleMs_ = nowMs;
        repaint();
    }
}

void Knob::closeEditor (bool apply)
{
    if (! editing_)
        return;
    editing_ = false;
    if (apply && editText_.isNotEmpty())
    {
        juce::String t = editText_.trim();
        double mult = 1.0;
        if (t.containsChar ('k') || t.containsChar ('K'))
        {
            mult = 1000.0;
            t = t.replaceCharacter ('k', ' ').replaceCharacter ('K', ' ').trim();
        }
        double v = t.getDoubleValue() * mult;
        if (editText_.containsChar ('%') && suffix_ != "%")
            v = min_ + (max_ - min_) * t.getDoubleValue() / 100.0;
        if (onDragStart) onDragStart();
        commitValue (v);
        if (onDragEnd)   onDragEnd();
    }
    editText_.clear();
    repaint();
}

bool Knob::keyPressed (const juce::KeyPress& key)
{
    if (! isEnabled())
        return false;
    if (editing_)
        return keyPressedEditing (key);

    const auto kc = key.getKeyCode();
    if (hasSkew_)
    {
        const double dn = key.getModifiers().isShiftDown() ? 0.005 : 0.025;
        if (kc == juce::KeyPress::upKey || kc == juce::KeyPress::rightKey)
        {
            setValue (fromNorm ((double) toNorm (value_) + dn), true);
            return true;
        }
        if (kc == juce::KeyPress::downKey || kc == juce::KeyPress::leftKey)
        {
            setValue (fromNorm ((double) toNorm (value_) - dn), true);
            return true;
        }
    }
    else
    {
        const double span   = max_ - min_;
        const double coarse = (interval_ > 0.0) ? interval_ * 10.0 : span * 0.05;
        const double fine   = (interval_ > 0.0) ? interval_        : span * 0.005;
        const double step   = key.getModifiers().isShiftDown() ? fine : coarse;

        if (kc == juce::KeyPress::upKey || kc == juce::KeyPress::rightKey)
        {
            setValue (juce::jlimit (min_, max_, value_ + step), true);
            return true;
        }
        if (kc == juce::KeyPress::downKey || kc == juce::KeyPress::leftKey)
        {
            setValue (juce::jlimit (min_, max_, value_ - step), true);
            return true;
        }
    }

    // 严禁按空格（spaceKey）弹出输入框（避免抢占 DAW 播放键）
    if (key == juce::KeyPress::returnKey)
    {
        openEditor();
        return true;
    }
    if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey)
    {
        setValue (default_, true);
        return true;
    }
    return false;
}

bool Knob::keyPressedEditing (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { closeEditor (false); return true; }
    if (key == juce::KeyPress::returnKey) { closeEditor (true);  return true; }
    if (key == juce::KeyPress::backspaceKey)
    {
        editText_ = editText_.substring (0, juce::jmax (0, editText_.length() - 1));
        repaint();
        return true;
    }

    const juce::juce_wchar ch = key.getTextCharacter();
    const juce::juce_wchar c  = (ch != 0) ? ch : (juce::juce_wchar) key.getKeyCode();
    if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == '%' || c == 'k' || c == 'K')
    {
        if (editText_.length() < 12)
            editText_ += juce::String::charToString (c);
        repaint();
        return true;
    }
    return false;
}

juce::String Knob::formatReadout() const
{
    const juce::String sign = (bipolar_ && value_ > 0.005) ? "+" : "";
    const int effDecimals   = (adaptiveMsDecimals_ && value_ >= 1.0) ? 0 : decimals_;
    juce::String txt        = valuePrefix_ + sign + juce::String (value_, effDecimals);
    if (suffix_.isNotEmpty())
        txt << " " << suffix_;
    return txt;
}

void Knob::buildBackLayer (float d)
{
    const int pad = themeTokens_.isNeumorphic ? 12 : 4;
    const int px  = juce::jmax (8, juce::roundToInt (d) + pad);
    backLayer_ = juce::Image (juce::Image::ARGB, px, px, true);
    juce::Graphics g (backLayer_);

    const float cx = px * 0.5f, cy = px * 0.5f;
    const float r  = d * 0.5f;
    const float a0 = valueToAngle (min_);
    const float a1 = valueToAngle (max_);

    const int   nTicks    = 11;
    const float tickInner = r + juce::jmax (1.5f, d * 0.035f);
    const float tickOuter = r + juce::jmax (3.5f, d * 0.075f);
    g.setColour (themeTokens_.isNeumorphic ? pal_.dim.withAlpha (0.55f) : pal_.gridZ);
    for (int i = 0; i < nTicks; ++i)
    {
        const float t  = (float) i / (float) (nTicks - 1);
        const float a  = a0 + t * (a1 - a0);
        const float x1 = cx + std::sin (a) * tickInner;
        const float y1 = cy - std::cos (a) * tickInner;
        const float x2 = cx + std::sin (a) * tickOuter;
        const float y2 = cy - std::cos (a) * tickOuter;
        g.drawLine (x1, y1, x2, y2, juce::jmax (1.0f, d * 0.018f));
    }

    const float bodyR = r - juce::jmax (1.0f, d * 0.02f);
    const juce::Rectangle<float> bodyBox (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    if (themeTokens_.isNeumorphic)
    {
        UiPack::NeuPainter::drawInsetCircle (g, bodyBox, pal_, themeTokens_.insetButton, d / 72.0f);
    }
    else
    {
        juce::ColourGradient bodyGrad (pal_.track.brighter (0.10f), cx, cy - bodyR,
                                       pal_.track.darker (0.25f), cx, cy + bodyR, false);
        g.setGradientFill (bodyGrad);
        g.fillEllipse (bodyBox);
    }
}

void Knob::paint (juce::Graphics& g)
{
    const float w  = (float) getWidth();
    const auto  kb = knobBounds();
    const float d  = kb.getWidth();
    const float cx = kb.getCentreX();
    const float cy = kb.getCentreY();
    const float r  = d * 0.5f;

    const juce::Colour primaryAccent = pal_.accent;

    if (showLabel_ && label_.isNotEmpty())
    {
        const auto labFont = CompTheme::font (CompTheme::semiBold(),
                                              labelTextSize_ > 0.0f ? labelTextSize_ : d * (14.0f / 72.0f));
        const float labH = labFont.getHeight() + 1.0f;
        g.setFont (labFont);
        g.setColour (accentLabel_ ? primaryAccent : pal_.dim);
        auto labelBox = juce::Rectangle<float> (0.0f,
                                                juce::jmax (0.0f, kb.getY() - labH - 3.0f),
                                                w, labH);
        g.drawText (label_, labelBox, juce::Justification::centred, false);
    }

    const float qd = std::round (d);
    if (std::abs (backLayerDiameter_ - qd) >= 1.0f)
    {
        buildBackLayer (qd);
        backLayerDiameter_ = qd;
    }
    g.drawImageAt (backLayer_, (int) (cx - backLayer_.getWidth() * 0.5f),
                               (int) (cy - backLayer_.getHeight() * 0.5f));

    const double nowMs  = lastNowMs_;
    const float hoverK  = hoverT_.value (nowMs);
    const float downK   = downT_.value (nowMs);
    const float rotateK = rotateT_.value (nowMs);
    const float activeK = juce::jmax (0.0f, hoverK * (1.0f - downK));
    const float pressK  = downK * (1.0f - rotateK);

    const float a0    = valueToAngle (min_);
    const float a1    = valueToAngle (max_);
    const float av    = valueToAngle (value_);
    const float thick = juce::jmax (2.0f, d * 0.10f);
    const float arcR  = r - thick * 0.5f;

    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (pal_.track.withAlpha (0.55f));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    const float from = bipolar_ ? valueToAngle (bipolarOrigin_) : a0;
    if (bipolar_)
    {
        const float tickR0 = arcR - thick * 0.75f;
        const float tickR1 = arcR + thick * 0.75f;
        g.setColour (pal_.dim.withAlpha (0.85f));
        g.drawLine (cx + std::sin (from) * tickR0, cy - std::cos (from) * tickR0,
                    cx + std::sin (from) * tickR1, cy - std::cos (from) * tickR1,
                    juce::jmax (1.0f, d * 0.02f));
    }

    if (std::abs (av - from) > 0.004f)
    {
        juce::Path val;
        val.addCentredArc (cx, cy, arcR, arcR, 0.0f,
                           juce::jmin (from, av), juce::jmax (from, av), true);
        const juce::Colour warmCol = pal_.accent4;
        juce::Colour arcCol = primaryAccent.brighter (0.10f * activeK);
        if (pressK > 0.001f)
            arcCol = arcCol.darker (0.32f * pressK);
        if (rotateK > 0.001f)
            arcCol = arcCol.interpolatedWith (warmCol.brighter (0.10f), rotateK * 0.85f);
        g.setColour (arcCol);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // THRESHOLD 同轨双层颜色叠加：主轨道内叠加青绿实时输入电平弧 + 超阈值琥珀高亮弧
    if (trackMeterEnabled_ && trackMeterValue_ > min_ + 0.2)
    {
        const float aMeter = valueToAngle (juce::jlimit (min_, max_, trackMeterValue_));
        if (aMeter > a0 + 0.01f)
        {
            const float aUnderEnd = juce::jmin (aMeter, av);
            if (aUnderEnd > a0 + 0.01f)
            {
                juce::Path underArc;
                underArc.addCentredArc (cx, cy, arcR, arcR, 0.0f, a0, aUnderEnd, true);
                g.setColour (pal_.accent3.withAlpha (0.76f));
                g.strokePath (underArc, juce::PathStrokeType (thick * 0.68f, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
            }
            if (aMeter > av + 0.01f)
            {
                juce::Path overArc;
                overArc.addCentredArc (cx, cy, arcR, arcR, 0.0f, av, aMeter, true);
                g.setColour (pal_.accent4.withAlpha (0.95f));
                g.strokePath (overArc, juce::PathStrokeType (thick * 0.92f, juce::PathStrokeType::curved,
                                                             juce::PathStrokeType::rounded));
            }
        }
    }

    const float capR = r - thick - juce::jmax (1.0f, d * 0.05f);
    const juce::Rectangle<float> capBox (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    if (themeTokens_.isNeumorphic)
    {
        const float ks = juce::jmax (0.65f, d / 72.0f);
        if (pressK > 0.45f)
        {
            UiPack::NeuPainter::drawInsetCircle (g, capBox, pal_, themeTokens_.insetButton, ks);
        }
        else
        {
            UiPack::DualShadowSpec spec = themeTokens_.shadowButton;
            spec.offset = spec.offset + (themeTokens_.shadowHover.offset - spec.offset) * activeK;
            spec.blur   = spec.blur   + (themeTokens_.shadowHover.blur   - spec.blur)   * activeK;
            UiPack::NeuPainter::drawRaisedCircle (g, capBox, pal_, spec, ks);
        }
    }
    else
    {
        juce::Colour capCol = pal_.knob;
        capCol = capCol.brighter (0.16f * activeK);
        capCol = capCol.darker   (0.26f * pressK);
        if (rotateK > 0.001f)
            capCol = capCol.interpolatedWith (pal_.accent4.interpolatedWith (pal_.knob, 0.35f), rotateK * 0.62f);
        juce::ColourGradient grad (capCol.brighter (0.12f), cx, cy - capR,
                                   capCol.darker (0.15f), cx, cy + capR, false);
        g.setGradientFill (grad);
        g.fillEllipse (capBox);
    }

    const float pw    = juce::jmax (1.8f, d * 0.07f);
    const float halfW = pw * 0.5f;
    const float pR0   = capR * 0.24f;
    const float pR1   = themeTokens_.isNeumorphic
                            ? (capR * 0.86f)
                            : juce::jmin (capR * 0.86f, capR - juce::jmax (1.5f, d * 0.035f));
    if (themeTokens_.isNeumorphic)
        g.setColour (rotateK > 0.05f ? pal_.accent : pal_.text.withAlpha (0.85f));
    else
        g.setColour (pal_.knob.contrasting (1.0f).withAlpha (pal_.light ? 0.75f : 0.80f));

    if (pR1 > pR0 + 0.5f)
    {
        juce::Path needle;
        needle.addRoundedRectangle (-halfW, -pR1, pw, pR1 - pR0,
                                    halfW, halfW, true, true, false, false);
        needle.applyTransform (juce::AffineTransform::rotation (av).translated (cx, cy));
        g.fillPath (needle);
    }

    // 旋钮下方数值读数：始终使用 Google Sans Flex（latinFont）
    const juce::String txt = formatReadout();
    const auto valFont = CompTheme::latinFont (CompTheme::medium(),
                                               valueTextSize_ > 0.0f ? valueTextSize_ : d * (15.0f / 72.0f));
    if (selectionFeedback_ && hasKeyboardFocus (true) && isEnabled())
    {
        const auto box = juce::Rectangle<float> (4.0f, kb.getBottom() + 1.0f, w - 8.0f, valFont.getHeight() + 4.0f);
        if (themeTokens_.isNeumorphic)
            UiPack::NeuPainter::drawInsetRect (g, box, 5.0f, pal_, themeTokens_.insetFocus, 0.85f, 1.0f, true);
        else
        {
            g.setColour (pal_.accent.withAlpha (0.15f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (pal_.accent);
            g.drawRoundedRectangle (box, 4.0f, 1.0f);
        }
    }

    const juce::Rectangle<float> readoutRect (0.0f, kb.getBottom() + 2.0f, w, valFont.getHeight() + 3.0f);
    g.setFont (valFont);
    g.setColour (editing_ ? pal_.accent : pal_.text);
    g.drawText (txt, readoutRect, juce::Justification::centredTop, false);

    if (editing_)
    {
        const float bw = juce::jmin (w - 2.0f, juce::jmax (w * 0.88f, 76.0f));
        const float bh = 22.0f;
        auto box = juce::Rectangle<float> (w * 0.5f - bw * 0.5f, cy - bh * 0.5f, bw, bh);
        if (themeTokens_.isNeumorphic)
        {
            UiPack::NeuPainter::drawInsetRect (g, box, 6.0f, pal_, themeTokens_.insetFocus, 1.0f, 1.0f, true);
            g.setColour (pal_.accent.withAlpha (0.5f));
            g.drawRoundedRectangle (box, 6.0f, 1.0f);
        }
        else
        {
            g.setColour (pal_.surface.withAlpha (0.97f));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (pal_.accent);
            g.drawRoundedRectangle (box, 5.0f, 1.2f);
        }
        g.setColour (pal_.text);
        g.setFont (CompTheme::latinFont (CompTheme::medium(), valueTextSize_ > 0.0f ? valueTextSize_ : 12.5f));
        g.drawText (editText_ + (editCursorOn_ ? "|" : " "), box, juce::Justification::centred, false);
    }
}

void Knob::resized() {}
