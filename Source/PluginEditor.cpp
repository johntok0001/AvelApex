#include "PluginEditor.h"
#include "Presets.h"

using namespace avel::ui;

namespace
{
    const float targets[] { 0.0f, -9.0f, -11.0f, -14.0f, -16.0f, -18.0f, -23.0f };
    const char* targetNames[] { "Off", "-9 LUFS  (club, loud masters)", "-11 LUFS", "-14 LUFS  (Spotify, YouTube, Tidal)",
                                "-16 LUFS  (Apple Music, podcasts)", "-18 LUFS", "-23 LUFS  (EBU R128 broadcast)" };
    constexpr int numTargets = 7;

    juce::String minus (const juce::String& s) { return s.replace ("-", juce::String::charToString (0x2212)); }

    juce::String lufsText (float v) { return v <= -99.0f ? juce::String::charToString (0x2013) : minus (juce::String (v, 1)); }
}

//==============================================================================
void StatsBar::update()
{
    shortTerm = proc.loudness.getShortTerm();
    integrated = proc.loudness.getIntegrated();
    range = proc.loudness.getRange();
    truePeak = proc.meters.truePeakMax.load();
    grMax = proc.meters.grMax.load();
    repaint();
}

void StatsBar::paint (juce::Graphics& g)
{
    struct Item { const char* caption; juce::String value; const char* unit; juce::Colour colour; };
    const float ceil = proc.apvts.getRawParameterValue (ids::ceiling)->load();

    juce::Colour iCol = col::label;
    if (target < 0.0f && integrated > -99.0f)
    {
        const float d = integrated - target;
        iCol = std::abs (d) <= 1.0f ? col::green : d > 1.0f ? col::orange : col::label;
    }
    const auto tpCol = truePeak > ceil + 0.05f ? col::red : col::label;

    const Item items[] {
        { "SHORT-TERM", lufsText (shortTerm), "LUFS", col::label },
        { "INTEGRATED", lufsText (integrated), "LUFS", iCol },
        { "RANGE", integrated <= -99.0f ? juce::String::charToString (0x2013) : juce::String (range, 1), "LU", col::label },
        { "TRUE PEAK", truePeak <= -99.0f ? juce::String::charToString (0x2013) : HistoryDisplay::dbString (truePeak), "dBTP", tpCol },
        { "MAX GR", grMax < 0.05f ? juce::String ("0.0") : minus ("-" + juce::String (grMax, 1)), "dB", grMax > 0.05f ? col::red.brighter (0.3f) : col::label },
    };

    const auto cf = captionFont(), vf = valueFont(), uf = font (10.5f, Weight::medium);
    float x = 0;
    for (auto& it : items)
    {
        const float w = textWidth (cf, it.caption) + textWidth (vf, it.value) + textWidth (uf, it.unit) + 34.0f;
        if (x + w > (float) getWidth()) break;   // narrow window: show what fits
        auto r = juce::Rectangle<float> (x, 0, w, (float) getHeight()).reduced (0.5f);
        drawPill (g, r, flash * 0.8f);
        auto c = r.reduced (11.0f, 0);
        g.setFont (cf); g.setColour (col::label3);
        g.drawText (it.caption, c.removeFromLeft (textWidth (cf, it.caption) + 7.0f), juce::Justification::centredLeft, false);
        g.setFont (vf); g.setColour (it.colour);
        g.drawText (it.value, c.removeFromLeft (textWidth (vf, it.value) + 4.0f), juce::Justification::centredLeft, false);
        g.setFont (uf); g.setColour (col::label3);
        g.drawText (it.unit, c, juce::Justification::centredLeft, false);
        x += w + 6.0f;
    }
}

