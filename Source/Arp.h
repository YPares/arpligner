/*
  ==============================================================================

    Arp.h
    Created: 12 Jan 2023 11:16:13pm
    Author:  Yves Parès

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ChordStore.h"

using namespace juce;

using NoteOnChan = int;

using Mappings = HashMap< NoteOnChan, Array<NoteNumber> >;

class Arp : public ArplignerAudioProcessor {
private:
  ChordStore mLocalChordStore;

  // On each pattern chan, to which note has been mapped each incoming
  // NoteNumber, so we can send the correct NOTE OFFs afterwards
  Mappings mCurMappings;

  void updateChordStore(ChordStore* chordStore) {
    chordStore->updateCurrentChord
    ((WhenNoChordNote::Enum)whenNoChordNote->getIndex(),
      (WhenSingleChordNote::Enum)whenSingleChordNote->getIndex());
  }

  ChordStore* getChordStore(InstanceBehaviour::Enum beh) {
    if (beh >= InstanceBehaviour::IS_CHORD)
      return GlobalChordStore::getInstance();
    else
      return &mLocalChordStore;
  }

  void processPatternNotes(ChordStore* chd, Array<MidiMessage>&, Array<MidiMessage>&, MidiBuffer&);

  //void finalizeMappings(MidiBuffer&);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Arp);

public:
  Arp() : ArplignerAudioProcessor() {
    mCurMappings.clear();
  }

  void prepareToPlay(double, int) override;

  void runArp(MidiBuffer&) override;
};
