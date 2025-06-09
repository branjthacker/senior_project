/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"



//==============================================================================
Harmonicator9000AudioProcessor::Harmonicator9000AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )

#endif
{
}

Harmonicator9000AudioProcessor::~Harmonicator9000AudioProcessor()
{
}

//==============================================================================
const juce::String Harmonicator9000AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Harmonicator9000AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Harmonicator9000AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Harmonicator9000AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Harmonicator9000AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Harmonicator9000AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Harmonicator9000AudioProcessor::getCurrentProgram()
{
    return 0;
}

void Harmonicator9000AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Harmonicator9000AudioProcessor::getProgramName (int index)
{
    return {};
}

void Harmonicator9000AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Harmonicator9000AudioProcessor::addToCorr(float sample) noexcept{
    //check if the index is at the end of the queue, if so start a new fft process
    if (corrCounter == LARGE_PITCH_ARRAY_SIZE) {
        if (!processingAvg) {
            std::copy(largePitchArray.begin(), largePitchArray.end(), avgVolArray.begin());
            processingAvg = true;
            //spawn a thread to do our dirty work
            std::thread avgThread(&Harmonicator9000AudioProcessor::updateAvg, this);
            avgThread.detach(); //let the thread go frolic on its own
        }
        if (!nextCorrBlockReady) {
            //the correlation calcs have completed, we can start a new one
            //add the first 256 samples in largePitchArray to the small array
            std::copy(largePitchArray.begin(), largePitchArray.begin() + SMALL_PITCH_ARRAY_SIZE, smallPitchArray.begin());
            nextCorrBlockReady = true;
            //spawn a thread to go calculate the new fundamental frequency (currently producing like 80 threads)
            std::thread corrThread(&Harmonicator9000AudioProcessor::getFundamentalFrequency, this);
            corrThread.detach(); //let the thread go frolic on its own

        }
        corrCounter = 0;
    }
    //add sample and advance the counter
    largePitchArray[corrCounter] = sample;
    corrCounter++;
}
//==============================================================================
//wave generators

void Harmonicator9000AudioProcessor::getNextSquare() noexcept {
    int i = 0;
    while (i < squareOutBuff.size()) {
        //update what sample we are at
        squareNumSamples++;
        if (squareNumSamples >= cycleTimeSamples) {
            squareNumSamples = 0;
        }
        //if we are in the first half of the cycle, return full
        if (squareNumSamples < (cycleTimeSamples / 2)) {
            squareOutBuff[i] = juce::Decibels::decibelsToGain(evenSynthVol) * avgVol;
        }
        else {
            squareOutBuff[i] = -(juce::Decibels::decibelsToGain(evenSynthVol) * avgVol);
        }
        i++;
    }
}

void Harmonicator9000AudioProcessor::getNextSaw() noexcept {
    //update, except the count goes in reverse to be able to build the wave properly
    int i = 0;
    while (i < sawOutBuff.size()) {
        sawNumSamples--;
        if (sawNumSamples <= 0) {
            sawNumSamples = cycleTimeSamples;
        }
        //return the ratio of the num samples / cycle time samples to get the saw pattern * vol
        sawOutBuff[i] =  (static_cast<float>(sawNumSamples) / static_cast<float>(cycleTimeSamples)) * juce::Decibels::decibelsToGain(oddSynthVol) * avgVol;
        i++;
    }

}

