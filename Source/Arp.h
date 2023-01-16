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

#define IS_NOTE_MESSAGE(msg) (msg.isNoteOn() || msg.isNoteOff())

template<class CritSec = CriticalSection>
class ChordStore {
private:
  Counters mCounters;
  Chord mCurrentChord;
  bool mShouldProcess;
  bool mShouldSilence;
  CritSec mObjectLock;
  
public:
  ChordStore() : mShouldProcess(true), mShouldSilence(false) {
  }
  
  void addChordNote(NoteNumber nn) {
    const ScopedLock lock(mObjectLock);
    
    if(!mCounters.contains(nn))
      mCounters.set(nn, 1);
    else
      mCounters.set(nn, mCounters[nn]+1);
  }

  void rmChordNote(NoteNumber nn) {
    const ScopedLock lock(mObjectLock);
    
    if(mCounters.contains(nn))
      mCounters.set(nn, mCounters[nn]-1);
    if(mCounters[nn] <= 0)
      mCounters.remove(nn);
  }

  void updateCurrentChord(WhenNoChordNote::Enum whenNoChordNoteVal,
			  WhenSingleChordNote::Enum whenSingleChordNoteVal) {
    const ScopedLock lock(mObjectLock);

    mShouldSilence = false;
    mShouldProcess = true;
    
    Chord newChord;
    for (Counters::Iterator i(mCounters); i.next();) {
      newChord.add(i.getKey());
    }
    
    switch (newChord.size()) {
    case 0:
      // No chord notes
      switch (whenNoChordNoteVal) {
      case WhenNoChordNote::LATCH_LAST_CHORD:
	if (mCurrentChord.size() == 0)
	  // No last chord known. We silence
	  mShouldSilence = true;
	break;
      case WhenNoChordNote::USE_PATTERN_AS_NOTES:
	mShouldProcess = false;
	break;
      case WhenNoChordNote::SILENCE:
	mShouldSilence = true;
	break;
      }
      break;
      
    case 1:
      // Just 1 chord note
      switch (whenSingleChordNoteVal) {
      case WhenSingleChordNote::TRANSPOSE_LAST_CHORD:
	if (mCurrentChord.size() > 0) {
	  int offset = newChord[0] - mCurrentChord[0];
	  newChord.clear();
	  for (NoteNumber nn : mCurrentChord)
	    newChord.add(nn+offset);
	  mCurrentChord = newChord;
	}
	else // No last chord known. We silence
	  mShouldSilence = true;
	break;
      case WhenSingleChordNote::POWERCHORD:
	newChord.add(newChord[0] + 7);
	mCurrentChord = newChord;
	break;
      case WhenSingleChordNote::USE_AS_IS:
	mCurrentChord = newChord;
	break;
      case WhenSingleChordNote::USE_PATTERN_AS_NOTES:
	mShouldProcess = false;
	break;
      case WhenSingleChordNote::SILENCE:
	mShouldSilence = true;
	break;
      }
      break;

    default:
      // "Normal" case: 2 chords notes or more
      mCurrentChord = newChord;
      break;
    };
  }

  Chord getCurrentChord() {
    return mCurrentChord;
  }

  bool shouldProcess() {
    return mShouldProcess;
  }

  bool shouldSilence() {
    return mShouldSilence;
  }
};

// Used in a multi-instance configuration
class GlobalChordStore : public ChordStore<> {
public:
  ~GlobalChordStore() {
    clearSingletonInstance();
  }

  JUCE_DECLARE_SINGLETON(GlobalChordStore, false);
};


class Arp : public ArplignerAudioProcessor {
private:
  ChordStore<> localChordStore;
  
  // On each pattern chan, to which note is currently mapped each incoming NoteNumber
  HashMap<NoteNumber, NoteNumber> curMappings[16];

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
  
  void processMIDIMessage(const Chord& curChord, MidiMessage& msg) {
    Chan chan = msg.getChannel() - 1;
    NoteNumber noteCodeIn = msg.getNoteNumber();
    int numChordNotes = curChord.size();
    int degShift = getDegreeShift(noteCodeIn);
    if (msg.isNoteOn()) {
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
      msg.setNoteNumber(finalNote);
    }
    else if (msg.isNoteOff() && curMappings[chan].contains(noteCodeIn)) {
      msg.setNoteNumber(curMappings[chan][noteCodeIn]);
    }
  }
  
public:
  void runArp(MidiBuffer& midibuf) {
    auto behaviour = instanceBehaviour->getIndex();
    if (behaviour == InstanceBehaviour::BYPASS)
      return;
    
    Array<MidiMessage> messagesToProcess, messagesToPassthrough;
    ChordStore<>* chordStore;
    
    if (behaviour < InstanceBehaviour::IS_GLOBAL_CHORD_TRACK)
      // We read chords from MIDI channel indicated by instanceBehaviour and use
      // the local chord store
      chordStore = &localChordStore;
    else
      // We read chord from the global chord store
      chordStore = GlobalChordStore::getInstance();

    for (auto msgMD : midibuf) {
      auto msg = msgMD.getMessage();
      if (behaviour == InstanceBehaviour::IS_GLOBAL_CHORD_TRACK ||
	  behaviour < InstanceBehaviour::IS_GLOBAL_CHORD_TRACK &&
	  msg.getChannel() == behaviour) {
	if (msg.isNoteOn())
	  chordStore->addChordNote(msg.getNoteNumber());
	else if(msg.isNoteOff())
	  chordStore->rmChordNote(msg.getNoteNumber());
	if (chordNotesPassthrough->get() || !IS_NOTE_MESSAGE(msg))
	  messagesToPassthrough.add(msg);
      }
      else {
	if (IS_NOTE_MESSAGE(msg)) {
	  if (!(ignoreBlackKeysInPatterns->get() && isBlackKey(msg.getNoteNumber())))
	    messagesToProcess.add(msg);
	}
	else
	  messagesToPassthrough.add(msg);
      }
    }

    midibuf.clear();

    if (behaviour <= InstanceBehaviour::IS_GLOBAL_CHORD_TRACK)
      chordStore->updateCurrentChord
	((WhenNoChordNote::Enum)whenNoChordNote->getIndex(),
	 (WhenSingleChordNote::Enum)whenSingleChordNote->getIndex());

    if (chordStore->shouldSilence())
      messagesToProcess.removeIf ([](auto& msg){
	return !msg.isNoteOff();
      });
    
    // Pass non-processable messages through:
    for (auto msg : messagesToPassthrough)
      midibuf.addEvent(msg, 0);

    // Process and add processable messages:
    for (auto msg : messagesToProcess) {
      if (chordStore->shouldProcess())
	processMIDIMessage(chordStore->getCurrentChord(), msg);
      midibuf.addEvent(msg, 0);
    }
  }
};