//==============================================================================
void MeterColumn::update (float dt)
{
    auto toDb = [] (float v) { return v > 1.0e-5f ? 20.0f * std::log10 (v) : -100.0f; };
    const float g = proc.meters.gr.exchange (0.0f);
    gr = g > gr ? g : gr + (g - gr) * juce::jmin (1.0f, dt * 12.0f);
    holdGr = juce::jmax (holdGr, g);
    for (int c = 0; c < 2; ++c)
    {
        const float v = toDb (proc.meters.out[(size_t) c].exchange (0.0f));
        out[c] = v > out[c] ? v : juce::jmax (-100.0f, out[c] - dt * 24.0f);  // 24 dB/s fall
        if (v >= peakOut[c]) { peakOut[c] = v; peakAge[c] = 0; }
        else if ((peakAge[c] += dt) > 1.5f) peakOut[c] = juce::jmax (-100.0f, peakOut[c] - dt * 30.0f);
        holdOut = juce::jmax (holdOut, v);
        proc.meters.in[(size_t) c].exchange (0.0f);
    }
    repaint();
}

void MeterColumn::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // readouts: two rows, caption left, value right
    const float ceil = proc.apvts.getRawParameterValue (ids::ceiling)->load();
    auto row = [&] (const char* caption, const juce::String& value, juce::Colour c)
    {
        auto rr = r.removeFromTop (16.0f);
        g.setFont (captionFont());
        g.setColour (col::label3);
        g.drawText (caption, rr, juce::Justification::centredLeft, false);
        g.setFont (font (11.0f, Weight::semibold));
        g.setColour (c);
        g.drawText (value, rr, juce::Justification::centredRight, false);
    };
    row ("GR", holdGr > 0.05f ? minus ("-" + juce::String (holdGr, 1)) : "0.0", holdGr > 0.05f ? col::red.brighter (0.35f) : col::label2);
    row ("OUT", holdOut <= -99.0f ? juce::String::charToString (0x2013) : HistoryDisplay::dbString (holdOut), holdOut > ceil + 0.05f ? col::red : col::label2);

    r.removeFromTop (10.0f);
    auto bars = r;
    auto grBar = juce::Rectangle<float> (bars.getX() + 1.0f, bars.getY(), 10.0f, bars.getHeight());
    auto lBar = juce::Rectangle<float> (bars.getX() + 19.0f, bars.getY(), 11.0f, bars.getHeight());
    auto rBar = lBar.translated (14.0f, 0);

    for (auto b : { grBar, lBar, rBar })
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (b, 3.0f);
    }
    // gain reduction hangs from the top
    if (gr > 0.01f)
    {
        auto f = grBar.withBottom (toY (-gr, grBar));
        g.setGradientFill (juce::ColourGradient (col::red, 0, f.getY(), col::red.withAlpha (0.65f), 0, f.getBottom() + 1, false));
        g.fillRoundedRectangle (f, 3.0f);
    }
    for (int c = 0; c < 2; ++c)
    {
        auto b = c == 0 ? lBar : rBar;
        if (out[c] > -range)
        {
            auto f = b.withTop (toY (out[c], b));
            const float yC = toY (ceil, b);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e8ee), 0, b.getY(), juce::Colour (0xff8a8a96), 0, b.getBottom(), false));
            g.fillRoundedRectangle (f, 3.0f);
            if (f.getY() < yC) { g.setColour (col::red); g.fillRect (f.withBottom (yC)); }
        }
        if (peakOut[c] > -range)
        {
            g.setColour (peakOut[c] > ceil + 0.05f ? col::red : col::label);
            g.fillRect (b.getX(), toY (peakOut[c], b) - 0.75f, b.getWidth(), 1.5f);
        }
    }
    // ceiling mark
    {
        const float yC = toY (ceil, lBar);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRect (lBar.getX() - 3.0f, yC - 0.5f, rBar.getRight() - lBar.getX() + 6.0f, 1.0f);
    }
    // scale
    g.setFont (font (10.0f, Weight::medium));
    for (float db : { 0.0f, -3.0f, -6.0f, -9.0f, -12.0f, -18.0f, -24.0f, -30.0f })
    {
        const float y = toY (db, lBar);
        g.setColour (col::label3);
        g.drawText (db == 0.0f ? "0" : minus (juce::String (db, 0)), juce::Rectangle<float> (rBar.getRight() + 2.0f, y - 6.0f, r.getRight() - rBar.getRight() - 2.0f, 12.0f), juce::Justification::centredRight, false);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (grBar.getRight() + 2.0f, y - 0.5f, 4.0f, 1.0f);
    }
}

