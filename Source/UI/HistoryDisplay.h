#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"

namespace avel::ui
{
    // Scrolling level / gain-reduction history (the main display of Avel Apex).
    // Input level (post gain) is the dim blue-grey fill, output level the white fill, gain reduction
    // hangs from the top in red, the short-term loudness is the yellow line.
    class HistoryDisplay : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit HistoryDisplay (ApexProcessor& p) : proc (p)
        {
            hist.resize (capacity);
            setTooltip ("Drag up or down anywhere in the display to change Gain. Double-click to reset Gain.");
        }


        // dB range follows the height so the scale stays around 12..16 px per dB
        void setGraphArea (juce::Rectangle<float> r)
        {
            graph = r;
            rangeDb = r.getHeight() >= 620.0f ? 48.0f : r.getHeight() >= 470.0f ? 36.0f : r.getHeight() >= 330.0f ? 30.0f : 24.0f;
            repaint();
        }
        void setBottomStrip (float h) { strip = h; }
        void setSeconds (float s) { seconds = s; repaint(); }
        void setTarget (float lufs) { target = lufs; repaint(); }
        void setShowLoudness (bool b) { showLoudness = b; repaint(); }
        void setAccent (juce::Colour c) { accent = c; }
        void setCardBounds (juce::Rectangle<float> r) { cardBounds = r; repaint(); }

        // Pull new columns from the processor (message thread).
        void pull()
        {
            auto& fifo = proc.historyFifo;
            const int ready = fifo.getNumReady();
            if (ready <= 0) return;
            const auto scope = fifo.read (ready);
            // display ballistics: instant rise, 60 dB/s fall, so the level contour reads like a meter
            const float fall = std::pow (10.0f, -60.0f / 20.0f / (float) juce::jmax (1.0, proc.columnRate.load()));
            auto take = [this, fall] (int start, int size)
            {
                for (int i = 0; i < size; ++i)
                {
                    auto c = proc.history[(size_t) (start + i)];
                    smIn = juce::jmax (c.in, smIn * fall);
                    smOut = juce::jmax (c.out, smOut * fall);
                    c.in = smIn; c.out = smOut;
                    hist[(size_t) (written % capacity)] = c;
                    ++written;
                }
            };
            take (scope.startIndex1, scope.blockSize1);
            take (scope.startIndex2, scope.blockSize2);
            repaint();
        }

        float dbToY (float db) const
        {
            return graph.getY() + (-db / rangeDb) * graph.getHeight();
        }

