#include "ChordStore.h"

void ChordStore::updateCurrentChord(WhenNoChordNote::Enum whenNoChordNoteVal,
                                    WhenSingleChordNote::Enum whenSingleChordNoteVal) {
  if (!mNeedsUpdate)
    return;
  
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

  mNeedsUpdate = false;
}

JUCE_IMPLEMENT_SINGLETON(GlobalChordStore);