//==============================================================================
Card::Card (ApexProcessor& p) : proc (p)
{
    auto& s = p.apvts;
    auto* um = &p.undo;

    style.setFontSize (13.0f);
    style.onClick = [this] { showStyleMenu(); };
    style.setTooltip ("Limiting style: how transients are shaped and how the release adapts to the material.");
    addAndMakeVisible (style);

    for (auto* d : { &lookahead, &attack, &release, &linkT, &linkR })
    {
        d->setKnobSize (50.0f);
        addAndMakeVisible (*d);
    }
    lookahead.bind (*s.getParameter (ids::lookahead), um);
    attack.bind (*s.getParameter (ids::attack), um);
    release.bind (*s.getParameter (ids::release), um);
    linkT.bind (*s.getParameter (ids::linkTrans), um);
    linkR.bind (*s.getParameter (ids::linkRel), um);
    lookahead.setTooltip ("How far ahead the limiter looks. Longer is cleaner, shorter keeps more snap (0 ms acts like a clipper).");
    attack.setTooltip ("How fast the slow stage reacts to sustained level. Shorter holds the level more firmly.");
    release.setTooltip ("How fast the gain recovers. Short is louder and denser, long is more transparent.");
    linkT.setTooltip ("Channel link for transients. 100 % reduces both channels together, lower lets each side limit on its own.");
    linkR.setTooltip ("Channel link for the release. Lower values keep the stereo image wider during recovery.");
    linkT.textFromValue = [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; };
    linkR.textFromValue = linkT.textFromValue;

    truePeak.bind (*s.getParameter (ids::truePeak), um);
    dc.bind (*s.getParameter (ids::dcFilter), um);
    audition.bind (*s.getParameter (ids::audition), um);
    audition.setOnColour (col::yellow);
    truePeak.setTooltip ("Detect and limit inter-sample peaks so the output stays below the ceiling in dBTP.");
    dc.setTooltip ("Removes DC offset and sub-sonic rumble below 4 Hz before limiting.");
    audition.setTooltip ("Audition limiting: listen only to what the limiter removes.");
    for (auto* w : { &truePeak, &dc, &audition }) addAndMakeVisible (*w);

    dither.setFontSize (12.0f);
    dither.onClick = [this] { showDitherMenu(); };
    dither.setTooltip ("Dither for the final bit depth. Use it only on the last plugin before exporting to 16 or 24 bit.");
    addAndMakeVisible (dither);
    refresh();
}

void Card::refresh()
{
    const int st = (int) proc.apvts.getRawParameterValue (ids::style)->load();
    const int di = (int) proc.apvts.getRawParameterValue (ids::dither)->load();
    const int sh = (int) proc.apvts.getRawParameterValue (ids::shaping)->load();
    if (st != lastStyle) { lastStyle = st; style.setText (avel::dsp::styles()[(size_t) juce::jlimit (0, 7, st)].name); repaint(); }
    if (di != lastDither || sh != lastShaping)
    {
        lastDither = di; lastShaping = sh;
        static const char* names[] { "Off", "24 bit", "20 bit", "18 bit", "16 bit" };
        dither.setText (juce::String (names[juce::jlimit (0, 4, di)]) + (di > 0 && sh > 0 ? " shaped" : ""), "Dither");
        dither.setTextColour (di > 0 ? col::label : col::label2);
    }
}

