/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


void knobLook::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider&) {

    //get bounds for each knob
    auto bounds = juce::Rectangle<float>(x, y, width, height);

    //color the knob based on what color was passed to it
    g.setColour(this->colour);
    g.fillEllipse(bounds);

    //color the knob edge
    g.setColour(KNOB_EDGE_COLOR);
    g.drawEllipse(bounds, 1.0);

    //draw a line on the knob that points to where it is in the rotation
    auto center = bounds.getCentre();
    juce::Path dialPath;
    juce::Rectangle<float> dialPointer;
    dialPointer.setLeft(center.getX() - 2);
    dialPointer.setRight(center.getX() + 2);
    dialPointer.setTop(bounds.getY());
    dialPointer.setBottom(center.getY());
    dialPath.addRectangle(dialPointer);

    jassert(rotaryStartAngle < rotaryEndAngle); //make sure the start is before the end, else bad

    auto knobAngRad = juce::jmap<float>(sliderPosProportional, 0.0, 1.0, rotaryStartAngle, rotaryEndAngle);

    //apply transform to rotate the dial pointer based on the angle
    dialPath.applyTransform(juce::AffineTransform().rotated(knobAngRad, center.getX(), center.getY()));
    g.fillPath(dialPath);
}
//==============================================================================

void paramKnob::paint(juce::Graphics& g) {

    //define the start and end angles of the rotary (0 degrees defined as 12 noon)
    auto startAng = juce::degreesToRadians(180.0 + 45.0);
    auto endAng = juce::degreesToRadians(180.0 - 45.0) + juce::MathConstants<float>::twoPi;

    auto range = getRange();

    auto knobBounds = getKnobBounds();
    
    getLookAndFeel().drawRotarySlider(g,
        knobBounds.getX(),
        knobBounds.getY(),
        knobBounds.getWidth(),
        knobBounds.getHeight(),
        juce::jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
        startAng, endAng, *this);
}

juce::Rectangle<int> paramKnob::getKnobBounds() const {
    
    auto bounds = getLocalBounds();
    //return a square so that the knobs are drawn as circles
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    size -= 1.5 * TEXT_HEIGHT_VALUE_LABELS;
    juce::Rectangle<int> finalBounds;
    finalBounds.setSize(size, size);
    finalBounds.setCentre(bounds.getCentreX(), bounds.getCentreY() - 10);
    //set it near the middle of the frame so text can be displayed above and below
    return finalBounds;


}

//==============================================================================
Harmonicator9000AudioProcessorEditor::Harmonicator9000AudioProcessorEditor (Harmonicator9000AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    //initialize all the knobs with their paint and labels
    fundamentalVol(FUNDAMEMTAL_VOL_COLOR, "-15dB", "+15dB"), 
    oddHarmVol(ODD_VOL_COLOR, "-15dB", "+15dB"),
    evenHarmVol(EVEN_VOL_COLOR, "-15dB", "+15dB"),
    oddSynthVol(ODD_SYNTH_COLOR, "-100dB", "0dB"),
    evenSynthVol(EVEN_SYNTH_COLOR, "-100dB", "0dB"),
    oddSynthLP(ODD_LP_COLOR, "100Hz", "20kHz"),
    evenSynthLP(EVEN_LP_COLOR, "100Hz", "20kHz"),

    //attatch all of the knobs to the parameters in the audio engine that they control
    fundamentalVolAttatch(audioProcessor.apvts, "fundamental", fundamentalVol),
    evenHarmVolAttatch(audioProcessor.apvts, "evenHarmonics", evenHarmVol),
    oddHarmVolAttatch(audioProcessor.apvts, "oddHarmonics", oddHarmVol),
    evenSynthVolAttatch(audioProcessor.apvts, "evenSynth", evenSynthVol),
    oddSynthVolAttatch(audioProcessor.apvts, "oddSynth", oddSynthVol),
    evenSynthLPAttatch(audioProcessor.apvts, "evenLowPass", evenSynthLP),
    oddSynthLPAttatch(audioProcessor.apvts, "oddLowPass", oddSynthLP)

{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (800, 200);
    freqLabel.setText("Frequency: 0.0 Hz", juce::dontSendNotification);
    freqLabel.setFont(juce::Font(TEXT_HEIGHT_KNOB_LABELS));
    freqLabel.setJustificationType(juce::Justification::centred);

    knobLabels.setText("Odd LP         Odd Synth         Odd Harm            Fundamental            Even Harm         Even Synth         Even LP", 
        juce::dontSendNotification);
    knobLabels.setFont(juce::Font(TEXT_HEIGHT_KNOB_LABELS));
    knobLabels.setJustificationType(juce::Justification::centred);

    //make all of the knobs and labels visible on the GUI
    addAndMakeVisible(knobLabels);
    addAndMakeVisible(freqLabel);
    addAndMakeVisible(fundamentalVol);
    addAndMakeVisible(evenHarmVol);
    addAndMakeVisible(oddHarmVol);
    addAndMakeVisible(evenSynthVol);
    addAndMakeVisible(oddSynthVol);
    addAndMakeVisible(evenSynthLP);
    addAndMakeVisible(oddSynthLP);
    startTimerHz(10);
}

Harmonicator9000AudioProcessorEditor::~Harmonicator9000AudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void Harmonicator9000AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
}

void Harmonicator9000AudioProcessorEditor::resized()
{
    //use the built in bounds component to set where all of the knobs will be located(and their size)
    //note that each time a remove is cakled, the space gets smaller, so to do 1/3 1/3 1/3 its 0.33 0.5 1.0
    auto knobBounds = getLocalBounds();
    knobLabels.setBounds(knobBounds.removeFromTop(knobBounds.getHeight() * 0.1));
    auto oddHarmonicSector = knobBounds.removeFromLeft(knobBounds.getWidth() * 0.4); //left 40% of the area
    auto fundamentalSector = knobBounds.removeFromLeft(knobBounds.getWidth() * 0.33); //middle 20% of the area
    auto evenHarmonicSector = knobBounds.removeFromLeft(knobBounds.getWidth() * 1.0); //right 40% of the area

    fundamentalVol.setBounds(fundamentalSector.removeFromBottom(fundamentalSector.getHeight() * 0.75)); //leave some room above to display freq value
    freqLabel.setBounds(fundamentalSector.removeFromBottom(fundamentalSector.getHeight() * 1.0));
    
    oddSynthLP.setBounds(oddHarmonicSector.removeFromLeft(oddHarmonicSector.getWidth() * 0.33)); //each odd control gets 1/3 of the space
    oddSynthVol.setBounds(oddHarmonicSector.removeFromLeft(oddHarmonicSector.getWidth() * 0.5));
    oddHarmVol.setBounds(oddHarmonicSector.removeFromLeft(oddHarmonicSector.getWidth() * 1.0));

    evenHarmVol.setBounds(evenHarmonicSector.removeFromLeft(evenHarmonicSector.getWidth() * 0.33)); //same for even
    evenSynthVol.setBounds(evenHarmonicSector.removeFromLeft(evenHarmonicSector.getWidth() * 0.5));
    evenSynthLP.setBounds(evenHarmonicSector.removeFromLeft(evenHarmonicSector.getWidth() * 1.0));

}

void Harmonicator9000AudioProcessorEditor::timerCallback() {
    freqLabel.setText(std::to_string(juce::truncatePositiveToUnsignedInt(Harmonicator9000AudioProcessor::fundamentalFreq)) + " Hz", juce::dontSendNotification);
    
}
