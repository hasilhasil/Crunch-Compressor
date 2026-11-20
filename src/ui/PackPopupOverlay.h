#pragma once

#include <JuceHeader.h>
#include "CompTheme.h"
#include "EditorLayout.h"

// ============================================================
// PackPopupOverlay — 四包自适应浮动下拉菜单层（挂载于顶层 Editor，永不被子面板裁切）
//   · 严格执行 POPUP_MENU_CONCENTRIC_RADIUS_SPEC.md 同心圆角嵌套定律：
//     外框 R_outer 与首尾项外贴边角满足 R_edge = max(4*s, R_outer - pad)
//   · 关闭态 setVisible(false)，零渲染开销、零鼠标拦截
// ============================================================
class PackPopupOverlay : public juce::Component
{
public:
    PackPopupOverlay()
    {
        setVisible (false);
        setWantsKeyboardFocus (true);
    }

    void applyUiSnapshot (const UiPack::UiSnapshot& snap)
    {
        snap_ = snap;
        if (isVisible())
            repaint();
    }

    bool isPopupOpen() const noexcept { return open_; }
    juce::String activeTag() const noexcept { return tag_; }

    /** 构造支持独立顶部圆角 (rTop) 与底部圆角 (rBot) 的同心圆角矩形路径 */
    static juce::Path makeConcentricRowPath (juce::Rectangle<float> b, float rTop, float rBot) noexcept
    {
        juce::Path p;
        const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
        const float maxR = juce::jmin (w, h) * 0.5f;
        const float rt   = juce::jlimit (0.0f, maxR, rTop);
        const float rb   = juce::jlimit (0.0f, maxR, rBot);
        constexpr float k = 0.55228475f; // 三次贝塞尔拟合 90° 圆弧精确常数

        p.startNewSubPath (x + rt, y);
        p.lineTo (x + w - rt, y);
        if (rt > 0.0f)
            p.cubicTo (x + w - rt * (1.0f - k), y, x + w, y + rt * (1.0f - k), x + w, y + rt);

        p.lineTo (x + w, y + h - rb);
        if (rb > 0.0f)
            p.cubicTo (x + w, y + h - rb * (1.0f - k), x + w - rb * (1.0f - k), y + h, x + w - rb, y + h);

        p.lineTo (x + rb, y + h);
        if (rb > 0.0f)
            p.cubicTo (x + rb * (1.0f - k), y + h, x, y + h - rb * (1.0f - k), x, y + h - rb);

        p.lineTo (x, y + rt);
        if (rt > 0.0f)
            p.cubicTo (x, y + rt * (1.0f - k), x + rt * (1.0f - k), y, x + rt, y);

        p.closeSubPath();
        return p;
    }

    void open (const juce::String& tag,
               juce::Rectangle<float> anchorInEditor,
               const juce::StringArray& items,
               int currentIndex,
               bool useTranslatedFont,
               std::function<void (int)> onSelect,
               std::function<void()> onDismiss = {})
    {
        if (open_ && tag_ == tag)
        {
            close();
            return;
        }

        tag_               = tag;
        anchor_            = anchorInEditor;
        items_             = items;
        currentIndex_      = currentIndex;
        hoverIndex_        = -1;
        useTranslatedFont_ = useTranslatedFont;
        onSelect_          = std::move (onSelect);
        onDismiss_         = std::move (onDismiss);
        open_              = true;

        setVisible (true);
        toFront (true);
        repaint();
    }

    void close()
    {
        if (! open_)
            return;
        open_       = false;
        hoverIndex_ = -1;
        tag_.clear();
        setVisible (false);
        if (onDismiss_)
            onDismiss_();
    }

    juce::Rectangle<float> menuRect() const
    {
        const EditorLayout lay (getLocalBounds(), snap_.layout);
        const float s      = lay.scale;
        const int   n      = juce::jmax (1, items_.size());
        const float itemH  = 26.0f * s;
        const float rowGap = 2.0f  * s;
        const float pad    = 6.0f  * s;
        const float h      = (float) n * itemH + (float) (n - 1) * rowGap + pad * 2.0f;
        const float w      = juce::jmax (148.0f * s, anchor_.getWidth());

        float x = anchor_.getX();
        if (x + w > (float) getWidth() - 8.0f * s)
            x = (float) getWidth() - w - 8.0f * s;
        x = juce::jmax (8.0f * s, x);

        float y = anchor_.getBottom() + 5.0f * s;
        if (anchor_.getCentreY() > (float) getHeight() * 0.55f || y + h > (float) getHeight() - 8.0f * s)
            y = anchor_.getY() - h - 5.0f * s;
        y = juce::jlimit (8.0f * s, juce::jmax (8.0f * s, (float) getHeight() - h - 8.0f * s), y);

        return { x, y, w, h };
    }

    juce::Rectangle<float> itemRect (int idx) const
    {
        const EditorLayout lay (getLocalBounds(), snap_.layout);
        const float s      = lay.scale;
        const auto  r      = menuRect();
        const float pad    = 6.0f  * s;
        const float itemH  = 26.0f * s;
        const float rowGap = 2.0f  * s;
        return { r.getX() + pad,
                 r.getY() + pad + (float) idx * (itemH + rowGap),
                 r.getWidth() - pad * 2.0f,
                 itemH };
    }

