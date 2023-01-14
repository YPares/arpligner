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
using Chord = SortedSet<NoteNumber>;

#define IS_NOTE_EVENT(msg) (msg.isNoteOn() || msg.isNoteOff())

class Arp : public ArplignerJuceAudioProcessor
{
private:
  // #note on - #note off for each note on the chord chan
  Counters counters;
  Chord lastChord;

  // On each pattern chan, to which note is currently mapped each incoming NoteNumber
  HashMap<NoteNumber, NoteNumber> curMappings[16];

  void addChordNote(NoteNumber nn) {
    if(!counters.contains(nn))
      counters.set(nn, 1);
    else
      counters.set(nn, counters[nn]+1);
  }

  void rmChordNote(NoteNumber nn) {
    if(counters.contains(nn))
      counters.set(nn, counters[nn]-1);
    if(counters[nn] <= 0)
      counters.remove(nn);
  }

  Chord getCurChord() {
    Chord s;
    for (Counters::Iterator i(counters); i.next();) {
      s.add(i.getKey());
    }
    return s;
  }

  bool isBlackKey(NoteNumber nn) {
    int r = nn % 12;
    return r == 1 || r == 3 || r == 6 || r == 8 || r == 10;
  }

  int getDegreeShift(NoteNumber nn) {
    int x = nn - firstDegreeCode->get();
    if (ignoreBlackKeysInPatterns->get()) {
      // We need to correct the [firstDegreeCode,nn] interval for the amount
      // of black keys it contains:
      int sign = (x < 0) ? -1 : 1;
      int absX = sign * x;
      for (int i=firstDegreeCode->get(); i!=nn; i=i+sign)
	if (isBlackKey(i))
	  absX--;
      return sign * absX;
    }
    else
      return x;
  }
  
  void transformEvent(const Chord& curChord, MidiMessage& ev) {
    Chan chan = ev.getChannel() - 1;
    NoteNumber noteCodeIn = ev.getNoteNumber();
    int numChordNotes = curChord.size();
    int degShift = getDegreeShift(noteCodeIn);
    if (ev.isNoteOn()) {
      int wantedDegree;
      if (degShift >= 0)
	wantedDegree = degShift % numChordNotes;
      else {
	int x = (-degShift) % numChordNotes;
        wantedDegree = (x==0) ? 0 : numChordNotes - x;
      }
      int wantedOctaveShift = floor((float)degShift / (float)numChordNotes);
      NoteNumber finalNote = curChord[wantedDegree] + 12*wantedOctaveShift;
      curMappings[chan].set(noteCodeIn, finalNote);
      ev.setNoteNumber(finalNote);
    }
    else if (ev.isNoteOff() && curMappings[chan].contains(noteCodeIn)) {
      ev.setNoteNumber(curMappings[chan][noteCodeIn]);
    }
  }

  void keepOnlyNoteOffs(Array<MidiMessage>& arr) {
    arr.removeIf ([](const MidiMessage& msg){
      return !msg.isNoteOff();
    });
  }
  
public:
  void runArp(MidiBuffer& midibuf) {
    Array<MidiMessage> eventsToProcess, eventsToPassthrough;
    
    for (auto msgMD : midibuf) {
      auto msg = msgMD.getMessage();
      if (msg.getChannel() == chordChan->get()) {
	if (msg.isNoteOn())
	  addChordNote(msg.getNoteNumber());
	else if(msg.isNoteOff())
	  rmChordNote(msg.getNoteNumber());
	if (chordNotesPassthrough->get() || !IS_NOTE_EVENT(msg))
	  eventsToPassthrough.add(msg);
      }
      else {
	if (IS_NOTE_EVENT(msg)) {
	  if (!(ignoreBlackKeysInPatterns->get() && isBlackKey(msg.getNoteNumber())))
	    eventsToProcess.add(msg);
	}
	else
	  eventsToPassthrough.add(msg);
      }
    }

    midibuf.clear();

    auto curChord = getCurChord();
    auto doProcess = true;
    DBG("Cur chord size: " << curChord.size());
    
    switch (curChord.size()) {
    case 0:
      // No chord notes
      switch (whenNoChordNote->getIndex()) {
      case WhenNoChordNote::LATCH_LAST_CHORD:
	if (lastChord.size() > 0)
	  curChord = lastChord; 
	else // No last chord known. We silence
	  keepOnlyNoteOffs(eventsToProcess);
	break;
      case WhenNoChordNote::USE_PATTERN_AS_NOTES:
	doProcess = false;
	break;
      case WhenNoChordNote::SILENCE:
	keepOnlyNoteOffs(eventsToProcess);
	break;
      }
      break;
      
    case 1:
      // Just 1 chord note
      switch (whenSingleChordNote->getIndex()) {
      case WhenSingleChordNote::TRANSPOSE_LAST_CHORD:
	if (lastChord.size() > 0) {
	  int offset = curChord[0] - lastChord[0];
	  curChord.clear();
	  for (NoteNumber nn : lastChord)
	    curChord.add(nn+offset);
	  lastChord = curChord;
	}
	else // No last chord known. We silence
	  keepOnlyNoteOffs(eventsToProcess);
	break;
      case WhenSingleChordNote::POWERCHORD:
	curChord.add(curChord[0] + 7);
	lastChord = curChord;
	break;
      case WhenSingleChordNote::USE_AS_IS:
	lastChord = curChord;
	break;
      case WhenSingleChordNote::USE_PATTERN_AS_NOTES:
	doProcess = false;
	break;
      case WhenSingleChordNote::SILENCE:
	keepOnlyNoteOffs(eventsToProcess);
	break;
      }
      break;

    default:
      // "Normal" case: 2 chords notes or more
      lastChord = curChord;
      break;
    };

    // Pass non-processable events through:
    for (auto ev : eventsToPassthrough)
      midibuf.addEvent(ev, 0);

    // Process and add processable events:
    for (auto ev : eventsToProcess) {
      if (doProcess)
	transformEvent(curChord, ev);
      midibuf.addEvent(ev, 0);
    }
  }
};
