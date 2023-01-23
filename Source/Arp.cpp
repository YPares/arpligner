/*
  ==============================================================================

    Arp.cpp
    Created: 12 Jan 2023 11:46:17pm
    Author:  yves

  ==============================================================================
*/

#include "Arp.h"


AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Arp();
}


// Pure functions that compute the mappings of input pattern notes:
namespace Mapping {
  
bool patternNoteIsMapped(PatternNotesMapping::Enum mappingMode, NoteNumber nn) {
  switch (mappingMode) {
  case PatternNotesMapping::MAP_NOTHING:
    return false;
  case PatternNotesMapping::WHITE_NOTE_TO_DEGREE:
    return !MidiMessage::isMidiNoteBlack(nn);
  default:
    return true;
  }
}

NoteNumber mapToChordDegree(const Chord& curChord, int degreeNum) {
  int numChordNotes = curChord.size();
  int wantedDegree;
  if (degreeNum >= 0)
    wantedDegree = degreeNum % numChordNotes;
  else {
    int absRem = (-degreeNum) % numChordNotes;
    wantedDegree = (absRem == 0) ? 0 : numChordNotes - absRem;
  }
  int wantedOctaveShift = floor((float)degreeNum / (float)numChordNotes);
  return curChord[wantedDegree] + 12*wantedOctaveShift;
}

NoteNumber mapPatternNote(NoteNumber referenceNote,
			  PatternNotesMapping::Enum mappingMode,
			  const Chord& curChord,
			  NoteNumber noteCodeIn) {
  int offsetFromRef = noteCodeIn - referenceNote;
  switch (mappingMode) {
  case PatternNotesMapping::TRANSPOSE_FROM_FIRST_DEGREE:
    return curChord[0] + offsetFromRef;
  case PatternNotesMapping::WHITE_NOTE_TO_DEGREE: {
    // We need to correct the [referenceNote,noteCodeIn] interval for the amount
    // of black keys it contains:
    int sign = (offsetFromRef < 0) ? -1 : 1;
    int absOffset = sign * offsetFromRef;
    for (int i=referenceNote; i!=noteCodeIn; i+=sign)
      if (MidiMessage::isMidiNoteBlack(i))
	absOffset--;
    return mapToChordDegree(curChord, sign * absOffset);
    }
  default:
    return mapToChordDegree(curChord, offsetFromRef);
  }
}

} // end namespace Mapping

// If this is called, we are either in Multi-channel mode, or a Pattern instance
// in Multi-instance mode:
void Arp::nonGlobalChordInstanceWork(MidiBuffer& midibuf, InstanceBehaviour::Enum behaviour) {
  Array<MidiMessage> noteOnsToProcess, noteOffsToProcess, messagesToPassthrough;
  ChordStore* chordStore = &mLocalChordStore;
  
  // We get some parameter values here to make sure they will remain coherent
  // throughout the processing of the whole buffer:
  auto mappingMode = (PatternNotesMapping::Enum)patternNotesMapping->getIndex();
  auto referenceNote = firstDegreeCode->getIndex();
  auto chordPassthrough = chordNotesPassthrough->get();
  auto unmappedPassthrough = unmappedPatternNotesPassthrough->get();
  
  if (behaviour >= InstanceBehaviour::IS_PATTERN)
    // We are a Pattern instance. We read chords from the global chord store
    chordStore = GlobalChordStore::getInstance();

  for (auto msgMD : midibuf) {
    auto msg = msgMD.getMessage();
    if (behaviour < InstanceBehaviour::IS_PATTERN &&
	msg.getChannel() == behaviour) {
      // We are a Multi-channel instance. We should process chords landing on
      // our chord channel:
      toChordStore(chordStore, msg);
      if (chordPassthrough || !IS_NOTE_MESSAGE(msg))
	messagesToPassthrough.add(msg);
    }
    else {
      if (IS_NOTE_MESSAGE(msg)) {
	if (Mapping::patternNoteIsMapped(mappingMode, msg.getNoteNumber())) {
	  if (msg.isNoteOn())
	    noteOnsToProcess.add(msg);
	  else
	    noteOffsToProcess.add(msg);
	}
	else if (unmappedPassthrough)
	  messagesToPassthrough.add(msg);
      }
      else
	messagesToPassthrough.add(msg);
    }
  }

  midibuf.clear();
  
  if (behaviour < InstanceBehaviour::IS_PATTERN)
    updateChordStore(chordStore);
  else if (behaviour == InstanceBehaviour::IS_PATTERN_1_BUFFER_DELAY) {
    noteOnsToProcess.swapWith(mLastBufferNoteOnsToProcess);
  }

  // Pass non-processable messages through:
  for (auto msg : messagesToPassthrough)
    midibuf.addEvent(msg, 0);

  auto shouldSilence = chordStore->shouldSilence();
  auto shouldProcess = chordStore->shouldProcess();
  auto curChord = chordStore->getCurrentChord();  
  
  if (shouldSilence)
    noteOnsToProcess.clear();
  
  // Process and add processable messages:
  for (auto msg : noteOffsToProcess) { // Note OFFs first
    int chan = msg.getChannel() - 1;
    NoteNumber noteCodeIn = msg.getNoteNumber();
    if (mCurMappings[chan].contains(noteCodeIn)) {
      msg.setNoteNumber(mCurMappings[chan][noteCodeIn]);
      mCurMappings[chan].remove(noteCodeIn);
    }
    midibuf.addEvent(msg,0);
  }
  for (auto msg : noteOnsToProcess) { // Then note ONs
    if (shouldProcess) {
      int chan = msg.getChannel() - 1;
      NoteNumber noteCodeIn = msg.getNoteNumber();
      NoteNumber noteCodeOut = Mapping::mapPatternNote(referenceNote,
						       mappingMode,
						       curChord,
						       noteCodeIn);
      msg.setNoteNumber(noteCodeOut);
      mCurMappings[chan].set(noteCodeIn, noteCodeOut);
    }
    midibuf.addEvent(msg, 0);
  }
}