//==============================================================================
void Harmonicator9000AudioProcessor::getFundamentalFrequency() noexcept{
    //while we're here, this is a great time to see im the user updated any knobs
    getUserDefinedSettings(apvts);
    //perform the autocorrelation, find the first strongest peak, do math to determine frequency
    //keep track of the most minimum index ( this will give how many samples our wave cycle is (hopefully))
    //todo: add some more percice checking, it can easily drop up or down an octave right now.
    //do not update the pitch below a certian volume
    int minIndex = 0;
    float minVal = 999999999999; //some absurdly large number
    //keep track of how far we've shifted the larger array
    int indexOffset = 8; //start slightly offset because the first samples will obviously line up.
    
    while (indexOffset < LARGE_PITCH_ARRAY_SIZE - SMALL_PITCH_ARRAY_SIZE) {
        float accumDiff = 0;
        int i = 0;
        while (i < SMALL_PITCH_ARRAY_SIZE) {
            //go through each sample of the small array and subtract it from the big array at it's offset index from i
            accumDiff += abs(smallPitchArray[i] - largePitchArray[i + indexOffset]);
            i++;
        }
        //compare this to the local minimum and maximum
        if (accumDiff < minVal) {
            minIndex = indexOffset;
            minVal = accumDiff;
        }
        indexOffset++;
    }
    //if the frequency change is significant, update it
    if ((minIndex > cycleTimeSamples + CRITICAL_SAMPLE_SHIFT ||
        minIndex < cycleTimeSamples - CRITICAL_SAMPLE_SHIFT) && 
        avgVol > CRITICAL_VOLUME_THRESH) {
        //map this to an analog frequency based on sample rate. (sample rate / minIndex)
        fundamentalFreq = sampleRate / minIndex;
        cycleTimeSamples = minIndex; //update for the wave generators
    }
    nextCorrBlockReady = false; //new thread can be spawned now, we're leaving this one

}
//==============================================================================
void Harmonicator9000AudioProcessor::updateAvg() noexcept {
    //use the avgVolArray to update avgVol.
    int i = 0;
    float tmpAvg = 0.0;
    while (i < avgVolArray.size()) {
        tmpAvg += abs(avgVolArray[i]);
        i++;
    }
    avgVol = tmpAvg / LARGE_PITCH_ARRAY_SIZE;
    processingAvg = false;

}
//==============================================================================
void Harmonicator9000AudioProcessor::updateFilters() noexcept {
    //use the booleans to check which filter we need to update then update that one
    float fundamentalCopy = fundamentalFreq; //make a copy so it remains consistent throughout the thread
    float oddVolCopy = oddHarmVol;
    float evenVolCopy = evenHarmVol;
    float fundVolCopy = fundamentalVol;
    //reset these flags
    lastFreq = fundamentalCopy;
    lastEvenVol = evenVolCopy;
    lastOddVol = oddVolCopy;
    lastFundVol = fundVolCopy;
    //prepare all the coeficients (they will be the same no matter which group is updated)
    auto genericCoefs = juce::dsp::IIR::Coefficients<float>::makeAllPass(
        sampleRate,
        300
    );
    auto fundamentalCoefs = genericCoefs;
    auto oddOneCoefs = genericCoefs;
    auto oddTwoCoefs = genericCoefs;
    auto oddThreeCoefs = genericCoefs;
    auto oddFourCoefs = genericCoefs;
    auto evenOneCoefs = genericCoefs;
    auto evenTwoCoefs = genericCoefs;
    auto evenThreeCoefs = genericCoefs;
    auto evenFourCoefs = genericCoefs;

    if (fundamentalCopy < sampleRate / 2) {
        fundamentalCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(fundVolCopy)
        );
    }
    if (fundamentalCopy * 3 < sampleRate / 2) {
        oddOneCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy * 3,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(oddVolCopy)
        );
    }
    if (fundamentalCopy * 5 < sampleRate / 2) {
        oddTwoCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy * 5,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(oddVolCopy)
        );
    }
    if (fundamentalCopy * 7 < sampleRate / 2) {
        oddThreeCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy * 7,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(oddVolCopy)
        );
    }
   /* if (fundamentalCopy * 9 < sampleRate / 2) {
        oddFourCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy * 9,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(oddVolCopy)
        );
    }*/
    if (fundamentalCopy * 2 < sampleRate / 2) {
        evenOneCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy * 2,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(evenVolCopy)
        );
    }
    if (fundamentalCopy * 4 < sampleRate / 2) {
        evenTwoCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy * 4,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(evenVolCopy)
        );
    }
    if (fundamentalCopy * 6 < sampleRate / 2) {
        evenThreeCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy * 6,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(evenVolCopy)
        );
    }
   /* if (fundamentalCopy * 8 < sampleRate / 2) {
        evenFourCoefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            sampleRate,
            fundamentalCopy * 8,
            FILTER_QUALITY,
            juce::Decibels::decibelsToGain(evenVolCopy)
        );
    }*/
    if (!grpArdy && !usingGrpA) {
        //update group A here
        fundamentalBand_groupA.coefficients = fundamentalCoefs;
        firstOddBand_groupA.coefficients = oddOneCoefs;
        secondOddBand_groupA.coefficients = oddTwoCoefs;
        thirdOddBand_groupA.coefficients = oddThreeCoefs;
      //  fourthOddBand_groupA.coefficients = oddFourCoefs;
        firstEvenBand_groupA.coefficients = evenOneCoefs;
        secondEvenBand_groupA.coefficients = evenTwoCoefs;
        thirdEvenBand_groupA.coefficients = evenThreeCoefs;
       // fourthEvenBand_groupA.coefficients = evenFourCoefs;
        grpArdy = true;
        
    }
    else if (!grpBrdy && !usingGrpB) {
        //update group B here
        fundamentalBand_groupB.coefficients = fundamentalCoefs;
        firstOddBand_groupB.coefficients = oddOneCoefs;
        secondOddBand_groupB.coefficients = oddTwoCoefs;
        thirdOddBand_groupB.coefficients = oddThreeCoefs;
      //  fourthOddBand_groupB.coefficients = oddFourCoefs;
        firstEvenBand_groupB.coefficients = evenOneCoefs;
        secondEvenBand_groupB.coefficients = evenTwoCoefs;
        thirdEvenBand_groupB.coefficients = evenThreeCoefs;
      //  fourthEvenBand_groupB.coefficients = evenFourCoefs;
        grpBrdy = true;
    }
}

