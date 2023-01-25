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
  bool mNeedsUpdate;
  
public:
  ChordStore() : mShouldProcess(true), mShouldSilence(false), mNeedsUpdate(false) {
  }
  
  void addChordNote(NoteNumber nn) {
    mNeedsUpdate = true;
    if(!mCounters.contains(nn))
      mCounters.set(nn, 1);
    else
      mCounters.set(nn, mCounters[nn]+1);
  }

  void rmChordNote(NoteNumber nn) {
    mNeedsUpdate = true;
    if(mCounters.contains(nn))
      mCounters.set(nn, mCounters[nn]-1);
    if(mCounters[nn] <= 0)
      mCounters.remove(nn);
  }

  void updateCurrentChord(WhenNoChordNote::Enum, WhenSingleChordNote::Enum);

  virtual void flushCurrentChord() {
    mCounters.clear();
    mCurrentChord.clear();
    mShouldProcess = true;
    mShouldSilence = false;
    mNeedsUpdate = false;
  }
  
  virtual void getCurrentChord(Chord& chord, bool& shouldProcess, bool& shouldSilence) {
    chord = mCurrentChord;
    shouldProcess = mShouldProcess;
    shouldSilence = mShouldSilence;
  }
};

// A JUCE singleton ChordStore. Used in a multi-instance configuration
class GlobalChordStore : public ChordStore {
public:
  ReadWriteLock globalStoreLock;
  
  ~GlobalChordStore() {
    clearSingletonInstance();
  }

  void flushCurrentChord() override {
    const ScopedWriteLock lock(globalStoreLock);
    ChordStore::flushCurrentChord();
  }

  void getCurrentChord(Chord& chord, bool& shouldProcess, bool& shouldSilence) override {
    // This will prevent a Pattern instance to access the current chord if the
    // Global chord instance is still updating it
    const ScopedReadLock lock(globalStoreLock);
    ChordStore::getCurrentChord(chord, shouldProcess, shouldSilence);
  }

  JUCE_DECLARE_SINGLETON(GlobalChordStore, false);
};