int Card::getIdealWidth() const { return 16 + 160 + 33 + 3 * 68 + 33 + 2 * 68 + 33 + 172 + 16; }

void Card::resized()
{
    auto r = getLocalBounds().reduced (16, 0);
    const int top = 32, ctrlH = getHeight() - top - 10;
    auto s0 = r.removeFromLeft (160);
    r.removeFromLeft (33);
    auto s1 = r.removeFromLeft (3 * 68);
    r.removeFromLeft (33);
    auto s2 = r.removeFromLeft (2 * 68);
    r.removeFromLeft (33);
    auto s3 = r.removeFromLeft (172);
    sections[0] = s0.toFloat(); sections[1] = s1.toFloat(); sections[2] = s2.toFloat(); sections[3] = s3.toFloat();

    style.setBounds (s0.getX(), top + 2, 160, 32);
    int x = s1.getX();
    for (auto* d : { &lookahead, &attack, &release }) { d->setBounds (x, top - 2, 68, ctrlH + 2); x += 68; }
    x = s2.getX();
    for (auto* d : { &linkT, &linkR }) { d->setBounds (x, top - 2, 68, ctrlH + 2); x += 68; }

    int y = top;
    for (auto* w : { &truePeak, &dc, &audition })
    {
        w->setBounds (s3.getX(), y, s3.getWidth(), 24);
        y += 26;
    }
    dither.setBounds (s3.getX(), y + 2, s3.getWidth(), 26);
}

void Card::paint (juce::Graphics& g)
{
    drawGlass (g, getLocalBounds().toFloat(), 18.0f, 0.0f);
    const char* headers[] { "STYLE", "ENVELOPE", "CHANNEL LINK", "OPTIONS" };
    g.setFont (captionFont());
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (col::label3);
        g.drawText (headers[i], sections[i].withY (12.0f).withHeight (12.0f), i == 0 || i == 3 ? juce::Justification::centredLeft : juce::Justification::centred, false);
        if (i > 0)
        {
            g.setColour (col::stroke);
            g.fillRect (sections[i].getX() - 17.0f, 14.0f, 1.0f, (float) getHeight() - 28.0f);
        }
    }
    // style description
    const auto& st = avel::dsp::styles()[(size_t) juce::jlimit (0, 7, lastStyle)];
    g.setFont (font (11.0f, Weight::regular));
    g.setColour (col::label2);
    g.drawFittedText (st.description, juce::Rectangle<float> (sections[0].getX() + 2.0f, 74.0f, sections[0].getWidth() - 2.0f, (float) getHeight() - 84.0f).toNearestInt(),
                      juce::Justification::topLeft, 4, 1.0f);
}

void Card::showStyleMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Style");
    const int cur = (int) proc.apvts.getRawParameterValue (ids::style)->load();
    for (int i = 0; i < (int) avel::dsp::styles().size(); ++i)
        m.addItem (i + 1, avel::dsp::styles()[(size_t) i].name, true, i == cur);
    showMenu (m, style, [this] (int r)
    {
        if (r <= 0) return;
        auto* p = proc.apvts.getParameter (ids::style);
        proc.undo.beginNewTransaction();
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (r - 1)));
        p->endChangeGesture();
    }, 180);
}

void Card::showDitherMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Dither");
    const int cur = (int) proc.apvts.getRawParameterValue (ids::dither)->load();
    const bool shaped = proc.apvts.getRawParameterValue (ids::shaping)->load() >= 0.5f;
    static const char* names[] { "Off", "24 bit", "20 bit", "18 bit", "16 bit" };
    for (int i = 0; i < 5; ++i) m.addItem (i + 1, names[i], true, i == cur);
    m.addSeparator();
    m.addItem (100, "Noise shaping", true, shaped);
    showMenu (m, dither, [this] (int r)
    {
        if (r <= 0) return;
        proc.undo.beginNewTransaction();
        auto set = [this] (const char* id, float v)
        {
            auto* p = proc.apvts.getParameter (id);
            p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture();
        };
        if (r == 100) set (ids::shaping, proc.apvts.getRawParameterValue (ids::shaping)->load() >= 0.5f ? 0.0f : 1.0f);
        else set (ids::dither, (float) (r - 1));
    }, 172);
}

