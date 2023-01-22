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

#define IS_NOTE_MESSAGE(msg) (msg.isNoteOn() || msg.isNoteOff())


class Arp : public ArplignerAudioProcessor {
private:
  ChordStore mLocalChordStore;
  Array<MidiMessage> mLastBufferMessagesToProcess;
  
  // On each pattern chan, to which note is currently mapped each incoming NoteNumber
  HashMap<NoteNumber, NoteNumber> curMappings[16];
  
public:
  void runArp(MidiBuffer&);
};