//==============================================================================
void Harmonicator9000AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    //update the sample rate
    Harmonicator9000AudioProcessor::sampleRate = sampleRate;

    //set the mode of various filters
    oddLowPass.setMode(juce::dsp::LadderFilterMode::LPF24);
    evenLowPass.setMode(juce::dsp::LadderFilterMode::LPF24);

    //set the synth output buffers to the samplesPerBlock size
    squareOutBuff.resize(samplesPerBlock);
    sawOutBuff.resize(samplesPerBlock);
    //create a spec to use for all of the filters
    juce::dsp::ProcessSpec filtSpec;
    filtSpec.sampleRate = sampleRate;
    filtSpec.maximumBlockSize = samplesPerBlock;
    filtSpec.numChannels = getTotalNumOutputChannels();

    //prepare all of the filters
    oddLowPass.prepare(filtSpec);
    evenLowPass.prepare(filtSpec);
    fundamentalBand_groupA.prepare(filtSpec);
    firstOddBand_groupA.prepare(filtSpec);
    secondOddBand_groupA.prepare(filtSpec);
    thirdOddBand_groupA.prepare(filtSpec);
    fourthOddBand_groupA.prepare(filtSpec);
    firstEvenBand_groupA.prepare(filtSpec);
    secondEvenBand_groupA.prepare(filtSpec);
    thirdEvenBand_groupA.prepare(filtSpec);
    fourthEvenBand_groupA.prepare(filtSpec);
    //2nd group to enable processing while calculating coefficients in parallel
    fundamentalBand_groupB.prepare(filtSpec);
    firstOddBand_groupB.prepare(filtSpec);
    secondOddBand_groupB.prepare(filtSpec);
    thirdOddBand_groupB.prepare(filtSpec);
    fourthOddBand_groupB.prepare(filtSpec);
    firstEvenBand_groupB.prepare(filtSpec);
    secondEvenBand_groupB.prepare(filtSpec);
    thirdEvenBand_groupB.prepare(filtSpec);
    fourthEvenBand_groupB.prepare(filtSpec);

    //set up filters in a startup state so that the process block will actually work
    usingGrpA = true;
    grpArdy = true;
    grpBrdy = false;
    usingGrpB = false;
    //this should make group b activate using defaults, then things will start updating as normal
    updateFilters();

    getUserDefinedSettings(apvts);
}

