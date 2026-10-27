#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    juce::String dbText (float v, int decimals = 1)
    {
        if (std::abs (v) < 0.05f) v = 0.0f;
        return (v > 0.0f ? "+" : "") + juce::String (v, decimals);
    }

    juce::String msText (float v)
    {
        if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " s";
        if (v >= 100.0f)  return juce::String (juce::roundToInt (v)) + " ms";
        if (v >= 10.0f)   return juce::String (v, 1) + " ms";
        return juce::String (v, 2) + " ms";
    }

    float parseNumber (const juce::String& t) { return t.retainCharacters ("0123456789.,-+").replaceCharacter (',', '.').getFloatValue(); }
}

ApexProcessor::ApexProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "AvelApex", createLayout())
{
    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pGain = p (ids::gain); pCeiling = p (ids::ceiling); pStyle = p (ids::style); pLook = p (ids::lookahead);
    pAttack = p (ids::attack); pRelease = p (ids::release); pLinkT = p (ids::linkTrans); pLinkR = p (ids::linkRel);
    pOs = p (ids::oversample); pTp = p (ids::truePeak); pDc = p (ids::dcFilter); pUnity = p (ids::unity);
    pAudition = p (ids::audition); pDither = p (ids::dither); pShaping = p (ids::shaping); pBypass = p (ids::bypass);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ids::bypass));

    uiState.setProperty ("width", 1180, nullptr);
    uiState.setProperty ("height", 720, nullptr);
    uiState.setProperty ("speed", 1, nullptr);
    uiState.setProperty ("target", 3, nullptr);
    uiState.setProperty ("loudnessLine", true, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout ApexProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB")
                     .withStringFromValueFunction ([] (float v, int) { return dbText (v); })
                     .withValueFromStringFunction ([] (const String& t) { return parseNumber (t); });
    auto msAttr = AudioParameterFloatAttributes().withLabel ("")
                     .withStringFromValueFunction ([] (float v, int) { return msText (v); })
                     .withValueFromStringFunction ([] (const String& t) { const float v = parseNumber (t); return t.containsIgnoreCase ("s") && ! t.containsIgnoreCase ("ms") ? v * 1000.0f : v; });
    auto pctAttr = AudioParameterFloatAttributes().withLabel ("%")
                     .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); })
                     .withValueFromStringFunction ([] (const String& t) { return parseNumber (t); });

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::gain, 1 }, "Gain", NormalisableRange<float> (0.0f, 30.0f, 0.01f, 0.6f), 0.0f, dbAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::ceiling, 1 }, "Output Level", NormalisableRange<float> (-30.0f, 0.0f, 0.01f, 2.2f), -1.0f, dbAttr));

    StringArray styleNames;
    for (auto& s : avel::dsp::styles()) styleNames.add (s.name);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::style, 1 }, "Style", styleNames, 4));

    NormalisableRange<float> look (0.0f, 5.0f, 0.01f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::lookahead, 1 }, "Lookahead", look, 2.0f, msAttr));
    NormalisableRange<float> atk (0.1f, 300.0f, 0.01f); atk.setSkewForCentre (20.0f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::attack, 1 }, "Attack", atk, 15.0f, msAttr));
    NormalisableRange<float> rel (1.0f, 3000.0f, 0.01f); rel.setSkewForCentre (120.0f);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::release, 1 }, "Release", rel, 100.0f, msAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::linkTrans, 1 }, "Link Transients", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 75.0f, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::linkRel, 1 }, "Link Release", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, pctAttr));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::oversample, 1 }, "Oversampling", StringArray { "Off", "2x", "4x", "8x", "16x" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::truePeak, 1 }, "True Peak", StringArray { "Off", "On" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::dcFilter, 1 }, "DC Filter", StringArray { "Off", "On" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::unity, 1 }, "Unity Gain", StringArray { "Off", "On" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::audition, 1 }, "Audition Limiting", StringArray { "Off", "On" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::dither, 1 }, "Dither", StringArray { "Off", "24 bit", "20 bit", "18 bit", "16 bit" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::shaping, 1 }, "Noise Shaping", StringArray { "Flat", "Shaped" }, 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));
    return l;
}

