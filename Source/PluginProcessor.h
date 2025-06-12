/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#define SMALL_PITCH_ARRAY_SIZE 200
#define LARGE_PITCH_ARRAY_SIZE 2500
#define CRITICAL_SAMPLE_SHIFT 5 //the amount of samples that are needed to trigger an actual change
#define CRITICAL_VOLUME_THRESH 0.05 //avg input volume must be above this for any synth generation or frequency updating (basically a gate)
#define FILTER_QUALITY 10.0 //define the Q for low pass filters on synth generators (adjust to taste)
#define PITCH_DETECTION_THRESH 1.8 //must be at least this amount smaller for a new pitch to be registered
#define MINIMUM_FREQ 40 //define the minimum and maximum frequencies servicable by the plugin (setup for bass, could add toggle in the future)
#define MAX_FREQ 392
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
    bool coefficientsRdy = false; //say weather or not new coefficients are ready
    float lastFreqPitch = 1.0; //the previous frequency, this needs to equal current frequency for an actual pitch update to prevent glitching
 
    //variables that hold the last state of vol and freq, we only update filters if they actually change
    float lastFreq= 1.0;
    float lastFundVol = 0.0;
    float lastOddVol = 0.0;
    float lastEvenVol = 0.0;
    //declare all of the coefficient holding variables
    juce::dsp::IIR::Coefficients<float>::Ptr fundamentalCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr oddOneCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr oddTwoCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr oddThreeCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr oddFourCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr evenOneCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr evenTwoCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr evenThreeCoefs;
    juce::dsp::IIR::Coefficients<float>::Ptr evenFourCoefs;
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

    //declare all of the filters for our fundamental frequency and harmonics (high Q peaking filters, stereo)
    juce::dsp::IIR::Filter<float> fundamentalBandL;
    juce::dsp::IIR::Filter<float> firstOddBandL;
    juce::dsp::IIR::Filter<float> secondOddBandL;
    juce::dsp::IIR::Filter<float> thirdOddBandL;
    juce::dsp::IIR::Filter<float> fourthOddBandL;
    juce::dsp::IIR::Filter<float> firstEvenBandL;
    juce::dsp::IIR::Filter<float> secondEvenBandL;
    juce::dsp::IIR::Filter<float> thirdEvenBandL;
    juce::dsp::IIR::Filter<float> fourthEvenBandL;

    juce::dsp::IIR::Filter<float> fundamentalBandR;
    juce::dsp::IIR::Filter<float> firstOddBandR;
    juce::dsp::IIR::Filter<float> secondOddBandR;
    juce::dsp::IIR::Filter<float> thirdOddBandR;
    juce::dsp::IIR::Filter<float> fourthOddBandR;
    juce::dsp::IIR::Filter<float> firstEvenBandR;
    juce::dsp::IIR::Filter<float> secondEvenBandR;
    juce::dsp::IIR::Filter<float> thirdEvenBandR;
    juce::dsp::IIR::Filter<float> fourthEvenBandR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Harmonicator9000AudioProcessor)
};
