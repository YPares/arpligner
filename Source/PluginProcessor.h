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
class ArplignerAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    ArplignerAudioProcessor();
    ~ArplignerAudioProcessor() override;

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
  juce::AudioParameterChoice* instanceBehaviour;
  juce::AudioParameterChoice* firstDegreeCode;
  juce::AudioParameterBool* chordNotesPassthrough;
  juce::AudioParameterChoice* whenNoChordNote;
  juce::AudioParameterChoice* whenSingleChordNote;
  juce::AudioParameterBool* ignoreBlackKeysInPatterns;

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArplignerAudioProcessor)
};


namespace InstanceBehaviour {
enum Enum {
  // Values between 1 & 16 are for isolated instance behaviour, where the value
  // indicates the midi channel corresponding to the chord track. Values of 16+
  // are for when using multiple instances of Arpligner, one being the chord
  // track.
  BYPASS = 0,
  IS_GLOBAL_CHORD_TRACK = 17,
  IS_PATTERN_TRACK,
  IS_PATTERN_TRACK_DELAY_1_BUFFER
};
}

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