void Harmonicator9000AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Harmonicator9000AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void Harmonicator9000AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());


    
    //generate and filter the buffers from each synth engine, then just add them in in the below processing (might need to thread this later)
    if (evenSynthVol > -100.0 && avgVol > CRITICAL_VOLUME_THRESH) {
        evenLowPass.setCutoffFrequencyHz(evenLP);
        getNextSquare();
        //wrap the buffer in a context (this is how JUCE needs it to happen apperantly)
        jassert(squareOutBuff.data() != nullptr);
        std::vector<float*> filterData = { squareOutBuff.data() };
        juce::dsp::AudioBlock<float> squareBlock(filterData.data(), 1, squareOutBuff.size());
        juce::dsp::ProcessContextReplacing<float> context(squareBlock);
        //filter it
        evenLowPass.process(context);
    }
    if (oddSynthVol > -100.0 && avgVol > CRITICAL_VOLUME_THRESH) {
        oddLowPass.setCutoffFrequencyHz(oddLP);
        getNextSaw();
        //wrap the buffer in a context (this is how JUCE needs it to happen apperantly)
        jassert(sawOutBuff.data() != nullptr);
        std::vector<float*> filterData = { sawOutBuff.data() };
        juce::dsp::AudioBlock<float> sawBlock(filterData.data(), 1, sawOutBuff.size());
        juce::dsp::ProcessContextReplacing<float> context(sawBlock);
        //filter it
        oddLowPass.process(context);
    }
    //check on how our filters are doing, switch them up if needed
    if (!((lastFundVol == fundamentalVol) && (lastFreq == fundamentalFreq) && (lastOddVol == oddHarmVol) && (lastEvenVol == evenHarmVol))) {
        if (usingGrpA && grpBrdy) {
            //switch over to group B, spawn a thread to recalculate group A
            usingGrpB = true;
            usingGrpA = false;
            grpArdy = false;
            std::thread filterThread(&Harmonicator9000AudioProcessor::updateFilters, this);
            filterThread.detach(); //let the thread go frolic on its own

        }
        else if (usingGrpB && grpArdy) {
            usingGrpB = false;
            usingGrpA = true;
            grpBrdy = false;
            std::thread filterThread(&Harmonicator9000AudioProcessor::updateFilters, this);
            filterThread.detach(); //let the thread go frolic on its own
        }
    }
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        //here: call a function that will add the sample to the DSP FIFO, increment the counter, set the
        //boolean if we are ready to do the calc (and reset the pointer)
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            if (channel == 1) {
                addToCorr(channelData[i]); //only process one channel for frequency or the buffers will get messed up
            }
            if (evenSynthVol > -100.0 && avgVol > CRITICAL_VOLUME_THRESH) {
                channelData[i] += squareOutBuff[i];
            }
            if (oddSynthVol > -100.0 && avgVol > CRITICAL_VOLUME_THRESH) {
                channelData[i] += sawOutBuff[i];
            }
            channelData[i] = channelData[i] / 2; //vol reduction to prevent peaking
        }
        //process the audio through the harmonic filtering
        std::vector<float*> harmFiltData = { channelData };
        juce::dsp::AudioBlock<float> harmBlock(harmFiltData.data(), 1, buffer.getNumSamples());
        juce::dsp::ProcessContextReplacing<float> harmContext(harmBlock);
        if (usingGrpA) {
            fundamentalBand_groupA.process(harmContext);
            firstOddBand_groupA.process(harmContext);
            secondOddBand_groupA.process(harmContext);
            thirdOddBand_groupA.process(harmContext);
            //fourthOddBand_groupA.process(harmContext);
            firstEvenBand_groupA.process(harmContext);
            secondEvenBand_groupA.process(harmContext);
            thirdEvenBand_groupA.process(harmContext);
            //fourthEvenBand_groupA.process(harmContext);
        }
        else if (usingGrpB) {
            fundamentalBand_groupB.process(harmContext);
            firstOddBand_groupB.process(harmContext);
            secondOddBand_groupB.process(harmContext);
            thirdOddBand_groupB.process(harmContext);
            //fourthOddBand_groupB.process(harmContext);
            firstEvenBand_groupB.process(harmContext);
            secondEvenBand_groupB.process(harmContext);
            thirdEvenBand_groupB.process(harmContext);
            //fourthEvenBand_groupB.process(harmContext);
        }
    }
}

//==============================================================================
bool Harmonicator9000AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Harmonicator9000AudioProcessor::createEditor()
{
    //return new juce::GenericAudioProcessorEditor(*this);

    //use a custom GUI instead of the generic one provided
    return new Harmonicator9000AudioProcessorEditor (*this);
}

