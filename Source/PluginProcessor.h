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
class ArplignerJuceAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    ArplignerJuceAudioProcessor();
    ~ArplignerJuceAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    virtual void runArp(juce::MidiBuffer&) = 0;
    
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

protected:
  juce::AudioParameterInt* chordChan;
  juce::AudioParameterInt* firstDegreeCode;
  juce::AudioParameterBool* chordNotesPassthrough;
  juce::AudioParameterChoice* whenNoChordNote;
  juce::AudioParameterChoice* whenSingleChordNote;
  juce::AudioParameterBool* ignoreBlackKeysInPatterns;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArplignerJuceAudioProcessor)
};

namespace WhenNoChordNote {
enum Enum {
  LATCH_LAST_CHORD = 0,
  SILENCE,
  USE_PATTERN_AS_NOTES
};
}
namespace WhenSingleChordNote {
enum Enum {
  TRANSPOSE_LAST_CHORD = 0,
  POWERCHORD,
  USE_AS_IS,
  SILENCE,
  USE_PATTERN_AS_NOTES
};
}
