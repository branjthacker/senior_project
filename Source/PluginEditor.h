/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#define TEXT_HEIGHT_VALUE_LABELS 14;
#define TEXT_HEIGHT_KNOB_LABELS 16;
#define FUNDAMEMTAL_VOL_COLOR juce::Colour(255, 0, 255)
#define ODD_VOL_COLOR juce::Colour(255, 0, 0)
#define EVEN_VOL_COLOR juce::Colour(0, 0, 255)
#define ODD_SYNTH_COLOR juce::Colour(200, 0, 0)
#define EVEN_SYNTH_COLOR juce::Colour(0, 0, 200)
#define ODD_LP_COLOR juce::Colour(145, 0, 0)
#define EVEN_LP_COLOR juce::Colour(0, 0, 145)
#define KNOB_EDGE_COLOR juce::Colour(255, 255, 255)
#define TEXT_COLOR juce::Colour(255, 255, 255)
#define BACKGROUND_COLOR juce::Colour(30, 30, 30)

struct knobLook : juce::LookAndFeel_V4 {
    knobLook(juce::Colour knobColour) : colour(knobColour) {}

    void drawRotarySlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider&) override;

    private:
        juce::Colour colour;

};

struct paramKnob : juce::Slider {
    //define the knobs to be used on the GUI as rotary knobs with a text entry/ display below them
    paramKnob(juce::Colour knobColour, juce::String lowerBoundLabel, juce::String upperBoundLabel) :
        juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
        juce::Slider::TextEntryBoxPosition::TextBoxBelow),
        look(knobColour),
        lowerBoundLabel(lowerBoundLabel),
        upperBoundLabel(upperBoundLabel)
    {
        setLookAndFeel(&look);
    }

    //destructor
    ~paramKnob() {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getKnobBounds() const;
private:
    //these parameters are all to be passed on creation as they are differnt per knob
    juce::Colour knobColour; //color but british because audio
    juce::String lowerBoundLabel;
    juce::String upperBoundLabel;
    knobLook look;
};

//==============================================================================
/**
*/
class Harmonicator9000AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    Harmonicator9000AudioProcessorEditor (Harmonicator9000AudioProcessor&);
    ~Harmonicator9000AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Harmonicator9000AudioProcessor& audioProcessor;
    void timerCallback() override;
    juce::Label freqLabel;
    paramKnob fundamentalVol;
    paramKnob evenHarmVol;
    paramKnob oddHarmVol;
    paramKnob oddSynthVol;
    paramKnob evenSynthVol;
    paramKnob oddSynthLP;
    paramKnob evenSynthLP;

    //define a way to attatch each knob to the parameter it will be controlling

    using paramStates = juce::AudioProcessorValueTreeState;
    using knobAttatch = paramStates::SliderAttachment;

    knobAttatch fundamentalVolAttatch;
    knobAttatch evenHarmVolAttatch;
    knobAttatch oddHarmVolAttatch;
    knobAttatch evenSynthVolAttatch;
    knobAttatch oddSynthVolAttatch;
    knobAttatch evenSynthLPAttatch;
    knobAttatch oddSynthLPAttatch;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Harmonicator9000AudioProcessorEditor)
};