bool ApexProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void ApexProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 48000.0;
    const int nc = juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels()));

    for (int i = 0; i < 5; ++i)
    {
        const int f = osFactor (i);
        if (i > 0)
        {
            oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) nc, (size_t) i,
                                            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            oversamplers[(size_t) i]->initProcessing ((size_t) maxChunk);
        }
        stage1[(size_t) i].prepare (sr * f, 5.0, f);
    }
    stage2.prepare (sr, 1.0, 1);

    const int maxLatency = 16384;
    for (auto& d : dryRaw) d.prepare (maxLatency);
    for (auto& d : dryGain) d.prepare (maxLatency);
    rawCopy.setSize (2, maxChunk);
    gainCopy.setSize (2, maxChunk);
    for (auto& t : outTp) t.reset();
    ditherer.reset();
    dcX = dcY = {};
    dcR = (float) (1.0 - juce::MathConstants<double>::twoPi * 4.0 / sr);

    gainSmooth.reset (sr, 0.05);
    gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load()));
    bypassMix.reset (sr, 0.02);
    bypassMix.setCurrentAndTargetValue (pBypass->load() >= 0.5f ? 1.0f : 0.0f);

    loudness.prepare (sr);
    colLen = juce::jmax (16, (int) std::round (sr / 200.0));
    columnRate = sr / colLen;
    colPos = 0; colIn = colOut = colGr = 0;
    tpMaxLin = 0; grMaxDb = 0;

    activeOs = -1;
    updateLatency ((int) pOs->load());
}

void ApexProcessor::updateLatency (int osIndex)
{
    osIndex = juce::jlimit (0, 4, osIndex);
    if (osIndex == activeOs) return;
    activeOs = osIndex;
    const int f = osFactor (osIndex);
    int lat = stage1[(size_t) osIndex].getLatency() / f + stage2.getLatency();
    if (osIndex > 0)
    {
        oversamplers[(size_t) osIndex]->reset();
        lat += (int) std::round (oversamplers[(size_t) osIndex]->getLatencyInSamples());
    }
    stage1[(size_t) osIndex].reset();
    stage2.reset();
    latency = lat;
    setLatencySamples (lat);
}

void ApexProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nc = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    for (int c = nc; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (nc == 0 || n == 0) return;

    if (meters.resetRequest.exchange (false)) { tpMaxLin = 0; grMaxDb = 0; meters.truePeakMax = -100.0f; meters.grMax = 0.0f; }

    updateLatency ((int) pOs->load());

    // Settings for this block
    avel::dsp::Limiter::Settings s;
    s.ceilingDb = pCeiling->load();
    s.lookaheadMs = pLook->load();
    s.attackMs = pAttack->load();
    s.releaseMs = pRelease->load();
    s.linkTransients = pLinkT->load() * 0.01f;
    s.linkRelease = pLinkR->load() * 0.01f;
    s.style = (int) pStyle->load();
    const bool tp = pTp->load() >= 0.5f;
    s.truePeakDetect = tp && osFactor (activeOs) < 4;
    stage1[(size_t) activeOs].setSettings (s);

    // Stage 2 only catches what is left (downsampling ripple, inter-sample peaks): short, clean, fully linked.
    avel::dsp::Limiter::Settings s2;
    s2.ceilingDb = s.ceilingDb - (tp ? 0.05f : 0.0f);   // small safety margin for reconstruction filters
    s2.lookaheadMs = 1.0f;
    s2.attackMs = 5.0f;
    s2.releaseMs = 40.0f;
    s2.linkTransients = 1.0f;
    s2.linkRelease = 1.0f;
    s2.style = 6;
    s2.truePeakDetect = tp;
    stage2.setSettings (s2);

    gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (pGain->load()));
    bypassMix.setTargetValue (pBypass->load() >= 0.5f ? 1.0f : 0.0f);

    blockIn = blockOut = {};
    blockGr = 0;

    for (int start = 0; start < n;)
    {
        const int len = juce::jmin (maxChunk, n - start, colLen - colPos);
        processChunk (buffer, start, len, nc);
        start += len;
    }

    for (int c = 0; c < 2; ++c)
    {
        const float v = c < nc ? blockIn[(size_t) c] : blockIn[0];
        const float o = c < nc ? blockOut[(size_t) c] : blockOut[0];
        auto prev = meters.in[(size_t) c].load();
        while (v > prev && ! meters.in[(size_t) c].compare_exchange_weak (prev, v)) {}
        prev = meters.out[(size_t) c].load();
        while (o > prev && ! meters.out[(size_t) c].compare_exchange_weak (prev, o)) {}
    }
    auto prev = meters.gr.load();
    while (blockGr > prev && ! meters.gr.compare_exchange_weak (prev, blockGr)) {}
    meters.truePeakMax = tpMaxLin > 1.0e-5f ? 20.0f * std::log10 (tpMaxLin) : -100.0f;
    meters.grMax = grMaxDb;
}

void ApexProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int nc)
{
    float* ptr[2] = { buffer.getWritePointer (0, start), nc > 1 ? buffer.getWritePointer (1, start) : nullptr };

    // keep the raw input for bypass
    for (int c = 0; c < nc; ++c) rawCopy.copyFrom (c, 0, ptr[c], len);

    // DC filter + input gain
    const bool dc = pDc->load() >= 0.5f;
    for (int i = 0; i < len; ++i)
    {
        const float g = gainSmooth.getNextValue();
        for (int c = 0; c < nc; ++c)
        {
            float x = ptr[c][i];
            if (dc)
            {
                const float y = x - dcX[(size_t) c] + dcR * dcY[(size_t) c];
                dcX[(size_t) c] = x; dcY[(size_t) c] = y; x = y;
            }
            ptr[c][i] = x * g;
        }
    }
    for (int c = 0; c < nc; ++c)
    {
        gainCopy.copyFrom (c, 0, ptr[c], len);
        const float pk = juce::FloatVectorOperations::findMaximum (ptr[c], len);
        const float mn = juce::FloatVectorOperations::findMinimum (ptr[c], len);
        blockIn[(size_t) c] = juce::jmax (blockIn[(size_t) c], pk, -mn);
        colIn = juce::jmax (colIn, pk, -mn);
    }

    // Stage 1: main limiter, oversampled
    float gr1;
    if (activeOs == 0)
        gr1 = stage1[0].process (ptr, nc, len);
    else
    {
        auto& os = *oversamplers[(size_t) activeOs];
        juce::dsp::AudioBlock<float> block (ptr, (size_t) nc, (size_t) len);
        auto up = os.processSamplesUp (block);
        float* upPtr[2] = { up.getChannelPointer (0), nc > 1 ? up.getChannelPointer (1) : nullptr };
        gr1 = stage1[(size_t) activeOs].process (upPtr, nc, (int) up.getNumSamples());
        os.processSamplesDown (block);
    }
    // Stage 2: residual / true-peak safety at the base rate
    const float gr2 = stage2.process (ptr, nc, len);
    const float gr = gr1 + gr2;
    blockGr = juce::jmax (blockGr, gr);
    colGr = juce::jmax (colGr, gr);
    grMaxDb = juce::jmax (grMaxDb, gr);

    // Output metering (before monitoring options)
    for (int c = 0; c < nc; ++c)
    {
        const float pk = juce::FloatVectorOperations::findMaximum (ptr[c], len);
        const float mn = juce::FloatVectorOperations::findMinimum (ptr[c], len);
        blockOut[(size_t) c] = juce::jmax (blockOut[(size_t) c], pk, -mn);
        colOut = juce::jmax (colOut, pk, -mn);
        auto& t = outTp[(size_t) c];
        for (int i = 0; i < len; ++i) tpMaxLin = juce::jmax (tpMaxLin, t.process (ptr[c][i], tpMaxLin));
    }
    loudness.process (ptr[0], nc > 1 ? ptr[1] : silence.data(), len);   // mono counts once (BS.1770)

    // Monitoring: audition (what the limiter removes) and unity gain
    const bool audition = pAudition->load() >= 0.5f;
    const bool unity = pUnity->load() >= 0.5f;
    const float invGain = 1.0f / juce::jmax (1.0e-4f, gainSmooth.getCurrentValue());
    for (int c = 0; c < nc; ++c)
    {
        auto& dg = dryGain[(size_t) c];
        const float* src = gainCopy.getReadPointer (c);
        for (int i = 0; i < len; ++i)
        {
            const float dry = dg.push (src[i], latency);
            if (audition) ptr[c][i] = dry - ptr[c][i];
        }
        if (unity) juce::FloatVectorOperations::multiply (ptr[c], invGain, len);
    }

    // Dither (not while auditioning the difference)
    const int ditherIndex = (int) pDither->load();
    static constexpr int bits[] { 0, 24, 20, 18, 16 };
    if (ditherIndex > 0 && ! audition)
        ditherer.process (ptr, nc, len, bits[juce::jlimit (0, 4, ditherIndex)], pShaping->load() >= 0.5f);

    // Bypass with latency compensation and a short crossfade
    for (int c = 0; c < nc; ++c)
    {
        auto& dr = dryRaw[(size_t) c];
        const float* src = rawCopy.getReadPointer (c);
        auto mix = bypassMix;
        for (int i = 0; i < len; ++i)
        {
            const float dry = dr.push (src[i], latency);
            const float m = mix.getNextValue();
            if (m > 0.0f) ptr[c][i] = ptr[c][i] + m * (dry - ptr[c][i]);
        }
        if (c == nc - 1) bypassMix = mix;
    }

    colPos += len;
    if (colPos >= colLen) pushColumn();
}

