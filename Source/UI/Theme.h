#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "BinaryData.h"

// Avel design system (shared by Avel EQ, Avel Ember, Avel Apex, ...).
// Keep this file identical across products; product-specific colours live in the editor.
namespace avel::ui
{
    namespace col
    {
        const juce::Colour bg0       { 0xff0b0b0d };
        const juce::Colour bg1       { 0xff151518 };
        const juce::Colour bg2       { 0xff1c1c1f };
        const juce::Colour glass     { 0xe6202023 };
        const juce::Colour glassHi   { 0xff2c2c2f };
        const juce::Colour stroke    { 0x17ffffff };
        const juce::Colour strokeHi  { 0x26ffffff };
        const juce::Colour fill      { 0x33767680 };
        const juce::Colour fillHi    { 0x59767680 };
        const juce::Colour thumb     { 0xff636366 };
        const juce::Colour label     { 0xfff5f5f7 };
        const juce::Colour label2    { 0x99ebebf5 };
        const juce::Colour label3    { 0x4debebf5 };
        const juce::Colour label4    { 0x29ebebf5 };
        const juce::Colour accent    { 0xff0a84ff };
        const juce::Colour green     { 0xff30d158 };
        const juce::Colour red       { 0xffff453a };
        const juce::Colour yellow    { 0xffffd60a };
        const juce::Colour orange    { 0xffff9f0a };
        const juce::Colour pill      { 0xf0232326 };

        inline juce::Colour band (int index)
        {
            static const juce::Colour c[] {
                juce::Colour (0xff0a84ff), juce::Colour (0xffff9f0a), juce::Colour (0xff30d158), juce::Colour (0xffbf5af2),
                juce::Colour (0xffff375f), juce::Colour (0xff64d2ff), juce::Colour (0xffffd60a), juce::Colour (0xff5e5ce6),
                juce::Colour (0xff66d4cf), juce::Colour (0xffff6961), juce::Colour (0xffac8e68), juce::Colour (0xffda8fff)
            };
            return c[(size_t) ((index % 12) + 12) % 12];
        }
    }

    enum class Weight { regular, medium, semibold };

