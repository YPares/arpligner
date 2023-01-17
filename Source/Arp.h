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

  int getDegreeShift(NoteNumber nn) {
    int x = nn - firstDegreeCode->getIndex();
    if (ignoreBlackKeysInPatterns->get()) {
      // We need to correct the [firstDegreeCode,nn] interval for the amount
      // of black keys it contains:
      int sign = (x < 0) ? -1 : 1;
      int absX = sign * x;
      for (int i=firstDegreeCode->getIndex(); i!=nn; i=i+sign)
	if (MidiMessage::isMidiNoteBlack(i))
	  absX--;
      return sign * absX;
    }
    else
      return x;
  }
  
  void processMIDIMessage(const Chord&, MidiMessage&);
  
public:
  void runArp(MidiBuffer&);
};