void ApexProcessor::pushColumn()
{
    Column col { colIn, colOut, colGr, loudness.getShortTerm(), loudness.getMomentary() };
    if (historyFifo.getFreeSpace() < 1) { colPos = 0; colIn = colOut = colGr = 0; return; } // UI not reading: drop
    const auto scope = historyFifo.write (1);
    if (scope.blockSize1 > 0) history[(size_t) scope.startIndex1] = col;
    else if (scope.blockSize2 > 0) history[(size_t) scope.startIndex2] = col;
    colPos = 0; colIn = colOut = colGr = 0;
}

//==============================================================================
int ApexProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String ApexProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, (int) factoryPresets().size()) ? factoryPresets()[(size_t) i].name : ""; }
void ApexProcessor::setCurrentProgram (int i) { if (juce::isPositiveAndBelow (i, (int) factoryPresets().size())) loadFactoryPreset (i); }

void ApexProcessor::loadFactoryPreset (int index)
{
    const auto& fp = factoryPresets()[(size_t) juce::jlimit (0, (int) factoryPresets().size() - 1, index)];
    undo.beginNewTransaction();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp == bypassParam) continue;
            float v = rp->convertFrom0to1 (rp->getDefaultValue());
            for (auto& kv : fp.values)
                if (rp->getParameterID() == kv.first) v = kv.second;
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        }
    currentProgram = index;
    presetName = fp.name;
    presetDirty = false;
}

void ApexProcessor::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot) return;
    slots[(size_t) activeSlot] = apvts.copyState();
    activeSlot = slot;
    if (slots[(size_t) slot].isValid())
        apvts.replaceState (slots[(size_t) slot].createCopy());
}

void ApexProcessor::copyToOtherSlot()
{
    slots[(size_t) (1 - activeSlot)] = apvts.copyState();
}

void ApexProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree root ("AvelApexState");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("preset", presetName, nullptr);
    root.setProperty ("slot", activeSlot, nullptr);
    root.appendChild (state, nullptr);
    root.appendChild (uiState.createCopy(), nullptr);
    juce::ValueTree ab ("AB");
    for (int i = 0; i < 2; ++i)
    {
        juce::ValueTree slot ("Slot");
        slot.setProperty ("index", i, nullptr);
        if (i == activeSlot) slot.appendChild (state.createCopy(), nullptr);
        else if (slots[(size_t) i].isValid()) slot.appendChild (slots[(size_t) i].createCopy(), nullptr);
        ab.appendChild (slot, nullptr);
    }
    root.appendChild (ab, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, destData);
}

void ApexProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("AvelApexState")) return;

    auto state = root.getChildWithName (apvts.state.getType());
    if (state.isValid())
    {
        apvts.replaceState (state);
        // push every value explicitly: APVTS skips parameters whose snapped value looks unchanged
        for (auto child : state)
            if (auto* prm = apvts.getParameter (child.getProperty ("id").toString()))
            {
                const float v = prm->convertTo0to1 ((float) child.getProperty ("value"));
                if (std::abs (prm->getValue() - v) > 1.0e-6f) prm->setValueNotifyingHost (v);
            }
    }
    presetName = root.getProperty ("preset", "Default").toString();
    activeSlot = juce::jlimit (0, 1, (int) root.getProperty ("slot", 0));

    auto ui = root.getChildWithName ("UI");
    if (ui.isValid())
        for (int i = 0; i < ui.getNumProperties(); ++i)
            uiState.setProperty (ui.getPropertyName (i), ui.getProperty (ui.getPropertyName (i)), nullptr);

    auto ab = root.getChildWithName ("AB");
    for (int i = 0; i < 2; ++i) slots[(size_t) i] = {};
    if (ab.isValid())
        for (auto slot : ab)
        {
            const int i = juce::jlimit (0, 1, (int) slot.getProperty ("index", 0));
            if (slot.getNumChildren() > 0) slots[(size_t) i] = slot.getChild (0).createCopy();
        }
}

juce::AudioProcessorEditor* ApexProcessor::createEditor() { return new ApexEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ApexProcessor(); }