    inline juce::Typeface::Ptr typeface (Weight w)
    {
        static juce::Typeface::Ptr r = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, BinaryData::InterRegular_otfSize);
        static juce::Typeface::Ptr m = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_otf, BinaryData::InterMedium_otfSize);
        static juce::Typeface::Ptr s = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, BinaryData::InterSemiBold_otfSize);
        return w == Weight::regular ? r : w == Weight::medium ? m : s;
    }

    inline juce::Font font (float size, Weight w = Weight::medium, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (typeface (w)).withHeight (size).withKerningFactor (kerning));
    }

    inline juce::Font captionFont() { return font (10.0f, Weight::semibold, 0.06f); }
    inline juce::Font valueFont()   { return font (12.0f, Weight::semibold); }

    inline float textWidth (const juce::Font& f, const juce::String& s)
    {
        return juce::GlyphArrangement::getStringWidth (f, s);
    }

    inline void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, float radius, float shadow = 1.0f)
    {
        if (shadow > 0.0f)
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius);
            juce::DropShadow (juce::Colours::black.withAlpha (0.55f * shadow), (int) (30 * shadow), { 0, (int) (12 * shadow) }).drawForPath (g, p);
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f * shadow), 4, { 0, 1 }).drawForPath (g, p);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xf0262629), r.getX(), r.getY(), juce::Colour (0xf01b1b1e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(),
                                                 juce::Colours::transparentWhite, r.getX(), r.getY() + juce::jmin (40.0f, r.getHeight() * 0.5f), false));
        g.fillRoundedRectangle (r.reduced (1.0f), radius - 1.0f);
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawLine (r.getX() + radius, r.getY() + 0.75f, r.getRight() - radius, r.getY() + 0.75f, 1.0f);
    }

    // Info pill: 0xf0232326 fill, radius 13 (or half height), hairline.
    inline void drawPill (juce::Graphics& g, juce::Rectangle<float> r, float hover = 0.0f)
    {
        const float rad = juce::jmin (13.0f, r.getHeight() * 0.5f);
        g.setColour (col::pill.interpolatedWith (juce::Colour (0xf02e2e31), hover));
        g.fillRoundedRectangle (r, rad);
        g.setColour (col::stroke.interpolatedWith (col::strokeHi, hover));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    }

    // Main display panel background (14 px radius, top radial glow, hairline).
    inline void drawDisplay (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour glow = juce::Colours::white)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17171a), r.getX(), r.getY(), juce::Colour (0xff0c0c0e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 14.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            juce::Path clip; clip.addRoundedRectangle (r, 14.0f);
            g.reduceClipRegion (clip);
            juce::ColourGradient rg (glow.withAlpha (0.035f), r.getCentreX(), r.getY(),
                                     glow.withAlpha (0.0f), r.getCentreX(), r.getY() + r.getHeight() * 0.75f, true);
            g.setGradientFill (rg);
            g.fillRect (r);
        }
        g.setColour (col::stroke);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    }

    inline bool approach (float& v, float target, float speed = 0.25f)
    {
        if (std::abs (v - target) < 0.002f) { const bool changed = v != target; v = target; return changed; }
        v += (target - v) * speed;
        return true;
    }

    namespace icon
    {
        using P = juce::Path;
        inline P undo() { P p; p.startNewSubPath (9, 6); p.lineTo (5, 10); p.lineTo (9, 14); p.startNewSubPath (5, 10); p.lineTo (14, 10);
                          p.cubicTo (17.5f, 10, 20, 12.5f, 20, 15); p.cubicTo (20, 17.5f, 17.5f, 20, 14, 20); p.lineTo (11, 20); return p; }
        inline P redo() { auto p = undo(); p.applyTransform (juce::AffineTransform::scale (-1, 1).translated (25, 0)); return p; }
        inline P trash() { P p; p.startNewSubPath (4.5f, 7); p.lineTo (19.5f, 7); p.startNewSubPath (9.5f, 7); p.lineTo (10, 4.5f); p.lineTo (14, 4.5f); p.lineTo (14.5f, 7);
                           p.startNewSubPath (6.5f, 7); p.lineTo (7.5f, 19.5f); p.lineTo (16.5f, 19.5f); p.lineTo (17.5f, 7);
                           p.startNewSubPath (10.25f, 10.5f); p.lineTo (10.5f, 16.5f); p.startNewSubPath (13.75f, 10.5f); p.lineTo (13.5f, 16.5f); return p; }
        inline P headphones() { P p; p.startNewSubPath (4.5f, 15); p.cubicTo (4.5f, 8, 8, 4.5f, 12, 4.5f); p.cubicTo (16, 4.5f, 19.5f, 8, 19.5f, 15);
                                p.addRoundedRectangle (4.0f, 13.5f, 4.0f, 6.5f, 1.6f); p.addRoundedRectangle (16.0f, 13.5f, 4.0f, 6.5f, 1.6f); return p; }
        inline P power() { P p; p.addCentredArc (12, 13, 7, 7, 0, juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
                           p.startNewSubPath (12, 4); p.lineTo (12, 11.5f); return p; }
        inline P chevronDown() { P p; p.startNewSubPath (7, 10); p.lineTo (12, 15); p.lineTo (17, 10); return p; }
        inline P chevronLeft()  { P p; p.startNewSubPath (14, 6); p.lineTo (8, 12); p.lineTo (14, 18); return p; }
        inline P chevronRight() { P p; p.startNewSubPath (10, 6); p.lineTo (16, 12); p.lineTo (10, 18); return p; }
        inline P copy() { P p; p.addRoundedRectangle (8.5f, 8.5f, 11.0f, 11.0f, 2.5f); p.startNewSubPath (5, 15); p.lineTo (5, 7); p.quadraticTo (5, 5, 7, 5); p.lineTo (15, 5); return p; }
        inline P close() { P p; p.startNewSubPath (7, 7); p.lineTo (17, 17); p.startNewSubPath (17, 7); p.lineTo (7, 17); return p; }
        inline P record() { P p; p.addEllipse (7.5f, 7.5f, 9.0f, 9.0f); return p; }
        inline P pencil() { P p; p.startNewSubPath (5, 19); p.lineTo (6, 15); p.lineTo (15.5f, 5.5f); p.lineTo (18.5f, 8.5f); p.lineTo (9, 18); p.closeSubPath();
                            p.startNewSubPath (13.5f, 7.5f); p.lineTo (16.5f, 10.5f); return p; }
        inline P file() { P p; p.startNewSubPath (7, 4); p.lineTo (14, 4); p.lineTo (18, 8); p.lineTo (18, 20); p.lineTo (7, 20); p.closeSubPath();
                          p.startNewSubPath (14, 4); p.lineTo (14, 8); p.lineTo (18, 8); return p; }
        inline P reset() { P p; p.addCentredArc (12, 12, 7, 7, 0, juce::degreesToRadians (-60.0f), juce::degreesToRadians (240.0f), true);
                           p.startNewSubPath (4.2f, 6.5f); p.lineTo (5.9f, 9.4f); p.lineTo (9.0f, 8.4f); return p; }
        inline P pause() { P p; p.startNewSubPath (9, 6.5f); p.lineTo (9, 17.5f); p.startNewSubPath (15, 6.5f); p.lineTo (15, 17.5f); return p; }
        inline P delta() { P p; p.startNewSubPath (12, 5); p.lineTo (19.5f, 18.5f); p.lineTo (4.5f, 18.5f); p.closeSubPath(); return p; }

        inline void draw (juce::Graphics& g, const P& p, juce::Rectangle<float> area, juce::Colour c, float stroke = 1.6f, float designSize = 24.0f)
        {
            const float s = juce::jmin (area.getWidth(), area.getHeight()) / designSize;
            auto t = juce::AffineTransform::scale (s).translated (area.getCentreX() - designSize * 0.5f * s, area.getCentreY() - designSize * 0.5f * s);
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
        }
    }

    //==============================================================================
    // Small helper: components that animate run a 60 Hz timer only while something moves.
    struct Animated : private juce::Timer
    {
        virtual ~Animated() = default;
        virtual bool animate() = 0;          // returns true while still moving
        void kick() { if (! isTimerRunning()) startTimerHz (60); }
    private:
        void timerCallback() override { if (! animate()) stopTimer(); }
    };

    //==============================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setDefaultSansSerifTypeface (typeface (Weight::medium));
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf0222225));
            setColour (juce::PopupMenu::textColourId, col::label);
            setColour (juce::PopupMenu::headerTextColourId, col::label3);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0262629));
            setColour (juce::TooltipWindow::textColourId, col::label);
            setColour (juce::TooltipWindow::outlineColourId, col::strokeHi);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2a2a2d));
            setColour (juce::TextEditor::textColourId, col::label);
            setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.45f));
            setColour (juce::TextEditor::outlineColourId, col::strokeHi);
            setColour (juce::TextEditor::focusedOutlineColourId, col::accent);
            setColour (juce::CaretComponent::caretColourId, col::accent);
            setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff222225));
            setColour (juce::AlertWindow::textColourId, col::label);
            setColour (juce::AlertWindow::outlineColourId, col::strokeHi);
            setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c2f));
            setColour (juce::TextButton::textColourOffId, col::label);
            setColour (juce::ResizableWindow::backgroundColourId, col::bg1);
        }

        juce::Font getPopupMenuFont() override { return font (13.0f, Weight::medium); }
        int getPopupMenuBorderSize() override { return 6; }

        void drawPopupMenuBackgroundWithOptions (juce::Graphics& g, int w, int h, const juce::PopupMenu::Options&) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
            g.setColour (findColour (juce::PopupMenu::backgroundColourId));
            g.fillRoundedRectangle (r, 9.0f);
            g.setColour (col::strokeHi);
            g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
        }

        void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h) override
        {
            if (isSeparator) { w = 50; h = 9; return; }
            h = standardHeight > 0 ? standardHeight : 26;
            w = (int) textWidth (getPopupMenuFont(), text) + 52;
        }

        void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                                const juce::Drawable*, const juce::Colour* textColour) override
        {
            auto r = area.toFloat().reduced (4.0f, 0.0f);
            if (isSeparator)
            {
                g.setColour (col::stroke);
                g.fillRect (r.withSizeKeepingCentre (r.getWidth() - 8.0f, 1.0f));
                return;
            }
            if (isHighlighted && isActive)
            {
                g.setColour (col::accent);
                g.fillRoundedRectangle (r.reduced (0.0f, 1.0f), 5.0f);
            }
            const auto tc = ! isActive ? col::label3
                          : isHighlighted ? juce::Colours::white
                          : (textColour != nullptr ? *textColour : col::label);
            if (isTicked)
            {
                juce::Path tick;
                tick.startNewSubPath (7, 12.5f); tick.lineTo (10.5f, 16); tick.lineTo (17, 8.5f);
                icon::draw (g, tick, r.withWidth (22.0f).translated (2.0f, 0.0f).withSizeKeepingCentre (14.0f, 14.0f), tc, 1.8f);
            }
            g.setColour (tc);
            g.setFont (getPopupMenuFont());
            g.drawText (text, r.withTrimmedLeft (26.0f).withTrimmedRight (20.0f), juce::Justification::centredLeft, true);
            if (shortcutKeyText.isNotEmpty())
            {
                g.setColour (isHighlighted ? juce::Colours::white.withAlpha (0.7f) : col::label3);
                g.drawText (shortcutKeyText, r.withTrimmedRight (10.0f), juce::Justification::centredRight, true);
            }
            if (hasSubMenu)
                icon::draw (g, icon::chevronRight(), r.removeFromRight (20.0f).withSizeKeepingCentre (12.0f, 12.0f), tc, 1.8f);
        }

        void drawPopupMenuSectionHeaderWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& text,
                                                    const juce::PopupMenu::Options&) override
        {
            g.setFont (captionFont());
            g.setColour (col::label3);
            g.drawText (text.toUpperCase(), area.toFloat().withTrimmedLeft (30.0f), juce::Justification::centredLeft, true);
        }

        juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
        {
            const auto f = font (12.0f, Weight::medium);
            const int w = juce::jmin (320, (int) textWidth (f, text) + 22);
            const int lines = juce::jmax (1, (int) std::ceil (textWidth (f, text) / 298.0f));
            const int h = 10 + lines * 16;
            return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                         pos.y > parent.getCentreY() ? pos.y - (h + 12) : pos.y + 18, w, h).constrainedWithin (parent);
        }

        void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
            g.setColour (juce::Colour (0xf62a2a2d));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (col::strokeHi);
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (col::label);
            g.setFont (font (12.0f, Weight::medium));
            g.drawFittedText (text, r.reduced (11.0f, 5.0f).toNearestInt(), juce::Justification::centredLeft, 6, 1.0f);
        }

        void drawAlertBox (juce::Graphics& g, juce::AlertWindow& w, const juce::Rectangle<int>& textArea, juce::TextLayout& tl) override
        {
            auto r = w.getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff222225));
            g.fillRoundedRectangle (r, 12.0f);
            g.setColour (col::strokeHi);
            g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
            tl.draw (g, textArea.toFloat());
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (down ? col::accent.darker (0.2f) : over ? col::accent.brighter (0.1f) : col::accent);
            g.fillRoundedRectangle (r, 7.0f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (13.0f, Weight::semibold); }
        juce::Font getAlertWindowTitleFont() override   { return font (15.0f, Weight::semibold); }
        juce::Font getAlertWindowMessageFont() override { return font (13.0f, Weight::regular); }
        juce::Font getAlertWindowFont() override        { return font (13.0f, Weight::regular); }
    };

    //==============================================================================
    // Shows a popup menu under a component, in the Avel style.
    inline void showMenu (juce::PopupMenu& m, juce::Component& target, std::function<void (int)> cb, int minWidth = 0)
    {
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target).withMinimumWidth (juce::jmax (minWidth, target.getWidth()))
                                                   .withStandardItemHeight (26),
                         [cb] (int r) { if (cb) cb (r); });
    }

    //==============================================================================
    class Dial : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        Dial (const juce::String& captionText = {}) : caption (captionText) { setWantsKeyboardFocus (false); }

        std::function<void (float)> onChange;              // plain value (bindPlain) or denormalised param value
        std::function<juce::String (float)> textFromValue; // optional override of the value text

        void setCaption (const juce::String& c) { caption = c; repaint(); }
        void setColour (juce::Colour c) { arcColour = c; repaint(); }
        void setBipolar (bool b) { bipolar = b; repaint(); }
        void setDimmed (bool d) { if (dimmed != d) { dimmed = d; repaint(); } }
        void setKnobSize (float s) { knobSize = s; repaint(); }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            param = &p;
            range = p.getNormalisableRange();
            defaultValue = range.convertFrom0to1 (p.getDefaultValue());
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setValueInternal (v, false); }, um);
            attachment->sendInitialUpdate();
        }

        void bindPlain (float minV, float maxV, float def, std::function<juce::String (float)> toText, std::function<void (float)> changed,
                        float skewCentre = 0.0f)
        {
            param = nullptr;
            attachment.reset();
            range = juce::NormalisableRange<float> (minV, maxV);
            if (skewCentre > minV && skewCentre < maxV) range.setSkewForCentre (skewCentre);
            defaultValue = def;
            textFromValue = std::move (toText);
            onChange = std::move (changed);
            setValueInternal (def, false);
        }

        void setValue (float v, bool notify) { setValueInternal (v, notify); }
        float getValue() const { return value; }

        juce::String getValueText() const
        {
            if (textFromValue) return textFromValue (value);
            if (param != nullptr) return param->getText (range.convertTo0to1 (value), 16) + (param->getLabel().isNotEmpty() ? " " + param->getLabel() : juce::String());
            return juce::String (value, 1);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setFont (captionFont());
            g.setColour (col::label3);
            if (caption.isNotEmpty())
                g.drawText (caption.toUpperCase(), b.removeFromTop (14.0f), juce::Justification::centred, false);
            auto valueArea = b.removeFromBottom (18.0f);
            const float size = juce::jmin (knobSize, b.getWidth(), b.getHeight());
            auto k = b.withSizeKeepingCentre (size, size);
            knobArea = k;

            const float start = juce::MathConstants<float>::pi * 1.25f, end = juce::MathConstants<float>::pi * 2.75f;
            const float norm = range.convertTo0to1 (value);
            const float ang = start + norm * (end - start);
            const float ringR = size * 0.5f - 3.0f;
            const auto c = k.getCentre();
            const auto arcCol = dimmed ? col::label3 : arcColour;

            juce::Path track;
            track.addCentredArc (c.x, c.y, ringR, ringR, 0, start, end, true);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float from = bipolar ? (start + end) * 0.5f : start;
            if (std::abs (ang - from) > 0.001f)
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, ringR, ringR, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
                g.setColour (arcCol.withAlpha (0.20f + 0.15f * hover));
                g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (arcCol);
                g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            auto face = k.reduced (8.5f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 8, { 0, 3 }).drawForRectangle (g, face.toNearestInt());
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343437).interpolatedWith (juce::Colour (0xff3d3d41), hover), face.getX(), face.getY(),
                                                     juce::Colour (0xff1d1d20), face.getX(), face.getBottom(), false));
            g.fillEllipse (face);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.drawEllipse (face.reduced (0.5f), 1.0f);

            const float ir0 = face.getWidth() * 0.18f, ir1 = face.getWidth() * 0.40f;
            g.setColour (dimmed ? col::label3 : col::label);
            g.drawLine (c.x + ir0 * std::sin (ang), c.y - ir0 * std::cos (ang), c.x + ir1 * std::sin (ang), c.y - ir1 * std::cos (ang), 2.0f);

            if (! editing)
            {
                g.setFont (valueFont());
                g.setColour (dimmed ? col::label.withAlpha (0.45f) : col::label.withAlpha (0.75f + 0.25f * hover));
                g.drawText (getValueText(), valueArea, juce::Justification::centred, false);
            }
            valueRect = valueArea;
        }

        void resized() override { if (editor) editor->setBounds (valueRect.toNearestInt().withSizeKeepingCentre (juce::jmax (56, getWidth() - 6), 18)); }

        void mouseEnter (const juce::MouseEvent&) override { hoverTarget = 1.0f; kick(); }
        void mouseExit (const juce::MouseEvent&) override  { hoverTarget = 0.0f; kick(); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (valueRect.contains (e.position) && ! knobArea.contains (e.position)) { pendingTextEdit = true; return; }
            pendingTextEdit = false;
            dragStart = range.convertTo0to1 (value);
            lastY = e.position.y;
            if (attachment) attachment->beginGesture();
            gesture = true;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! gesture) return;
            const float speed = e.mods.isShiftDown() || e.mods.isCommandDown() ? 0.0008f : 0.005f;
            dragStart = juce::jlimit (0.0f, 1.0f, dragStart + (lastY - e.position.y) * speed);
            lastY = e.position.y;
            setValueInternal (range.convertFrom0to1 (dragStart), true);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (gesture) { if (attachment) attachment->endGesture(); gesture = false; }
            if (pendingTextEdit && valueRect.contains (e.position) && e.mouseWasClicked()) showEditor();
            pendingTextEdit = false;
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (valueRect.contains (e.position) && ! knobArea.contains (e.position)) return;
            if (attachment) attachment->setValueAsCompleteGesture (defaultValue);
            else setValueInternal (defaultValue, true);
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            const float n = juce::jlimit (0.0f, 1.0f, range.convertTo0to1 (value) + (w.deltaY * (w.isReversed ? -1.0f : 1.0f)) * 0.06f);
            if (attachment) attachment->setValueAsCompleteGesture (range.convertFrom0to1 (n));
            else setValueInternal (range.convertFrom0to1 (n), true);
        }

    private:
        bool animate() override
        {
            const bool moving = approach (hover, hoverTarget, 0.3f);
            repaint();
            return moving;
        }

        void setValueInternal (float v, bool notify)
        {
            v = range.snapToLegalValue (juce::jlimit (range.start, range.end, v));
            if (v == value && ! notify) { repaint(); return; }
            value = v;
            if (notify)
            {
                if (attachment) attachment->setValueAsPartOfGesture (v);
                if (onChange) onChange (v);
            }
            else if (param != nullptr && onChange) onChange (v);
            repaint();
        }

        void showEditor()
        {
            editing = true;
            editor = std::make_unique<juce::TextEditor>();
            editor->setFont (valueFont());
            editor->setJustification (juce::Justification::centred);
            editor->setIndents (2, 1);
            editor->setText (param != nullptr ? param->getText (range.convertTo0to1 (value), 16) : getValueText(), false);
            editor->selectAll();
            addAndMakeVisible (*editor);
            resized();
            editor->grabKeyboardFocus();
            editor->onReturnKey = [this] { commitEditor (true); };
            editor->onEscapeKey = [this] { commitEditor (false); };
            editor->onFocusLost = [this] { commitEditor (true); };
            repaint();
        }

        void commitEditor (bool apply)
        {
            if (! editor) return;
            const auto t = editor->getText().trim();
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Dial> (this)] { if (sp) { sp->editor.reset(); sp->editing = false; sp->repaint(); } });
            if (! apply || t.isEmpty()) return;
            float v;
            if (param != nullptr) v = range.convertFrom0to1 (param->getValueForText (t));
            else v = t.retainCharacters ("0123456789.-+").getFloatValue();
            if (attachment) attachment->setValueAsCompleteGesture (v);
            else setValueInternal (v, true);
        }

        juce::String caption;
        juce::Colour arcColour { col::accent };
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        juce::NormalisableRange<float> range { 0.0f, 1.0f };
        float value = 0.0f, defaultValue = 0.0f, dragStart = 0.0f, lastY = 0.0f, hover = 0.0f, hoverTarget = 0.0f, knobSize = 52.0f;
        bool bipolar = false, dimmed = false, gesture = false, editing = false, pendingTextEdit = false;
        juce::Rectangle<float> valueRect, knobArea;
        std::unique_ptr<juce::TextEditor> editor;
    };

    //==============================================================================
    class Segmented : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        explicit Segmented (juce::StringArray items = {}) : labels (std::move (items)) {}

        std::function<void (int)> onChange;

        void setItems (juce::StringArray items) { labels = std::move (items); repaint(); }
        void setFontSize (float s) { fontSize = s; repaint(); }
        void setItemTooltips (juce::StringArray t) { tips = std::move (t); }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            param = &p;
            if (labels.isEmpty())
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (&p)) labels = c->choices;
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setSelected ((int) std::round (v), false); }, um);
            attachment->sendInitialUpdate();
            thumbX = (float) selected;
        }

        void setSelected (int i, bool notify)
        {
            i = juce::jlimit (0, juce::jmax (0, labels.size() - 1), i);
            if (i == selected && ! notify) return;
            selected = i;
            if (notify)
            {
                if (attachment) attachment->setValueAsCompleteGesture ((float) i);
                if (onChange) onChange (i);
            }
            else if (onChange && param != nullptr) onChange (i);
            kick();
        }

        int getSelected() const { return selected; }

        int getIdealWidth() const
        {
            const auto f = font (fontSize, Weight::semibold);
            float w = 0;
            for (auto& l : labels) w = juce::jmax (w, textWidth (f, l));
            return (int) std::ceil ((w + 22.0f) * (float) labels.size()) + 4;
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const float rad = juce::jmin (8.0f, r.getHeight() * 0.5f);
            g.setColour (col::fill);
            g.fillRoundedRectangle (r, rad);
            if (labels.isEmpty()) return;
            const float segW = (r.getWidth() - 4.0f) / (float) labels.size();
            auto th = juce::Rectangle<float> (r.getX() + 2.0f + thumbX * segW, r.getY() + 2.0f, segW, r.getHeight() - 4.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 4, { 0, 1 }).drawForRectangle (g, th.toNearestInt());
            g.setColour (col::thumb);
            g.fillRoundedRectangle (th, rad - 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawRoundedRectangle (th.reduced (0.5f), rad - 2.0f, 1.0f);

            g.setFont (font (fontSize, Weight::semibold));
            for (int i = 0; i < labels.size(); ++i)
            {
                auto s = juce::Rectangle<float> (r.getX() + 2.0f + (float) i * segW, r.getY(), segW, r.getHeight());
                if (i > 0 && std::abs ((float) i - thumbX) > 0.6f && std::abs ((float) i - 1 - thumbX) > 0.6f)
                {
                    g.setColour (col::stroke);
                    g.fillRect (s.getX() - 0.5f, s.getCentreY() - 6.0f, 1.0f, 12.0f);
                }
                const float sel = juce::jmax (0.0f, 1.0f - std::abs ((float) i - thumbX));
                const float hv = i == hoverIndex ? 0.25f : 0.0f;
                g.setColour (col::label2.interpolatedWith (col::label, juce::jmin (1.0f, sel + hv)));
                g.drawText (labels[i], s, juce::Justification::centred, false);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int h = indexAt (e.position.x);
            if (h != hoverIndex) { hoverIndex = h; repaint(); }
            if (juce::isPositiveAndBelow (h, tips.size())) setTooltip (tips[h]);
        }
        void mouseExit (const juce::MouseEvent&) override { hoverIndex = -1; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override { setSelected (indexAt (e.position.x), true); }

    private:
        int indexAt (float x) const
        {
            if (labels.isEmpty()) return -1;
            return juce::jlimit (0, labels.size() - 1, (int) ((x - 2.0f) / ((float) getWidth() - 4.0f) * (float) labels.size()));
        }

        bool animate() override
        {
            const bool moving = approach (thumbX, (float) selected, 0.3f);
            repaint();
            return moving;
        }

        juce::StringArray labels, tips;
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        int selected = 0, hoverIndex = -1;
        float thumbX = 0.0f, fontSize = 12.0f;
    };

    //==============================================================================
    // iOS toggle with an optional label on its left.
    class Switch : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        explicit Switch (const juce::String& text = {}) : labelText (text) {}

        std::function<void (bool)> onChange;
        void setOnColour (juce::Colour c) { onColour = c; repaint(); }

        void bind (juce::RangedAudioParameter& p, juce::UndoManager* um)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { setState (v >= 0.5f, false); }, um);
            attachment->sendInitialUpdate();
            pos = state ? 1.0f : 0.0f;
            bound = true;
        }

        void setState (bool s, bool notify)
        {
            if (s == state && ! notify) return;
            state = s;
            if (notify)
            {
                if (attachment) attachment->setValueAsCompleteGesture (s ? 1.0f : 0.0f);
                if (onChange) onChange (s);
            }
            else if (bound && onChange) onChange (s);
            kick();
        }

        bool getState() const { return state; }

        int getIdealWidth() const { return 36 + (labelText.isNotEmpty() ? (int) std::ceil (textWidth (font (12.0f, Weight::medium), labelText)) + 9 : 0); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto sw = r.removeFromRight (36.0f).withSizeKeepingCentre (36.0f, 21.0f);
            if (labelText.isNotEmpty())
            {
                g.setFont (font (12.0f, Weight::medium));
                g.setColour (col::label2.interpolatedWith (col::label, hover * 0.6f));
                g.drawText (labelText, r.withTrimmedRight (9.0f), juce::Justification::centredRight, false);
            }
            g.setColour (col::fill.interpolatedWith (onColour, pos));
            g.fillRoundedRectangle (sw, sw.getHeight() * 0.5f);
            auto k = juce::Rectangle<float> (17.0f, 17.0f).withCentre ({ sw.getX() + 10.5f + pos * 15.0f, sw.getCentreY() });
            juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 3, { 0, 1 }).drawForRectangle (g, k.toNearestInt());
            g.setColour (juce::Colours::white);
            g.fillEllipse (k);
        }

        void mouseEnter (const juce::MouseEvent&) override { hoverTarget = 1.0f; kick(); }
        void mouseExit (const juce::MouseEvent&) override  { hoverTarget = 0.0f; kick(); }
        void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition())) setState (! state, true); }

    private:
        bool animate() override
        {
            bool m = approach (pos, state ? 1.0f : 0.0f, 0.3f);
            m = approach (hover, hoverTarget, 0.3f) || m;
            repaint();
            return m;
        }

        juce::String labelText;
        juce::Colour onColour { col::green };
        std::unique_ptr<juce::ParameterAttachment> attachment;
        bool state = false, bound = false;
        float pos = 0.0f, hover = 0.0f, hoverTarget = 0.0f;
    };

    //==============================================================================
    // Stroke icon button (24 px design grid) with optional text; toggle and primary styles.
    class IconButton : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        IconButton() = default;
        IconButton (juce::Path p, const juce::String& tip = {}) : path (std::move (p)) { setTooltip (tip); }

        std::function<void()> onClick;

        void setIcon (juce::Path p) { path = std::move (p); repaint(); }
        void setText (const juce::String& t) { text = t; repaint(); }
        void setToggled (bool t) { if (toggled != t) { toggled = t; kick(); } }
        bool isToggled() const { return toggled; }
        void setPrimary (bool p) { primary = p; repaint(); }
        void setTint (juce::Colour c) { tint = c; repaint(); }

        int getIdealWidth() const
        {
            int w = path.isEmpty() ? 12 : 30;
            if (text.isNotEmpty()) w += (int) std::ceil (textWidth (font (12.0f, Weight::semibold), text)) + (path.isEmpty() ? 12 : 4);
            return w;
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const float rad = juce::jmin (8.0f, r.getHeight() * 0.5f);
            if (primary)
            {
                g.setColour (tint.brighter (0.12f * hover).darker (down ? 0.15f : 0.0f));
                g.fillRoundedRectangle (r, rad);
            }
            else if (hover > 0.001f || tog > 0.001f)
            {
                g.setColour (col::fill.withMultipliedAlpha (juce::jmax (hover, tog * 0.8f)));
                g.fillRoundedRectangle (r, rad);
            }
            const auto c = primary ? juce::Colours::white
                         : col::label2.interpolatedWith (col::label, hover).interpolatedWith (tint, tog);
            auto content = r;
            if (! path.isEmpty())
            {
                auto ia = text.isNotEmpty() ? content.removeFromLeft (r.getHeight()).reduced (5.0f) : content.reduced (5.0f);
                icon::draw (g, path, ia.withSizeKeepingCentre (juce::jmin (ia.getWidth(), 20.0f), juce::jmin (ia.getHeight(), 20.0f)), isEnabled() ? c : col::label4);
            }
            if (text.isNotEmpty())
            {
                g.setFont (font (12.0f, Weight::semibold));
                g.setColour (isEnabled() ? c : col::label4);
                g.drawText (text, path.isEmpty() ? content : content.withTrimmedRight (6.0f), juce::Justification::centred, false);
            }
        }

        void mouseEnter (const juce::MouseEvent&) override { hoverTarget = 1.0f; kick(); }
        void mouseExit (const juce::MouseEvent&) override  { hoverTarget = 0.0f; kick(); }
        void mouseDown (const juce::MouseEvent&) override  { down = true; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            down = false; repaint();
            if (isEnabled() && e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick();
        }

    private:
        bool animate() override
        {
            bool m = approach (hover, hoverTarget, 0.3f);
            m = approach (tog, toggled ? 1.0f : 0.0f, 0.3f) || m;
            repaint();
            return m;
        }

        juce::Path path;
        juce::String text;
        juce::Colour tint { col::accent };
        bool toggled = false, primary = false, down = false;
        float hover = 0.0f, hoverTarget = 0.0f, tog = 0.0f;
    };

    //==============================================================================
    // Pill-shaped dropdown (preset pill, style menu, ...): text + chevron, click opens a menu.
    class PillButton : public juce::Component, public juce::SettableTooltipClient, private Animated
    {
    public:
        std::function<void()> onClick;

        void setText (const juce::String& t, const juce::String& prefix = {}) { text = t; prefixText = prefix; repaint(); }
        void setFontSize (float s) { fontSize = s; repaint(); }
        void setChevron (bool c) { chevron = c; repaint(); }
        void setTextColour (juce::Colour c) { textColour = c; repaint(); }

        int getIdealWidth() const
        {
            const auto f = font (fontSize, Weight::semibold);
            float w = textWidth (f, text) + 28.0f + (chevron ? 16.0f : 0.0f);
            if (prefixText.isNotEmpty()) w += textWidth (font (fontSize, Weight::medium), prefixText) + 5.0f;
            return (int) std::ceil (w);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            drawPill (g, r.reduced (0.5f), hover);
            auto c = r.reduced (14.0f, 0.0f);
            if (chevron)
                icon::draw (g, icon::chevronDown(), c.removeFromRight (14.0f).withSizeKeepingCentre (13.0f, 13.0f), col::label2, 1.8f);
            if (prefixText.isNotEmpty())
            {
                const auto pf = font (fontSize, Weight::medium);
                g.setFont (pf);
                g.setColour (col::label3);
                const float pw = textWidth (pf, prefixText) + 5.0f;
                g.drawText (prefixText, c.removeFromLeft (pw), juce::Justification::centredLeft, false);
            }
            g.setFont (font (fontSize, Weight::semibold));
            g.setColour (textColour.interpolatedWith (juce::Colours::white, hover * 0.3f));
            g.drawText (text, c, juce::Justification::centredLeft, true);
        }

        void mouseEnter (const juce::MouseEvent&) override { hoverTarget = 1.0f; kick(); }
        void mouseExit (const juce::MouseEvent&) override  { hoverTarget = 0.0f; kick(); }
        void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }

    private:
        bool animate() override { const bool m = approach (hover, hoverTarget, 0.3f); repaint(); return m; }
        juce::String text, prefixText;
        juce::Colour textColour { col::label };
        float fontSize = 12.5f, hover = 0.0f, hoverTarget = 0.0f;
        bool chevron = true;
    };
}
