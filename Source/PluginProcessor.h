/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define SMALL_PITCH_ARRAY_SIZE 256
#define LARGE_PITCH_ARRAY_SIZE 3000
#define CRITICAL_SAMPLE_SHIFT 5 //the amount of samples that are needed to trigger an actual change
#define CRITICAL_VOLUME_THRESH 0.005 //avg input volume must be above this for any synth generation or frequency updating (basically a gate)
#define FILTER_QUALITY 2.0 //define the Q for low pass filters on synth generators (adjust to taste)

void getUserDefinedSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class Harmonicator9000AudioProcessor  : public juce::AudioProcessor
{
public:

    //==============================================================================
    static float fundamentalFreq;
    static int cycleTimeSamples; //cycle time in samples (calculated based off frequency each time it changes)
    static int squareNumSamples; //number of samples square wave generator has spent in the current cycle
    static int sawNumSamples; //number of samples saw wave generator has spent in the current cycle
    static float evenSynthVol;
    static float oddSynthVol;
    static float fundamentalVol;
    static float oddHarmVol;
    static float evenHarmVol;
    static float oddLP;
    static float evenLP;
    static float avgVol; //average volume for the last few ms normalized between 0 and 1
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
    
    std::array<float, LARGE_PITCH_ARRAY_SIZE> largePitchArray;
    std::array<float, SMALL_PITCH_ARRAY_SIZE> smallPitchArray;
    std::array<float, LARGE_PITCH_ARRAY_SIZE> avgVolArray; //copy into this each time we start a new freq calc, will update avg. vol
    std::vector<float> squareOutBuff; //these will be reassigned to proper size in prepareToPlay
    std::vector<float> sawOutBuff;
    int corrCounter = 0; //counts up to LARGE_PITCH_ARRAY_SIZE samples, fills buffers and triggers a calc, then resets
    bool nextCorrBlockReady = false; //set true when the corr is triggered, corr sets false when it is done.
    bool processingAvg = false; //set high before the processingAvg function is called, set low when done
    //booleans to communicate between processes about which filters are in what stage
    bool grpArdy = false;
    bool grpBrdy = false;
    bool usingGrpA = false;
    bool usingGrpB = false;
    //variables that hold the last state of vol and freq, we only update filters if they actually change
    float lastFreq= 1.0;
    float lastFundVol = 0.0;
    float lastOddVol = 0.0;
    float lastEvenVol = 0.0;

    double sampleRate = 48000; //default sample rate, change in process audio block
    //function to add sample to fft
    void addToCorr(float sample) noexcept;
    //compute fft then find the fundamental(this should be spawned in a thread or fork)
    void getFundamentalFrequency() noexcept;
    //update the average
    void updateAvg() noexcept;
    //function to get the next square wave sample
    void getNextSquare() noexcept;
    //function to get the next saw wave sample
    void getNextSaw() noexcept;
    //function to update filter coefficients
    void updateFilters() noexcept;

    //declare the filters for each of our synth ocillators 
    juce::dsp::LadderFilter<float> oddLowPass;
    juce::dsp::LadderFilter<float> evenLowPass;

    //declare all of the filters for our fundamental frequency and harmonics (high Q peaking filters)
    juce::dsp::IIR::Filter<float> fundamentalBand_groupA;
    juce::dsp::IIR::Filter<float> firstOddBand_groupA;
    juce::dsp::IIR::Filter<float> secondOddBand_groupA;
    juce::dsp::IIR::Filter<float> thirdOddBand_groupA;
    juce::dsp::IIR::Filter<float> fourthOddBand_groupA;
    juce::dsp::IIR::Filter<float> firstEvenBand_groupA;
    juce::dsp::IIR::Filter<float> secondEvenBand_groupA;
    juce::dsp::IIR::Filter<float> thirdEvenBand_groupA;
    juce::dsp::IIR::Filter<float> fourthEvenBand_groupA;
    //2nd group to enable processing while calculating coefficients in parallel
    juce::dsp::IIR::Filter<float> fundamentalBand_groupB;
    juce::dsp::IIR::Filter<float> firstOddBand_groupB;
    juce::dsp::IIR::Filter<float> secondOddBand_groupB;
    juce::dsp::IIR::Filter<float> thirdOddBand_groupB;
    juce::dsp::IIR::Filter<float> fourthOddBand_groupB;
    juce::dsp::IIR::Filter<float> firstEvenBand_groupB;
    juce::dsp::IIR::Filter<float> secondEvenBand_groupB;
    juce::dsp::IIR::Filter<float> thirdEvenBand_groupB;
    juce::dsp::IIR::Filter<float> fourthEvenBand_groupB;

    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Harmonicator9000AudioProcessor)
};
