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

  
void Arp::runArp(MidiBuffer& midibuf) {
  auto behaviour = instanceBehaviour->getIndex();
  if (behaviour == InstanceBehaviour::BYPASS)
    return;
  
  Array<MidiMessage> messagesToProcess, messagesToPassthrough;
  ChordStore* chordStore;
  // We get some parameter values here to make sure they will remain coherent
  // throughout the processing of the whole buffer:
  auto mappingMode = (PatternNotesMapping::Enum)patternNotesMapping->getIndex();
  auto referenceNote = firstDegreeCode->getIndex();
  auto chordPassthrough = chordNotesPassthrough->get();
  auto unmappedPassthrough = unmappedPatternNotesPassthrough->get();
  
  if (behaviour < InstanceBehaviour::IS_CHORD)
    // We read chords from MIDI channel indicated by instanceBehaviour and use
    // the local chord store
    chordStore = &mLocalChordStore;
  else
    // We read chords from the global chord store
    chordStore = GlobalChordStore::getInstance();

  for (auto msgMD : midibuf) {
    auto msg = msgMD.getMessage();
    if (behaviour == InstanceBehaviour::IS_CHORD ||
	behaviour < InstanceBehaviour::IS_CHORD &&
	msg.getChannel() == behaviour) {
      if (msg.isNoteOn())
	chordStore->addChordNote(msg.getNoteNumber());
      else if(msg.isNoteOff())
	chordStore->rmChordNote(msg.getNoteNumber());
      if (chordPassthrough || !IS_NOTE_MESSAGE(msg))
	messagesToPassthrough.add(msg);
    }
    else {
      if (IS_NOTE_MESSAGE(msg)) {
	if (Mapping::patternNoteIsMapped(mappingMode, msg.getNoteNumber()))
	  messagesToProcess.add(msg);
	else if (unmappedPassthrough)
	  messagesToPassthrough.add(msg);
      }
      else
	messagesToPassthrough.add(msg);
    }
  }

  midibuf.clear();
  
  if (behaviour <= InstanceBehaviour::IS_CHORD)
    chordStore->updateCurrentChord
      ((WhenNoChordNote::Enum)whenNoChordNote->getIndex(),
       (WhenSingleChordNote::Enum)whenSingleChordNote->getIndex());
  else if (behaviour == InstanceBehaviour::IS_PATTERN_1_BUFFER_DELAY) {
    messagesToProcess.swapWith(mLastBufferMessagesToProcess);
  }

  auto shouldSilence = chordStore->shouldSilence();
  auto shouldProcess = chordStore->shouldProcess();
  auto curChord = chordStore->getCurrentChord();  
  
  if (shouldSilence)
    messagesToProcess.removeIf ([](auto& msg){
      return !msg.isNoteOff();
    });
  
  // Pass non-processable messages through:
  for (auto msg : messagesToPassthrough)
    midibuf.addEvent(msg, 0);

  // Process and add processable messages:
  for (auto msg : messagesToProcess) {
    if (shouldProcess) {
      int chan = msg.getChannel() - 1;
      NoteNumber noteCodeIn = msg.getNoteNumber();
      if (msg.isNoteOn()) {
	NoteNumber noteCodeOut = Mapping::mapPatternNote(referenceNote,
							 mappingMode,
							 curChord,
							 noteCodeIn);
	msg.setNoteNumber(noteCodeOut);
	curMappings[chan].set(noteCodeIn, noteCodeOut);
      }
      else if (msg.isNoteOff() && curMappings[chan].contains(noteCodeIn)) {
	msg.setNoteNumber(curMappings[chan][noteCodeIn]);
      }      
    }
    midibuf.addEvent(msg, 0);
  }
}