//==============================================================================
ApexEditor::ApexEditor (ApexProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), stats (p), card (p), meters (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    tooltips.setOpaque (false);

    auto* um = &p.undo;

    preset.onClick = [this] { showPresetMenu(); };
    preset.setTooltip ("Presets");
    addAndMakeVisible (preset);
    undoBtn.onClick = [this] { proc.undo.undo(); };
    redoBtn.onClick = [this] { proc.undo.redo(); };
    addAndMakeVisible (undoBtn);
    addAndMakeVisible (redoBtn);

    oversampling.setItems ({ "Off", juce::String::fromUTF8 ("2\xc3\x97"), juce::String::fromUTF8 ("4\xc3\x97"), juce::String::fromUTF8 ("8\xc3\x97"), juce::String::fromUTF8 ("16\xc3\x97") });
    oversampling.bind (*p.apvts.getParameter (ids::oversample), um);
    oversampling.setTooltip ("Oversampling: limits at a higher sample rate for fewer aliasing artefacts. Costs CPU and latency.");
    addAndMakeVisible (oversampling);

    ab.setSelected (p.getSlot(), false);
    ab.onChange = [this] (int i) { proc.selectSlot (i); };
    ab.setTooltip ("Two settings slots to compare");
    addAndMakeVisible (ab);
    copyBtn.onClick = [this] { proc.copyToOtherSlot(); };
    addAndMakeVisible (copyBtn);

    unity.bind (*p.apvts.getParameter (ids::unity), um);
    unity.setOnColour (col::accent);
    unity.setTooltip ("Unity Gain: hear the limiter at the input level, without the loudness gain, to judge its sound fairly.");
    addAndMakeVisible (unity);

    bypassBtn.onClick = [this]
    {
        auto* b = proc.apvts.getParameter (ids::bypass);
        proc.undo.beginNewTransaction();
        b->beginChangeGesture(); b->setValueNotifyingHost (b->getValue() >= 0.5f ? 0.0f : 1.0f); b->endChangeGesture();
    };
    bypassBtn.setTint (col::orange);
    addAndMakeVisible (bypassBtn);

    display.setAccent (accentA());
    addAndMakeVisible (display);
    display.addAndMakeVisible (stats);
    targetPill.setFontSize (12.0f);
    targetPill.onClick = [this]
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Loudness target");
        const int cur = (int) proc.uiState.getProperty ("target", 3);
        for (int i = 0; i < numTargets; ++i) m.addItem (i + 1, minus (targetNames[i]), true, i == cur);
        m.addSeparator();
        const bool line = (bool) proc.uiState.getProperty ("loudnessLine", true);
        m.addItem (100, "Show short-term loudness", true, line);
        m.addItem (101, "Reset loudness meter");
        showMenu (m, targetPill, [this] (int r)
        {
            if (r == 100) proc.uiState.setProperty ("loudnessLine", ! (bool) proc.uiState.getProperty ("loudnessLine", true), nullptr);
            else if (r == 101) proc.resetLoudness();
            else if (r > 0) proc.uiState.setProperty ("target", r - 1, nullptr);
            updateTargetPill();
        }, 260);
    };
    targetPill.setTooltip ("Loudness target: the yellow dashed line, and the Integrated value turns green within 1 LU of it.");
    display.addAndMakeVisible (targetPill);
    speed.setFontSize (11.5f);
    speed.setSelected ((int) p.uiState.getProperty ("speed", 1), false);
    speed.onChange = [this] (int i) { proc.uiState.setProperty ("speed", i, nullptr); display.setSeconds ((float) (4 << i)); };
    speed.setTooltip ("Time shown in the display");
    display.addAndMakeVisible (speed);
    display.setSeconds ((float) (4 << speed.getSelected()));
    display.addAndMakeVisible (card);
    updateTargetPill();

    gain.bind (*p.apvts.getParameter (ids::gain), um);
    gain.setColour (accentA());
    gain.setKnobSize (54.0f);
    gain.setTooltip ("Gain into the limiter. More gain means more loudness and more limiting. You can also drag in the display.");
    gain.textFromValue = [] (float v) { return HistoryDisplay::dbString (v) + " dB"; };
    output.textFromValue = [this] (float v) { return HistoryDisplay::dbString (v) + (proc.apvts.getRawParameterValue (ids::truePeak)->load() >= 0.5f ? " dBTP" : " dB"); };
    output.bind (*p.apvts.getParameter (ids::ceiling), um);
    output.setColour (col::label2);
    output.setKnobSize (54.0f);
    output.setTooltip ("Output level: the ceiling the output never exceeds (dBTP with True Peak on).");
    addAndMakeVisible (gain);
    addAndMakeVisible (output);
    addAndMakeVisible (meters);

    const int w = (int) p.uiState.getProperty ("width", 1180), h = (int) p.uiState.getProperty ("height", 720);  // read before resized() stores sizes
    setResizable (true, true);
    setResizeLimits (980, 620, 2200, 1400);
    constructed = true;
    setSize (juce::jlimit (980, 2200, w), juce::jlimit (620, 1400, h));
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

