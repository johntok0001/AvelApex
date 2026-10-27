#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/HistoryDisplay.h"

namespace avel::ui
{
    // Loudness / peak readouts in the top-left of the display.
    class StatsBar : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit StatsBar (ApexProcessor& p) : proc (p) { setTooltip ("Loudness of the output (EBU R128). Click to reset Integrated, Range, True Peak and Max GR."); }

        void setTarget (float t) { target = t; }
        void update();
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked()) { proc.resetLoudness(); flash = 1.0f; } }
        bool tick() { return approach (flash, 0.0f, 0.12f); }

    private:
        ApexProcessor& proc;
        float shortTerm = -100, integrated = -100, range = 0, truePeak = -100, grMax = 0, target = -14.0f, flash = 0.0f;
    };

    // Right column: gain-reduction and output level bars with peak hold.
    class MeterColumn : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit MeterColumn (ApexProcessor& p) : proc (p) { setTooltip ("GR: gain reduction. L / R: output level. Click a number to reset its peak hold."); }
        void update (float dt);
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked()) { holdGr = 0; holdOut = -100; } }

    private:
        float toY (float db, juce::Rectangle<float> r) const { return r.getY() + juce::jlimit (0.0f, 1.0f, -db / range) * r.getHeight(); }
        ApexProcessor& proc;
        static constexpr float range = 36.0f;
        float gr = 0, out[2] { -100, -100 }, holdGr = 0, holdOut = -100, peakOut[2] { -100, -100 }, peakAge[2] {};
    };

    // Glass editor card with style, envelope, link and option controls.
    class Card : public juce::Component
    {
    public:
        explicit Card (ApexProcessor& p);
        void paint (juce::Graphics&) override;
        void resized() override;
        int getIdealWidth() const;
        static constexpr int idealHeight = 142;
        void refresh();

    private:
        void showStyleMenu();
        void showDitherMenu();

        ApexProcessor& proc;
        PillButton style, dither;
        Dial lookahead { "Lookahead" }, attack { "Attack" }, release { "Release" }, linkT { "Transients" }, linkR { "Release" };
        Switch truePeak { "True Peak" }, dc { "DC Filter" }, audition { "Audition" };
        juce::Rectangle<float> sections[4];
        int lastStyle = -1, lastDither = -1, lastShaping = -1;
    };
}

class ApexEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ApexEditor (ApexProcessor&);
    ~ApexEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::Colour accentA() { return juce::Colour (0xffff453a); }
    static juce::Colour accentB() { return juce::Colour (0xffff2d78); }

private:
    void timerCallback() override;
    void showPresetMenu();
    void savePreset();
    void loadPresetFile (const juce::File&);
    static juce::File presetFolder();
    void updateTargetPill();

    ApexProcessor& proc;
    avel::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    avel::ui::PillButton preset;
    avel::ui::IconButton undoBtn { avel::ui::icon::undo(), "Undo" }, redoBtn { avel::ui::icon::redo(), "Redo" };
    avel::ui::Segmented oversampling;
    avel::ui::Segmented ab { { "A", "B" } };
    avel::ui::IconButton copyBtn { avel::ui::icon::copy(), "Copy the current settings to the other slot" };
    avel::ui::Switch unity { "Unity Gain" };
    avel::ui::IconButton bypassBtn { avel::ui::icon::power(), "Bypass (latency compensated)" };

    avel::ui::HistoryDisplay display;
    avel::ui::StatsBar stats;
    avel::ui::PillButton targetPill;
    avel::ui::Segmented speed { { "4 s", "8 s", "16 s" } };
    avel::ui::Card card;

    avel::ui::Dial gain { "Gain" }, output { "Output" };
    avel::ui::MeterColumn meters;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<float> displayArea;
    double lastTime = 0;
    juce::String shownPreset;
    bool lastTp = true, showOsCaption = true, constructed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexEditor)
};
