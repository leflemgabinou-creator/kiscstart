#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class KickProcessor : public juce::AudioProcessor
{
public:
    KickProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kickstart"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // données pour l'affichage (écrites par le thread audio, lues par l'interface)
    static constexpr int kCols = 400;
    std::atomic<float> inMin[kCols], inMax[kCols], outMin[kCols], outMax[kCols];
    std::atomic<float> uiPhase { 1.f };

private:
    double sr = 44100.0;

    juce::dsp::LinkwitzRileyFilter<float> xover;
    std::array<float, 1025> table {};
    int tableShape = -1;

    float phase = 1.f;          // 0..1 dans le cycle (1 = fin, pas de coupure)
    bool running = false;
    float gainSmooth = 1.f;
    float detLevel = 0.f;
    bool armed = true;
    int refractory = 0;

    int lastCol = 0;
    float curInMin = 0, curInMax = 0, curOutMin = 0, curOutMax = 0;

    int lastMode = -1;
};