ApexEditor::~ApexEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void ApexEditor::updateTargetPill()
{
    const int t = juce::jlimit (0, numTargets - 1, (int) proc.uiState.getProperty ("target", 3));
    targetPill.setText (t == 0 ? juce::String ("Off") : minus (juce::String (targets[t], 0)) + " LUFS", "Target");
    display.setTarget (targets[t]);
    stats.setTarget (targets[t]);
    display.setShowLoudness ((bool) proc.uiState.getProperty ("loudnessLine", true));
    resized();
}

void ApexEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141416), 0, 0, juce::Colour (0xff09090a), 0, (float) getHeight(), false));
    g.fillAll();

    // logo
    auto logo = juce::Rectangle<float> (16.0f, 16.0f, 26.0f, 26.0f);
    g.setGradientFill (juce::ColourGradient (accentA(), logo.getX(), logo.getY(), accentB(), logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (logo.reduced (0.5f), 7.0f, 1.0f);
    {
        // glyph: a waveform pressed under a ceiling
        juce::Path p;
        p.startNewSubPath (5.5f, 7.5f); p.lineTo (18.5f, 7.5f);
        p.startNewSubPath (5.5f, 15.0f); p.lineTo (7.5f, 10.5f); p.lineTo (9.5f, 17.5f); p.lineTo (12.0f, 10.0f); p.lineTo (14.5f, 17.5f); p.lineTo (16.5f, 10.5f); p.lineTo (18.5f, 15.0f);
        icon::draw (g, p, logo, juce::Colours::white, 1.7f, 24.0f);
    }
    g.setFont (font (15.5f, Weight::semibold));
    g.setColour (col::label);
    g.drawText ("Avel", juce::Rectangle<float> (50.0f, 16.0f, 40.0f, 26.0f), juce::Justification::centredLeft, false);
    const float aw = textWidth (font (15.5f, Weight::semibold), "Avel");
    g.setFont (font (15.5f, Weight::regular));
    g.setColour (col::label2);
    g.drawText ("Apex", juce::Rectangle<float> (50.0f + aw + 5.0f, 16.0f, 60.0f, 26.0f), juce::Justification::centredLeft, false);

    // oversampling caption
    if (showOsCaption)
    {
    g.setFont (captionFont());
    g.setColour (col::label3);
    g.drawText ("OVERSAMPLING", juce::Rectangle<float> ((float) oversampling.getX() - 92.0f, (float) oversampling.getY(), 84.0f, (float) oversampling.getHeight()),
                juce::Justification::centredRight, false);
    }

    drawDisplay (g, displayArea, accentA());

    // right column frame
    auto rc = juce::Rectangle<float> ((float) gain.getX() - 2.0f, displayArea.getY(), (float) gain.getWidth() + 4.0f, displayArea.getHeight());
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    g.fillRoundedRectangle (rc, 14.0f);
    g.setColour (col::stroke);
    g.drawRoundedRectangle (rc.reduced (0.5f), 14.0f, 1.0f);
}

void ApexEditor::resized()
{
    const int W = getWidth(), H = getHeight();
    if (constructed)
    {
        proc.uiState.setProperty ("width", W, nullptr);
        proc.uiState.setProperty ("height", H, nullptr);
    }

    // top bar
    const float aw = textWidth (font (15.5f, Weight::semibold), "Avel") + 5.0f + textWidth (font (15.5f, Weight::regular), "Apex");
    int x = 50 + (int) aw + 20;
    preset.setBounds (x, 14, juce::jlimit (170, 240, preset.getIdealWidth()), 30);
    x = preset.getRight() + 8;
    undoBtn.setBounds (x, 14, 30, 30);
    redoBtn.setBounds (undoBtn.getRight() + 2, 14, 30, 30);

    int rx = W - 16;
    bypassBtn.setBounds (rx - 30, 14, 30, 30); rx = bypassBtn.getX() - 12;
    unity.setBounds (rx - unity.getIdealWidth(), 14, unity.getIdealWidth(), 30); rx = unity.getX() - 18;
    copyBtn.setBounds (rx - 30, 14, 30, 30); rx = copyBtn.getX() - 4;
    ab.setBounds (rx - 64, 16, 64, 26);

    // oversampling, centred with its caption when there is room
    const int leftEdge = redoBtn.getRight() + 16, rightEdge = ab.getX() - 16;
    int osW = juce::jmax (236, oversampling.getIdealWidth());
    showOsCaption = rightEdge - leftEdge >= osW + 92;
    if (! showOsCaption) osW = juce::jmin (osW, rightEdge - leftEdge);
    const int capW = showOsCaption ? 92 : 0;
    const int ox = juce::jlimit (leftEdge + capW, rightEdge - osW, (W - (capW + osW)) / 2 + capW);
    oversampling.setBounds (ox, 16, osW, 26);

    // right column and display
    const int colW = 84;
    auto colArea = juce::Rectangle<int> (W - 12 - colW, 58, colW, H - 58 - 12);
    displayArea = juce::Rectangle<float> (12.0f, 58.0f, (float) (colArea.getX() - 10 - 12), (float) (H - 58 - 12));

    gain.setBounds (colArea.getX() + 2, colArea.getY() + 10, colW - 4, 96);
    output.setBounds (colArea.getX() + 2, colArea.getBottom() - 104, colW - 4, 96);
    meters.setBounds (colArea.getX() + 6, gain.getBottom() + 10, colW - 12, output.getY() - gain.getBottom() - 22);

    display.setBounds (displayArea.toNearestInt());
    const int dw = display.getWidth(), dh = display.getHeight();
    const int strip = 22;
    const int cw = juce::jmin (dw - 24, card.getIdealWidth());
    card.setBounds ((dw - cw) / 2, dh - strip - 8 - Card::idealHeight, cw, Card::idealHeight);
    display.setBottomStrip ((float) strip);
    display.setCardBounds (card.getBounds().toFloat());
    display.setGraphArea ({ 0.0f, 58.0f, (float) dw, (float) card.getY() - 10.0f - 58.0f });

    speed.setBounds (dw - 12 - 132, 12, 132, 28);
    const int tw = targetPill.getIdealWidth();
    targetPill.setBounds (speed.getX() - 8 - tw, 12, tw, 28);
    stats.setBounds (12, 12, juce::jmax (100, targetPill.getX() - 24), 28);
}

void ApexEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastTime) * 0.001);
    lastTime = now;

    display.pull();
    display.tick();
    meters.update (dt);
    if (stats.tick()) stats.repaint();
    stats.update();
    card.refresh();

    undoBtn.setEnabled (proc.undo.canUndo());
    redoBtn.setEnabled (proc.undo.canRedo());
    bypassBtn.setToggled (proc.apvts.getRawParameterValue (ids::bypass)->load() >= 0.5f);
    if (ab.getSelected() != proc.getSlot()) ab.setSelected (proc.getSlot(), false);

    if (proc.presetName != shownPreset)
    {
        shownPreset = proc.presetName;
        preset.setText (shownPreset);
        resized();
    }

    const bool tpOn = proc.apvts.getRawParameterValue (ids::truePeak)->load() >= 0.5f;
    if (tpOn != lastTp) { lastTp = tpOn; output.repaint(); }
    const double latMs = proc.getLatencySamples() * 1000.0 / juce::jmax (1.0, proc.getSampleRate());
    oversampling.setTooltip ("Oversampling: limits at a higher sample rate for fewer aliasing artefacts. Costs CPU. Current latency "
                             + juce::String (latMs, 1) + " ms.");
}