        void paint (juce::Graphics& g) override
        {
            auto full = getLocalBounds().toFloat();
            const float bottom = full.getBottom() - strip;
            const auto ceilDb = proc.apvts.getRawParameterValue (ids::ceiling)->load();

            {
                juce::Graphics::ScopedSaveState ss (g);
                juce::Path clip; clip.addRoundedRectangle (full, 14.0f);
                g.reduceClipRegion (clip);
                g.reduceClipRegion (full.withBottom (bottom).toNearestInt());

                // grid
                for (float db = 0.0f; db <= rangeDb + 0.01f; db += (db < 12.0f ? 3.0f : 6.0f))
                {
                    const float y = dbToY (-db);
                    g.setColour (juce::Colours::white.withAlpha (db == 0.0f ? 0.075f : 0.03f));
                    g.fillRect (full.getX(), y - 0.5f, full.getWidth(), 1.0f);
                }
                const double colRate = proc.columnRate.load();
                const float cpp = (float) (seconds * colRate / juce::jmax (1.0f, full.getWidth() - gutter));
                // second lines, scrolling with the audio
                if (colRate > 0)
                {
                    const double nowSec = (double) written / colRate;
                    for (int s = 0; s <= (int) seconds + 1; ++s)
                    {
                        const double t = std::floor (nowSec) - s;
                        const float x = full.getRight() - gutter - (float) ((nowSec - t) * colRate / cpp);
                        if (x < full.getX()) break;
                        g.setColour (juce::Colours::white.withAlpha (0.03f));
                        g.fillRect (x - 0.5f, graph.getY(), 1.0f, bottom - graph.getY());
                    }
                }

                drawHistory (g, full.withTrimmedRight (gutter), bottom, cpp);

                // target loudness
                if (showLoudness && target < 0.0f)
                {
                    const float y = dbToY (target);
                    juce::Path dash; dash.startNewSubPath (full.getX(), y); dash.lineTo (full.getRight(), y);
                    juce::Path d; const float pat[] { 4.0f, 4.0f };
                    juce::PathStrokeType (1.0f).createDashedStroke (d, dash, pat, 2);
                    g.setColour (col::yellow.withAlpha (0.30f));
                    g.fillPath (d);
                }

                // ceiling
                {
                    const float y = dbToY (ceilDb);
                    juce::Path dash; dash.startNewSubPath (full.getX(), y); dash.lineTo (full.getRight(), y);
                    juce::Path d; const float pat[] { 6.0f, 4.0f };
                    juce::PathStrokeType (1.0f).createDashedStroke (d, dash, pat, 2);
                    g.setColour (juce::Colours::white.withAlpha (0.38f));
                    g.fillPath (d);
                }

                // label gutter on the right
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRect (full.getRight() - gutter, graph.getY(), 1.0f, bottom - graph.getY());

                // fade the oldest part into the panel
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff131316), full.getX(), 0, juce::Colour (0x00131316), full.getX() + 60.0f, 0, false));
                g.fillRect (full.withWidth (60.0f).withBottom (bottom));
            }

            // dB labels on the right
            g.setFont (font (10.5f, Weight::medium));
            for (float db = 0.0f; db <= rangeDb + 0.01f; db += (db < 12.0f ? 3.0f : 6.0f))
            {
                const float y = dbToY (-db);
                g.setColour (col::label3);
                g.drawText (db == 0.0f ? "0" : juce::String (-db, 0).replace ("-", juce::String::charToString (0x2212)),
                            juce::Rectangle<float> (full.getRight() - gutter + 4.0f, y - 7.0f, gutter - 14.0f, 14.0f), juce::Justification::centredRight, false);
            }

            // ceiling + gain pills on the left
            {
                const float y = dbToY (ceilDb);
                auto pill = juce::Rectangle<float> (full.getX() + 12.0f, y - 12.0f, 0, 24.0f);
                const auto caption = juce::String ("CEILING");
                const auto value = dbString (ceilDb) + " dB";
                drawLabelPill (g, pill, caption, value, col::label);
            }
            {
                const float gain = proc.apvts.getRawParameterValue (ids::gain)->load();
                const float a = juce::jlimit (0.0f, 1.0f, 0.55f + gainHover * 0.45f);
                const float y = graph.getBottom() - 30.0f;
                auto pill = juce::Rectangle<float> (full.getX() + 12.0f, y - 12.0f, 0, 24.0f);
                drawLabelPill (g, pill, "GAIN", dbString (gain) + " dB", col::label.withAlpha (a), true);
            }

            // shadow of the floating card (the card itself is a child component)
            if (! cardBounds.isEmpty())
            {
                juce::Path p; p.addRoundedRectangle (cardBounds, 18.0f);
                juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 30, { 0, 12 }).drawForPath (g, p);
                juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 4, { 0, 1 }).drawForPath (g, p);
            }

            // time labels in the bottom strip
            g.setFont (font (10.5f, Weight::medium));
            g.setColour (col::label3);
            const int step = seconds > 10.0f ? 2 : 1;
            for (int s = 0; s <= (int) seconds; s += step)
            {
                const float x = full.getRight() - gutter - (float) s / seconds * (full.getWidth() - gutter);
                if (x < full.getX() + 20.0f) break;
                const auto t = s == 0 ? juce::String ("now") : juce::String::charToString (0x2212) + juce::String (s) + " s";
                g.drawText (t, juce::Rectangle<float> (x - 30.0f, bottom, 60.0f, strip), juce::Justification::centred, false);
            }
        }

        // gain dragging
        void mouseEnter (const juce::MouseEvent&) override { hoverTarget = 1.0f; }
        void mouseExit (const juce::MouseEvent&) override  { hoverTarget = dragging ? 1.0f : 0.0f; }
        void tick() { if (approach (gainHover, dragging ? 1.0f : hoverTarget, 0.3f)) repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            auto* p = proc.apvts.getParameter (ids::gain);
            proc.undo.beginNewTransaction();
            p->beginChangeGesture();
            dragStartGain = proc.apvts.getRawParameterValue (ids::gain)->load();
            dragStartY = e.position.y;
            dragging = true;
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging) return;
            const float pxPerDb = graph.getHeight() / rangeDb;
            const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
            const float v = juce::jlimit (0.0f, 30.0f, dragStartGain + (dragStartY - e.position.y) / pxPerDb * fine);
            auto* p = proc.apvts.getParameter (ids::gain);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragging) proc.apvts.getParameter (ids::gain)->endChangeGesture();
            dragging = false;
            if (! isMouseOver()) hoverTarget = 0.0f;
        }
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            auto* p = proc.apvts.getParameter (ids::gain);
            proc.undo.beginNewTransaction();
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }

        static juce::String dbString (float v, int decimals = 1)
        {
            if (std::abs (v) < 0.05f) v = 0.0f;
            auto s = juce::String (std::abs (v), decimals);
            return v > 0.0f ? "+" + s : v < 0.0f ? juce::String::charToString (0x2212) + s : s;
        }

        static void drawLabelPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& caption, const juce::String& value,
                                   juce::Colour valueColour, bool updown = false)
        {
            const auto cf = captionFont();
            const auto vf = valueFont();
            const float w = textWidth (cf, caption) + textWidth (vf, value) + 30.0f + (updown ? 12.0f : 0.0f);
            r.setWidth (w);
            drawPill (g, r);
            auto c = r.reduced (11.0f, 0.0f);
            g.setFont (cf);
            g.setColour (col::label3);
            g.drawText (caption, c.removeFromLeft (textWidth (cf, caption) + 7.0f), juce::Justification::centredLeft, false);
            if (updown)
            {
                auto a = c.removeFromRight (10.0f);
                juce::Path p; p.startNewSubPath (8, 9); p.lineTo (12, 5); p.lineTo (16, 9); p.startNewSubPath (8, 15); p.lineTo (12, 19); p.lineTo (16, 15);
                icon::draw (g, p, a.withSizeKeepingCentre (12.0f, 12.0f), col::label3, 1.6f);
            }
            g.setFont (vf);
            g.setColour (valueColour);
            g.drawText (value, c, juce::Justification::centredLeft, false);
        }

    private:
        void drawHistory (juce::Graphics& g, juce::Rectangle<float> full, float bottom, float cpp)
        {
            const juce::int64 avail = juce::jmin (written, (juce::int64) capacity);
            if (avail < 2) return;
            const int bucket = juce::jmax (1, (int) std::floor (cpp));
            const juce::int64 newest = written - 1;
            const juce::int64 firstBucket = newest / bucket;

            juce::Path inFill, outFill, outLine, grFill, grLine, luLine;
            bool started = false, luStarted = false;
            float lastX = full.getRight();
            float firstX = full.getRight();

            auto lvY = [&] (float lin) { return juce::jlimit (graph.getY(), bottom + 2.0f, dbToY (lin > 1.0e-6f ? 20.0f * std::log10 (lin) : -200.0f)); };
            auto grY = [&] (float db) { return juce::jlimit (graph.getY(), bottom, graph.getY() + db / rangeDb * graph.getHeight()); };

            for (juce::int64 b = firstBucket;; --b)
            {
                const juce::int64 c0 = b * bucket, c1 = juce::jmin (newest, c0 + bucket - 1);
                if (c0 < written - avail || c0 < 0) break;
                Column m { 0, 0, 0, -100.0f, -100.0f };
                for (juce::int64 c = c0; c <= c1; ++c)
                {
                    const auto& h = hist[(size_t) (c % capacity)];
                    m.in = juce::jmax (m.in, h.in); m.out = juce::jmax (m.out, h.out); m.gr = juce::jmax (m.gr, h.gr);
                    m.shortTerm = h.shortTerm;
                }
                const float x = full.getRight() - (float) (newest - c0) / cpp;
                if (x < full.getX() - 2.0f) break;
                const float yi = lvY (m.in), yo = lvY (m.out), yg = grY (m.gr);
                if (! started)
                {
                    inFill.startNewSubPath (x, bottom + 2.0f); inFill.lineTo (x, yi);
                    outFill.startNewSubPath (x, bottom + 2.0f); outFill.lineTo (x, yo);
                    outLine.startNewSubPath (x, yo);
                    grFill.startNewSubPath (x, graph.getY()); grFill.lineTo (x, yg);
                    grLine.startNewSubPath (x, yg);
                    firstX = x;
                    started = true;
                }
                else
                {
                    inFill.lineTo (x, yi); outFill.lineTo (x, yo); outLine.lineTo (x, yo);
                    grFill.lineTo (x, yg); grLine.lineTo (x, yg);
                }
                if (showLoudness && m.shortTerm > -60.0f)
                {
                    const float yl = juce::jlimit (graph.getY(), bottom, dbToY (m.shortTerm));
                    if (! luStarted) { luLine.startNewSubPath (x, yl); luStarted = true; }
                    else luLine.lineTo (x, yl);
                }
                else luStarted = false;
                lastX = x;
            }
            if (! started) return;
            inFill.lineTo (lastX, bottom + 2.0f); inFill.closeSubPath();
            outFill.lineTo (lastX, bottom + 2.0f); outFill.closeSubPath();
            grFill.lineTo (lastX, graph.getY()); grFill.closeSubPath();
            juce::ignoreUnused (firstX);

            const juce::Colour inCol (0xff7d8aa6);
            g.setGradientFill (juce::ColourGradient (inCol.withAlpha (0.42f), 0, graph.getY(), inCol.withAlpha (0.0f), 0, bottom, false));
            g.fillPath (inFill);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.24f), 0, graph.getY(),
                                                     juce::Colours::white.withAlpha (0.0f), 0, bottom, false));
            g.fillPath (outFill);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.strokePath (outLine, juce::PathStrokeType (1.0f));

            g.setGradientFill (juce::ColourGradient (col::red.withAlpha (0.55f), 0, graph.getY(),
                                                     col::red.withAlpha (0.20f), 0, graph.getY() + graph.getHeight() * 0.4f, false));
            g.fillPath (grFill);
            g.setColour (col::red);
            g.strokePath (grLine, juce::PathStrokeType (1.25f));

            if (showLoudness)
            {
                g.setColour (col::yellow.withAlpha (0.25f));
                g.strokePath (luLine, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (col::yellow.withAlpha (0.95f));
                g.strokePath (luLine, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        using Column = ApexProcessor::Column;
        ApexProcessor& proc;
        static constexpr int capacity = 8192;
        std::vector<Column> hist;
        juce::int64 written = 0;
        juce::Rectangle<float> graph, cardBounds;
        static constexpr float gutter = 40.0f;
        float rangeDb = 30.0f;
        float smIn = 0.0f, smOut = 0.0f, hoverTarget = 0.0f, strip = 22.0f, seconds = 8.0f, target = -14.0f, gainHover = 0.0f, dragStartGain = 0.0f, dragStartY = 0.0f;
        bool showLoudness = true, dragging = false;
        juce::Colour accent { col::accent };
    };
}