//==============================================================================
void Harmonicator9000AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    //store the state of the plugin such that it saves user defined parameters between loads
    juce::MemoryOutputStream memParamSave(destData, true);
    apvts.state.writeToStream(memParamSave);

}

void Harmonicator9000AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    //load in the user saved state of parameters instead of resetting everything to default values
    auto restoredParams = juce::ValueTree::readFromData(data, sizeInBytes);
    if (restoredParams.isValid()) { //check if the data is copied, if not will auto reset to defaults
        apvts.replaceState(restoredParams);
        //might have to call another function here, unsure at this point. 
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
Harmonicator9000AudioProcessor::createParameterLayout() {
    //create all the parameters we will be using as knobs with ranges that
    //make sense (for band volume +/- 15dB, for filtering 100Hz->20kHz,
    // for synth volume -inf (-100 will be fine, check for this and just don't activate synth if val = -100) to 0))
    //layed out in the order they will appear on the panel
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>("oddLowPass",
        "Odd Low Pass", 100.0, 20000.0, 20000.0));

    layout.add(std::make_unique<juce::AudioParameterFloat>("oddSynth",
        "Odd Synth", -100.0, 0.0, -100.0));

    layout.add(std::make_unique<juce::AudioParameterFloat>("oddHarmonics",
        "Odd Harmonics", -15.0, 15.0, 0.0));

    layout.add(std::make_unique<juce::AudioParameterFloat>("fundamental",
        "Fundamental Frequency", -15.0, 15.0, 0.0));

    layout.add(std::make_unique<juce::AudioParameterFloat>("evenHarmonics",
        "Even Harmonics", -15.0, 15.0, 0.0));

    layout.add(std::make_unique<juce::AudioParameterFloat>("evenSynth",
        "Even Synth", -100.0, 0.0, -100.0));

    layout.add(std::make_unique<juce::AudioParameterFloat>("evenLowPass",
        "Even Low Pass", 100.0, 20000.0, 20000.0));

    return layout;
}

void getUserDefinedSettings(juce::AudioProcessorValueTreeState& apvts) {
    //populates all of the settings as they are defined in the GUI
    Harmonicator9000AudioProcessor::oddLP = apvts.getRawParameterValue("oddLowPass")->load();
    Harmonicator9000AudioProcessor::oddSynthVol = apvts.getRawParameterValue("oddSynth")->load();
    Harmonicator9000AudioProcessor::oddHarmVol = apvts.getRawParameterValue("oddHarmonics")->load();
    Harmonicator9000AudioProcessor::fundamentalVol = apvts.getRawParameterValue("fundamental")->load();
    Harmonicator9000AudioProcessor::evenHarmVol = apvts.getRawParameterValue("evenHarmonics")->load();
    Harmonicator9000AudioProcessor::evenSynthVol = apvts.getRawParameterValue("evenSynth")->load();
    Harmonicator9000AudioProcessor::evenLP = apvts.getRawParameterValue("evenLowPass")->load();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Harmonicator9000AudioProcessor();
}

float Harmonicator9000AudioProcessor::fundamentalFreq = 100.0; // Define static freq variable outside of the class
float Harmonicator9000AudioProcessor::evenSynthVol = -100.0; //same with all the other adjustable variables
float Harmonicator9000AudioProcessor::oddSynthVol = -100.0;
float Harmonicator9000AudioProcessor::fundamentalVol = 0.0;
float Harmonicator9000AudioProcessor::oddHarmVol = 0.0;
float Harmonicator9000AudioProcessor::evenHarmVol = 0.0;
float Harmonicator9000AudioProcessor::oddLP = 20000.0;
float Harmonicator9000AudioProcessor::evenLP = 20000.0;
int Harmonicator9000AudioProcessor::cycleTimeSamples = 1; //cycle time in samples (calculated based off frequency each time it changes, can never be 0)
int Harmonicator9000AudioProcessor::squareNumSamples = 0; //number of samples square wave generator has spent in the current cycle
int Harmonicator9000AudioProcessor::sawNumSamples = 0; //number of samples saw wave generator has spent in the current cycle
float Harmonicator9000AudioProcessor::avgVol = 0.0;
