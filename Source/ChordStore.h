/*
  ==============================================================================

    ChordStore.h
    Created: 16 Jan 2023 08:00:00pm
    Author:  Yves Parès

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

using namespace juce;

using NoteNumber = int;
using Chord = SortedSet<NoteNumber>;
using Counters = HashMap<NoteNumber, int>;


// A thread-safe way to keep track of the currently playing chord
class ChordStore {
private:
  Counters mCounters;
  Chord mCurrentChord;
  bool mShouldProcess;
  bool mShouldSilence;
  CriticalSection mObjectLock;
  
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

  void updateCurrentChord(WhenNoChordNote::Enum, WhenSingleChordNote::Enum);
  
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

// A JUCE singleton ChordStore. Used in a multi-instance configuration
class GlobalChordStore : public ChordStore {
public:
  ~GlobalChordStore() {
    clearSingletonInstance();
  }

  JUCE_DECLARE_SINGLETON(GlobalChordStore, false);
};
