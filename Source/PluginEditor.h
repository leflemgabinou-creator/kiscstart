#pragma once
#include "PluginProcessor.h"

// Interface entièrement dessinée en code, coordonnées de référence 2416 x 1232
class KickEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KickEditor (KickProcessor&);
    ~KickEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override { repaint(); }
    float refScale() const { return (float) getWidth() / 2416.f; }
    juce::Point<float> toRef (const juce::MouseEvent& e) const { return e.position / refScale(); }

    void drawPill (juce::Graphics&, juce::Rectangle<float>, const juce::String&, bool on, float fontSize = 44.f);
    void drawShapeIcon (juce::Graphics&, juce::Rectangle<float>, int shapeIdx, juce::Colour c, float width);
    void drawKnob (juce::Graphics&);
    void drawDisplay (juce::Graphics&);
    void drawBottomLeft (juce::Graphics&);
    void drawNavAndShapes (juce::Graphics&);
    void drawTopBar (juce::Graphics&);

    juce::Rectangle<float> divRect (int i) const;
    juce::Rectangle<float> modeRect (int i) const;
    float sliderNorm() const;

    KickProcessor& proc;

    float mix = 0.8f, len = 0.83f, freq = 150.f;
    int div = 1, mode = 0, shape = 2;
    bool loop = true, band = false, listen = false;

    juce::ParameterAttachment mixA, divA, modeA, loopA, shapeA, lenA, bandA, listenA, freqA;

    enum class Drag { none, knob, length, freqSlider } drag = Drag::none;
    float dragStartValue = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickEditor)
};
