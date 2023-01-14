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

using namespace juce;


using NoteNumber = int;
using Counters = HashMap<NoteNumber, int>;
using Chan = int;


class Arp : public ArplignerJuceAudioProcessor
{
private:
  // #note on - #note off for each note on the chord chan
  Counters counters;

  // On each pattern chan, to which note is currently mapped each incoming NoteNumber
  HashMap<Chan, NoteNumber> curMappings[16];

  void addChordNote(NoteNumber n) {
    if(!counters.contains(n))
      counters.set(n, 1);
    else
      counters.set(n, counters[n]+1);
  }

  void rmChordNote(NoteNumber n) {
    if(counters.contains(n))
      counters.set(n, counters[n]-1);
    if(counters[n] <= 0)
      counters.remove(n);
  }

  SortedSet<NoteNumber> getCurChord() {
    SortedSet<NoteNumber> s;
    for (Counters::Iterator i(counters); i.next();) {
      s.add(i.getKey());
    }
    return s;
  }

  void transformEvent(const SortedSet<NoteNumber>& curChord, MidiMessage& ev) {
  }

public:
  Arp() : ArplignerJuceAudioProcessor() {
  }
  
  void runArp(MidiBuffer& midibuf) {
    DBG("Chord chan: " << chordChan->get());

    Array<MidiMessage> eventsToProcess, otherEvents;
    
    for (auto msgMD : midibuf) {
      auto msg = msgMD.getMessage();
      if (msg.getChannel() == chordChan->get()) {
	if (msg.isNoteOn())
	  addChordNote(msg.getNoteNumber());
	else if(msg.isNoteOff())
	  rmChordNote(msg.getNoteNumber());
      }
      else {
	eventsToProcess.add(msg);
      }
      DBG(msg.getNoteNumber());
    }
    midibuf.clear();

    auto curChord = getCurChord();
    for (auto n : curChord) {
      DBG("Chord note: " << n);
    }
    auto numChordNotes = curChord.size();
    if (numChordNotes >= 2) {
    }
    else if (numChordNotes == 1) {
    }
    else {
    }

    for (auto ev : otherEvents)
      midibuf.addEvent(ev, 0);
    for (auto ev : eventsToProcess) {
      transformEvent(curChord, ev);
      midibuf.addEvent(ev, 0);
    }
  }
};
