/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class Harmonicator9000AudioProcessor  : public juce::AudioProcessor
{
public:

    //==============================================================================
    static constexpr auto fftOrder = 12; //define an fft size of 4096 for enough tracking resolution
    static constexpr auto fftSize = 1 << fftOrder;
    static float fundamentalFreq;
    //==============================================================================
    Harmonicator9000AudioProcessor();
    ~Harmonicator9000AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{ *this, nullptr, "Parameters",
    createParameterLayout()};

private:
    //==============================================================================
    juce::dsp::FFT forwardFFT; //the actual FFT object
    std::array<float, fftSize> fifo; //queue to hold input samples
    std::array<float, fftSize * 2> fftData; //queue to hold output sample
    int fifoCounter = 0; //counts up to 4096 samples, triggers FFT, then resets
    bool nextFFTBlockReady = false; //set true when we want to actually do the fft
    double sampleRate = 48000; //default sample rate, change in process audio block
    //function to add sample to fft
    void addToFFT(float sample) noexcept;
    //compute fft then find the fundamental(this should be spawned in a thread or fork)
    void getFundamentalFrequency() noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Harmonicator9000AudioProcessor)
};
