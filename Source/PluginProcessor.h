#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "DSP/Limiter.h"
#include "DSP/Dither.h"
#include "DSP/LoudnessMeter.h"
#include "DSP/TruePeak.h"

namespace ids
{
    inline constexpr const char* gain        = "gain";
    inline constexpr const char* ceiling     = "ceiling";
    inline constexpr const char* style       = "style";
    inline constexpr const char* lookahead   = "lookahead";
    inline constexpr const char* attack      = "attack";
    inline constexpr const char* release     = "release";
    inline constexpr const char* linkTrans   = "linkTransients";
    inline constexpr const char* linkRel     = "linkRelease";
    inline constexpr const char* oversample  = "oversampling";
    inline constexpr const char* truePeak    = "truePeak";
    inline constexpr const char* dcFilter    = "dcFilter";
    inline constexpr const char* unity       = "unityGain";
    inline constexpr const char* audition    = "audition";
    inline constexpr const char* dither      = "dither";
    inline constexpr const char* shaping     = "noiseShaping";
    inline constexpr const char* bypass      = "bypass";
}

class ApexProcessor : public juce::AudioProcessor
{
public:
    ApexProcessor();
    ~ApexProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Avel Apex"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    //==============================================================================
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" };

    // Presets
    void loadFactoryPreset (int index);
    juce::String presetName { "Default" };
    bool presetDirty = false;

    // A / B
    void selectSlot (int slot);
    void copyToOtherSlot();
    int getSlot() const { return activeSlot; }

    // Metering (written by the audio thread, read by the UI)
    struct Meters
    {
        std::array<std::atomic<float>, 2> in {}, out {};     // peak since last read (linear)
        std::atomic<float> gr { 0.0f };                      // max gain reduction since last read (dB, positive)
        std::atomic<float> truePeakMax { -100.0f };          // output true peak since reset (dBTP)
        std::atomic<float> grMax { 0.0f };                   // max gain reduction since reset (dB)
        std::atomic<bool> resetRequest { false };
    } meters;

    LoudnessMeter loudness;           // output, before monitoring options
    void resetLoudness() { loudness.resetIntegrated(); meters.resetRequest = true; }

    struct Column { float in, out, gr, shortTerm, momentary; };
    static constexpr int historySize = 8192;
    juce::AbstractFifo historyFifo { historySize };
    std::array<Column, historySize> history {};
    std::atomic<double> columnRate { 200.0 };   // columns per second

    static int osFactor (int index) { return 1 << juce::jlimit (0, 4, index); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateLatency (int osIndex);
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int nc);
    void pushColumn();

    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<float>* pGain; std::atomic<float>* pCeiling; std::atomic<float>* pStyle; std::atomic<float>* pLook;
    std::atomic<float>* pAttack; std::atomic<float>* pRelease; std::atomic<float>* pLinkT; std::atomic<float>* pLinkR;
    std::atomic<float>* pOs; std::atomic<float>* pTp; std::atomic<float>* pDc; std::atomic<float>* pUnity;
    std::atomic<float>* pAudition; std::atomic<float>* pDither; std::atomic<float>* pShaping; std::atomic<float>* pBypass;

    double sr = 48000.0;
    static constexpr int maxChunk = 256;
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 5> oversamplers;
    std::array<avel::dsp::Limiter, 5> stage1;
    avel::dsp::Limiter stage2;
    avel::dsp::Dither ditherer;
    std::array<avel::dsp::TruePeakDetector, 2> outTp;
    std::array<avel::dsp::DelayLine, 2> dryRaw, dryGain;
    juce::AudioBuffer<float> rawCopy, gainCopy;
    std::array<float, 256> silence {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmooth;
    juce::SmoothedValue<float> bypassMix;
    std::array<float, 2> dcX {}, dcY {};
    float dcR = 0.9995f;
    int activeOs = -1, latency = 0;

    // per-column accumulation
    int colLen = 240, colPos = 0;
    float colIn = 0, colOut = 0, colGr = 0;
    std::array<float, 2> blockIn {}, blockOut {};
    float blockGr = 0, tpMaxLin = 0, grMaxDb = 0;

    int currentProgram = 0;
    int activeSlot = 0;
    std::array<juce::ValueTree, 2> slots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApexProcessor)
};
