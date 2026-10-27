// Offline test harness for Avel Apex: runs the processor without a DAW and renders the editor to PNG.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

static constexpr double SR = 48000.0;

static void setParam (ApexProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

// A synthetic "mix": kick, snare, hats, bass and chords, peaking near 0 dBFS.
static juce::AudioBuffer<float> makeMusic (double secs, int seed = 1)
{
    juce::Random rnd (seed);
    const int n = (int) (SR * secs);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    const double beat = 60.0 / 124.0;
    float lpL = 0, lpR = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / SR;
        const double tb = std::fmod (t, beat);
        const double bar = std::fmod (t, beat * 4);
        // kick
        double kick = std::exp (-tb * 18.0) * std::sin (juce::MathConstants<double>::twoPi * (48.0 * tb + 90.0 * (1 - std::exp (-tb * 30.0)) / 30.0));
        // snare on 2 and 4
        const double ts = std::fmod (t + beat, beat * 2);
        double snare = ts < 0.25 ? std::exp (-ts * 22.0) * ((rnd.nextFloat() * 2 - 1) * 0.6 + 0.4 * std::sin (juce::MathConstants<double>::twoPi * 190 * ts)) : 0.0;
        // hats on eighths
        const double th = std::fmod (t, beat * 0.5);
        double hat = std::exp (-th * 70.0) * (rnd.nextFloat() * 2 - 1) * 0.25;
        // bass
        static const double notes[] { 41.2, 41.2, 49.0, 36.7 };
        const double f = notes[(int) (t / (beat * 4)) % 4];
        double bass = 0.32 * std::sin (juce::MathConstants<double>::twoPi * f * t) * (0.6 + 0.4 * std::exp (-tb * 6));
        // chords
        double pad = 0;
        for (double ff : { 220.0, 261.6, 329.6, 392.0 }) pad += 0.045 * std::sin (juce::MathConstants<double>::twoPi * ff * t + ff);
        pad *= 0.7 + 0.3 * std::sin (bar * 3.0);
        lpL = 0.8f * lpL + 0.2f * (float) hat; lpR = 0.8f * lpR + 0.2f * (float) (rnd.nextFloat() * 2 - 1) * 0.0f;
        const double L = 0.85 * kick + 0.5 * snare + (hat - lpL) * 1.2 + bass + pad;
        const double R = 0.85 * kick + 0.45 * snare + hat * 0.9 + bass + pad * 0.9;
        b.setSample (0, i, (float) L * 0.9f);
        b.setSample (1, i, (float) R * 0.9f);
    }
    return b;
}

// Worst case for inter-sample peaks: sines at fs/4 with 45 degree phase plus a near-Nyquist square.
static juce::AudioBuffer<float> makeIsp (double secs)
{
    const int n = (int) (SR * secs);
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / SR;
        const float a = (float) std::sin (juce::MathConstants<double>::twoPi * (SR / 4) * t + juce::MathConstants<double>::pi / 4);
        const float sq = std::sin (juce::MathConstants<double>::twoPi * 11025.0 * t) > 0 ? 0.7f : -0.7f;
        const bool section = (i / 24000) % 2 == 0;
        b.setSample (0, i, section ? 0.9f * a : sq);
        b.setSample (1, i, section ? 0.9f * a : -sq);
    }
    return b;
}

static juce::AudioBuffer<float> render (ApexProcessor& p, const juce::AudioBuffer<float>& in, int block = 512)
{
    juce::AudioBuffer<float> out (in);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    for (int pos = 0; pos < in.getNumSamples(); pos += block)
    {
        const int len = juce::jmin (block, in.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, len);
        p.processBlock (view, midi);
    }
    return out;
}

static float samplePeakDb (const juce::AudioBuffer<float>& b, int skip)
{
    float m = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        m = juce::jmax (m, b.getMagnitude (c, skip, b.getNumSamples() - skip));
    return juce::Decibels::gainToDecibels (m, -200.0f);
}

