/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

using namespace juce;


//==============================================================================
/**
*/
class ArplignerAudioProcessor : public AudioProcessor
#if JucePlugin_Enable_ARA
  , public AudioProcessorARAExtension
#endif
{
public:
  //==============================================================================
  ArplignerAudioProcessor();
  ~ArplignerAudioProcessor() override;

  //==============================================================================
  void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

  void processBlock(AudioBuffer<float>&, MidiBuffer&) override;

  virtual void runArp(MidiBuffer&) = 0;

  //==============================================================================
  AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const String getProgramName(int index) override;
  void changeProgramName(int index, const String& newName) override;

  //==============================================================================
  void getStateInformation(MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

protected:
  AudioParameterChoice* instanceBehaviour;
  AudioParameterChoice* whenNoChordNote;
  AudioParameterChoice* whenSingleChordNote;
  AudioParameterChoice* firstDegreeCode;
  AudioParameterChoice* patternNotesMapping;
  AudioParameterInt*    numMillisecsOfLatency;
  AudioParameterChoice* patternNotesWraparound;
  AudioParameterChoice* unmappedNotesBehaviour;
  AudioParameterChoice* preMappingChordProcessing;
  AudioParameterBool*   holdCurState;

private:
  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArplignerAudioProcessor)
};


namespace InstanceBehaviour {
  enum Enum {
    // Values between 1 & 16 are for Multi-channel behaviour, where the value
    // indicates the midi channel corresponding to the chord track. Values of 17+
    // are for Multi-instance behaviour
    BYPASS = 0,
    IS_CHORD = 17,
    IS_PATTERN
  };
}

namespace WhenNoChordNote {
  enum Enum {
    SILENCE = 0,
    USE_PATTERN_AS_NOTES,
    LATCH_LAST_CHORD
  };
}

namespace WhenSingleChordNote {
  enum Enum {
    SILENCE = 0,
    USE_PATTERN_AS_NOTES,
    USE_AS_IS,
    POWERCHORD,
    TRANSPOSE_LAST_CHORD
  };
}

namespace PreMappingChordProcessing {
  enum Enum {
    NONE = 0,
    IGNORE_BASS_NOTE,
    ADD_WHOLE_STEPS,
    ADD_WHOLE_STEPS_DEF_P4_P5_MAJ7
  };
}

namespace PatternNotesMapping {
  enum Enum {
    ALWAYS_LEAVE_UNMAPPED = 0,
    SEMITONE_TO_DEGREE,
    WHITE_NOTE_TO_DEGREE
  };
}

namespace PatternNotesWraparound {
  enum Enum {
    NO_WRAPAROUND = 0,
    AFTER_ALL_DEGREES = 1,
    // Values of 2 and above indicate a specific number of notes after which to
    // wrap around. This effectively discards all the chord/scale degrees above
    // that number, and leaving unmapped pattern notes that are above the last
    // degree of the chord/scale but before the wraparound value.
  };
}

namespace UnmappedNotesBehaviour {
  enum Enum {
    SILENCE = 0,
    USE_AS_IS,
    TRANSPOSE_FROM_FIRST_DEGREE,
    PLAY_ALL_DEGREES_UP_TO_NOTE
  };
}