    void paint (juce::Graphics& g) override
    {
        if (! open_ || items_.isEmpty())
            return;

        const EditorLayout lay (getLocalBounds(), snap_.layout);
        const float s      = lay.scale;
        const auto& pal    = snap_.palette;
        const auto  r      = menuRect();
        const float pad    = 6.0f * s;
        const float rOuter = (snap_.isNeumorphic() ? snap_.theme.radiusLg : 10.0f) * s;
        const float rEdge  = juce::jmax (4.0f * s, rOuter - pad);
        const float rMid   = 4.0f * s;
        const int   nItems = items_.size();

        if (snap_.isNeumorphic())
        {
            UiPack::NeuPainter::drawRaisedRect (g, r, rOuter, pal, snap_.theme.shadowCard, s, 0.99f, false);
        }
        else
        {
            g.setColour (pal.halo.withMultipliedAlpha (0.46f));
            g.fillRoundedRectangle (r.translated (0.0f, 4.0f * s).expanded (3.0f * s), rOuter + 2.0f * s);
            g.setColour (pal.surface.withAlpha (0.96f));
            g.fillRoundedRectangle (r, rOuter);
            g.setColour (pal.accent.withAlpha (0.60f));
            g.drawRoundedRectangle (r, rOuter, 1.0f * s);
        }

        for (int i = 0; i < nItems; ++i)
        {
            const auto  row  = itemRect (i);
            const bool  cur  = (i == currentIndex_);
            const bool  hot  = (i == hoverIndex_);
            const float rTop = (i == 0)          ? rEdge : rMid;
            const float rBot = (i == nItems - 1) ? rEdge : rMid;

            if (cur || hot)
            {
                const auto rowPath = makeConcentricRowPath (row, rTop, rBot);
                if (snap_.isNeumorphic())
                {
                    const float sh = (cur ? 1.5f : 1.2f) * s;
                    g.setColour (pal.shadowLight.withMultipliedAlpha (cur ? 0.55f : 0.72f));
                    g.fillPath (rowPath, juce::AffineTransform::translation (cur ? sh : -sh, cur ? sh : -sh));
                    g.setColour (pal.shadowDark.withMultipliedAlpha (cur ? 0.45f : 0.48f));
                    g.fillPath (rowPath, juce::AffineTransform::translation (cur ? -sh : sh, cur ? -sh : sh));
                    g.setColour (cur ? pal.bg.interpolatedWith (pal.accent, 0.12f) : pal.bgLight.withAlpha (0.85f));
                    g.fillPath (rowPath);
                }
                else
                {
                    g.setColour (cur ? pal.accent.withAlpha (0.28f) : pal.track.withAlpha (0.80f));
                    g.fillPath (rowPath);
                    if (cur)
                    {
                        g.setColour (pal.accent.withAlpha (0.80f));
                        g.strokePath (rowPath, juce::PathStrokeType (1.0f * s));
                    }
                }
            }

            const juce::Colour fg = cur ? pal.accent : (hot ? pal.text : pal.dim);
            float textLeft = row.getX() + 10.0f * s;

            if (cur)
            {
                g.setColour (pal.accent);
                g.setFont (CompTheme::latinFont (CompTheme::bold(), 12.5f * s));
                g.drawText (juce::String::fromUTF8 ("\xE2\x9C\x93"),
                            juce::Rectangle<float> (textLeft - 2.0f * s, row.getY(), 14.0f * s, row.getHeight()),
                            juce::Justification::centredLeft, false);
                textLeft += 15.0f * s;
            }

            g.setColour (fg);
            g.setFont (useTranslatedFont_
                         ? CompTheme::font (cur ? CompTheme::semiBold() : CompTheme::regular(), 12.5f * s)
                         : CompTheme::latinFont (cur ? CompTheme::semiBold() : CompTheme::regular(), 12.5f * s));
            g.drawText (items_[i],
                        juce::Rectangle<float> (textLeft, row.getY(), row.getRight() - textLeft - 6.0f * s, row.getHeight()),
                        juce::Justification::centredLeft, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int hov = -1;
        for (int i = 0; i < items_.size(); ++i)
        {
            if (itemRect (i).contains (e.position))
            {
                hov = i;
                break;
            }
        }
        if (hov != hoverIndex_)
        {
            hoverIndex_ = hov;
            repaint (menuRect().expanded (8.0f).toNearestInt());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < items_.size(); ++i)
        {
            if (itemRect (i).contains (e.position))
            {
                auto cb = onSelect_;
                close();
                if (cb)
                    cb (i);
                return;
            }
        }
        close();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (! open_)
            return false;
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        if (key == juce::KeyPress::upKey)
        {
            const int base = (hoverIndex_ >= 0) ? hoverIndex_ : currentIndex_;
            hoverIndex_ = juce::jlimit (0, items_.size() - 1, base - 1);
            repaint();
            return true;
        }
        if (key == juce::KeyPress::downKey)
        {
            const int base = (hoverIndex_ >= 0) ? hoverIndex_ : currentIndex_;
            hoverIndex_ = juce::jlimit (0, items_.size() - 1, base + 1);
            repaint();
            return true;
        }
        if (key == juce::KeyPress::returnKey)
        {
            const int pick = (hoverIndex_ >= 0) ? hoverIndex_ : currentIndex_;
            if (pick >= 0 && pick < items_.size())
            {
                auto cb = onSelect_;
                close();
                if (cb)
                    cb (pick);
            }
            return true;
        }
        return false;
    }

private:
    UiPack::UiSnapshot snap_ = UiPack::UiComposition().snapshot();
    bool open_ = false;
    juce::String tag_;
    juce::Rectangle<float> anchor_;
    juce::StringArray items_;
    int currentIndex_ = 0;
    int hoverIndex_   = -1;
    bool useTranslatedFont_ = false;
    std::function<void (int)> onSelect_;
    std::function<void()> onDismiss_;
};