// Independent true-peak measurement: 16x linear-phase FIR oversampling.
static float truePeakDb (const juce::AudioBuffer<float>& b, int skip)
{
    juce::dsp::Oversampling<float> os (2, 4, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os.initProcessing (1024);
    float m = 0;
    juce::AudioBuffer<float> tmp (2, 1024);
    for (int pos = 0; pos < b.getNumSamples(); pos += 1024)
    {
        const int len = juce::jmin (1024, b.getNumSamples() - pos);
        for (int c = 0; c < 2; ++c) tmp.copyFrom (c, 0, b, c, pos, len);
        juce::dsp::AudioBlock<float> blk (tmp.getArrayOfWritePointers(), 2, (size_t) len);
        auto up = os.processSamplesUp (blk);
        if (pos >= skip)
            for (size_t c = 0; c < 2; ++c)
                for (size_t i = 0; i < up.getNumSamples(); ++i) m = juce::jmax (m, std::abs (up.getSample ((int) c, (int) i)));
    }
    return juce::Decibels::gainToDecibels (m, -200.0f);
}

static std::unique_ptr<ApexProcessor> makeProc()
{
    auto p = std::make_unique<ApexProcessor>();
    p->setPlayConfigDetails (2, 2, SR, 512);
    p->prepareToPlay (SR, 512);
    return p;
}

static void prepareAgain (ApexProcessor& p) { p.prepareToPlay (SR, 512); }

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), SR, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const bool quick = argc > 2 && juce::String (argv[2]) == "--ui-only";


    if (! quick)
    {
        std::cout << "== Latency (impulse, no limiting)" << std::endl;
        for (int os = 0; os < 5; ++os)
        {
            auto p = makeProc();
            setParam (*p, ids::oversample, (float) os);
            setParam (*p, ids::ceiling, 0.0f);
            prepareAgain (*p);
            juce::AudioBuffer<float> in (2, 24000); in.clear();
            in.setSample (0, 4000, 0.5f); in.setSample (1, 4000, 0.5f);
            auto out = render (*p, in);
            int idx = 0; float m = 0;
            for (int i = 0; i < out.getNumSamples(); ++i) if (std::abs (out.getSample (0, i)) > m) { m = std::abs (out.getSample (0, i)); idx = i; }
            const int lat = p->getLatencySamples();
            check (idx - 4000 == lat,
                   "OS " + juce::String (ApexProcessor::osFactor (os)) + "x: reported latency " + juce::String (lat) + " samples ("
                   + juce::String (lat / SR * 1000.0, 2) + " ms), measured " + juce::String (idx - 4000) + ", peak " + juce::String (m, 4));
        }

        std::cout << "== Transparency below the ceiling (-20 dBFS 1 kHz sine, gain 0)" << std::endl;
        for (int os : { 0, 2 })
        {
            auto p = makeProc();
            setParam (*p, ids::oversample, (float) os);
            prepareAgain (*p);
            juce::AudioBuffer<float> in (2, 48000);
            for (int i = 0; i < in.getNumSamples(); ++i)
                for (int c = 0; c < 2; ++c) in.setSample (c, i, 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / SR));
            auto out = render (*p, in);
            const int lat = p->getLatencySamples();
            double err = 0, ref = 0;
            for (int i = 8000; i < 40000; ++i) { const double d = out.getSample (0, i) - in.getSample (0, i - lat); err += d * d; ref += (double) in.getSample (0, i - lat) * in.getSample (0, i - lat); }
            const double nullDb = 10.0 * std::log10 ((err + 1e-30) / ref);
            check (nullDb < (os == 0 ? -120.0 : -60.0), "OS " + juce::String (ApexProcessor::osFactor (os)) + "x null vs dry: " + juce::String (nullDb, 1) + " dB");
        }

        std::cout << "== Ceiling guarantee (gain +12 dB, ceiling -1 dB)" << std::endl;
        const auto music = makeMusic (6.0);
        const auto isp = makeIsp (2.0);
        float worstTp[2][5] {};
        for (int os = 0; os < 5; ++os)
            for (int tp = 0; tp < 2; ++tp)
            {
                float worstSp = -200, worstT = -200;
                for (int st = 0; st < 8; ++st)
                    for (int sig = 0; sig < 2; ++sig)
                    {
                        auto p = makeProc();
                        setParam (*p, ids::oversample, (float) os);
                        setParam (*p, ids::truePeak, (float) tp);
                        setParam (*p, ids::style, (float) st);
                        setParam (*p, ids::gain, 12.0f);
                        setParam (*p, ids::ceiling, -1.0f);
                        prepareAgain (*p);
                        auto out = render (*p, sig == 0 ? music : isp);
                        worstSp = juce::jmax (worstSp, samplePeakDb (out, 4800));
                        worstT = juce::jmax (worstT, truePeakDb (out, 4800));
                    }
                worstTp[tp][os] = worstT;
                check (worstSp <= -1.0f + 0.0005f, "OS " + juce::String (ApexProcessor::osFactor (os)) + "x TP " + (tp ? "on " : "off") +
                       ": max sample peak " + juce::String (worstSp, 3) + " dB, max true peak " + juce::String (worstT, 3) + " dBTP");
                if (tp) check (worstT <= -1.0f + 0.1f, "      true peak within 0.1 dB of the ceiling");
            }

        std::cout << "== Loudness and gain reduction on the synthetic mix (style, OS 4x, TP on, +8 dB)" << std::endl;
        for (int st = 0; st < 8; ++st)
        {
            auto p = makeProc();
            setParam (*p, ids::oversample, 2);
            setParam (*p, ids::style, (float) st);
            setParam (*p, ids::gain, 8.0f);
            prepareAgain (*p);
            auto out = render (*p, music);
            std::cout << "  " << juce::String (avel::dsp::styles()[(size_t) st].name).paddedRight (' ', 12)
                      << " integrated " << juce::String (p->loudness.getIntegrated(), 1) << " LUFS, max GR "
                      << juce::String (p->meters.grMax.load(), 1) << " dB, TP " << juce::String (truePeakDb (out, 4800), 2) << " dBTP (own meter "
                      << juce::String (p->meters.truePeakMax.load(), 2) << ")" << std::endl;
            if (st == 7) writeWav (outDir.getChildFile ("music_modern_+8dB.wav"), out);
        }
        writeWav (outDir.getChildFile ("music_input.wav"), music);

        std::cout << "== CPU (6 s stereo, 512-sample blocks, Allround style)" << std::endl;
        for (int os = 0; os < 5; ++os)
        {
            auto p = makeProc();
            setParam (*p, ids::oversample, (float) os);
            setParam (*p, ids::gain, 10.0f);
            prepareAgain (*p);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            render (*p, music);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            std::cout << "  OS " << ApexProcessor::osFactor (os) << "x: " << juce::String (ms / 60.0, 2) << " % of one core" << std::endl;
        }

        std::cout << "== Dither (16 bit, shaped): output stays on the 16-bit grid" << std::endl;
        {
            auto p = makeProc();
            setParam (*p, ids::dither, 4);
            setParam (*p, ids::gain, 6.0f);
            prepareAgain (*p);
            auto out = render (*p, music);
            double maxErr = 0;
            for (int i = 4800; i < out.getNumSamples(); ++i) { const double v = out.getSample (0, i) * 32768.0; maxErr = juce::jmax (maxErr, std::abs (v - std::round (v))); }
            check (maxErr < 1.0e-3, "max distance from the 16-bit grid " + juce::String (maxErr, 6) + " LSB");
        }

        std::cout << "== Audition (delta) and bypass" << std::endl;
        {
            auto p = makeProc();
            setParam (*p, ids::bypass, 1);
            prepareAgain (*p);
            auto out = render (*p, music);
            const int lat = p->getLatencySamples();
            double err = 0;
            for (int i = 8000; i < 40000; ++i) err = juce::jmax (err, (double) std::abs (out.getSample (0, i) - music.getSample (0, i - lat)));
            check (err < 1.0e-6, "bypass = delayed dry (max error " + juce::String (err, 8) + ")");

            auto q = makeProc();
            setParam (*q, ids::gain, 0.0f);
            setParam (*q, ids::ceiling, 0.0f);
            setParam (*q, ids::audition, 1);
            prepareAgain (*q);
            juce::AudioBuffer<float> quiet (music); quiet.applyGain (0.25f);
            auto d = render (*q, quiet);
            check (samplePeakDb (d, 4800) < -100.0f, "audition is silent when nothing is limited (" + juce::String (samplePeakDb (d, 4800), 1) + " dB)");
        }

        std::cout << "== State, presets and A/B" << std::endl;
        {
            auto p = makeProc();
            p->loadFactoryPreset (3);
            juce::MemoryBlock mb; p->getStateInformation (mb);
            auto q = makeProc();
            q->setStateInformation (mb.getData(), (int) mb.getSize());
            bool same = true;
            for (int i = 0; i < p->getParameters().size(); ++i) same &= std::abs (p->getParameters()[i]->getValue() - q->getParameters()[i]->getValue()) < 1e-6f;
            check (same && q->presetName == p->presetName, "state round trip (" + juce::String ((int) mb.getSize()) + " bytes), preset '" + q->presetName + "'");

            setParam (*p, ids::gain, 3.0f);
            p->selectSlot (1);
            setParam (*p, ids::gain, 9.0f);
            p->selectSlot (0);
            const float a = p->apvts.getRawParameterValue (ids::gain)->load();
            p->selectSlot (1);
            const float b = p->apvts.getRawParameterValue (ids::gain)->load();
            check (std::abs (a - 3.0f) < 0.01f && std::abs (b - 9.0f) < 0.01f, "A/B slots keep their own settings (A " + juce::String (a) + ", B " + juce::String (b) + ")");
        }
    }

    std::cout << "== Editor snapshots" << std::endl;
    {
        auto p = makeProc();
        p->loadFactoryPreset (2);
        prepareAgain (*p);
        const auto music = makeMusic (20.0, 3);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        ed->setVisible (true);
        juce::MidiBuffer midi;
        int pos = 0;
        auto feed = [&] (int frames)
        {
            for (int f = 0; f < frames; ++f)
            {
                for (int k = 0; k < 2; ++k)
                {
                    juce::AudioBuffer<float> blk (2, 408);
                    for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, music, c, pos % (music.getNumSamples() - 408), 408);
                    pos += 408;
                    p->processBlock (blk, midi);
                }
                juce::MessageManager::getInstance()->runDispatchLoopUntil (17);
            }
        };
        auto snap = [&] (const juce::String& name)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile (name);
            f.deleteFile();
            juce::FileOutputStream fos (f);
            juce::PNGImageFormat().writeImageToStream (img, fos);
            std::cout << "  wrote " << f.getFullPathName() << " (" << img.getWidth() << "x" << img.getHeight() << ")" << std::endl;
        };
        ed->setSize (1180, 720);
        feed (480);   // ~8 s
        snap ("apex_1180x720.png");
        ed->setSize (980, 620);
        feed (60);
        snap ("apex_980x620.png");
        ed->setSize (1600, 960);
        feed (60);
        snap ("apex_1600x960.png");
        // a few other states
        ed->setSize (1180, 720);
        setParam (*p, ids::truePeak, 0);
        setParam (*p, ids::audition, 1);
        setParam (*p, ids::dither, 4);
        setParam (*p, ids::style, 3);
        p->uiState.setProperty ("speed", 0, nullptr);
        feed (120);
        snap ("apex_states.png");
    }

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURE(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