//==============================================================================
juce::File ApexEditor::presetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Avel").getChildFile ("Apex Presets");
}

void ApexEditor::showPresetMenu()
{
    juce::PopupMenu m;
    juce::String category = "-";
    const auto& fp = factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
    {
        if (category != fp[(size_t) i].category)
        {
            category = fp[(size_t) i].category;
            if (category.isNotEmpty()) m.addSectionHeader (category);
        }
        m.addItem (i + 1, fp[(size_t) i].name, true, proc.presetName == fp[(size_t) i].name);
    }
    auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.avelpreset");
    files.sort();
    if (! files.isEmpty())
    {
        m.addSectionHeader ("User");
        for (int i = 0; i < files.size(); ++i)
            m.addItem (1000 + i, files[i].getFileNameWithoutExtension(), true, proc.presetName == files[i].getFileNameWithoutExtension());
    }
    m.addSeparator();
    m.addItem (900, juce::String::fromUTF8 ("Save Preset\xe2\x80\xa6"));
    m.addItem (901, juce::String::fromUTF8 ("Load Preset File\xe2\x80\xa6"));
    m.addItem (902, "Open Preset Folder");

    showMenu (m, preset, [this, files] (int r)
    {
        if (r <= 0) return;
        if (r <= (int) factoryPresets().size()) proc.loadFactoryPreset (r - 1);
        else if (r >= 1000 && r - 1000 < files.size()) loadPresetFile (files[r - 1000]);
        else if (r == 900) savePreset();
        else if (r == 902) { presetFolder().createDirectory(); presetFolder().startAsProcess(); }
        else if (r == 901)
        {
            chooser = std::make_unique<juce::FileChooser> ("Load preset", presetFolder(), "*.avelpreset");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) loadPresetFile (fc.getResult()); });
        }
    }, 220);
}

void ApexEditor::savePreset()
{
    presetFolder().createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save preset", presetFolder().getChildFile (proc.presetName + ".avelpreset"), "*.avelpreset");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("avelpreset");
        auto state = proc.apvts.copyState();
        state.removeProperty ("bypass", nullptr);
        if (auto xml = state.createXml()) xml->writeTo (f);
        proc.presetName = f.getFileNameWithoutExtension();
    });
}

void ApexEditor::loadPresetFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (proc.apvts.state.getType())) return;
    proc.undo.beginNewTransaction();
    for (auto child : tree)
    {
        const auto id = child.getProperty ("id").toString();
        if (id == ids::bypass) continue;
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
    }
    proc.presetName = f.getFileNameWithoutExtension();
}
