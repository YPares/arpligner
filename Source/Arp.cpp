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


// Functions that compute the mappings of input pattern notes:
namespace Mapping {

void mapToChordDegree(PatternNotesWraparound::Enum wrapMode,
		      const Chord& curChord,
		      int degreeNum,
		      NoteNumber& noteCodeOut,
		      bool& isMapped) {
  int numChordDegrees = curChord.size();

  if ((degreeNum < 0 || degreeNum >= numChordDegrees) &&
      wrapMode == PatternNotesWraparound::NO_WRAPAROUND) {
    isMapped = false;
    return;
  }

  int numValidDegrees =
    (wrapMode <= PatternNotesWraparound::AFTER_ALL_CHORD_DEGREES)
    ? numChordDegrees
    : wrapMode;
  int wantedDegree;
  if (degreeNum >= 0)
    wantedDegree = degreeNum % numValidDegrees;
  else {
    int absRem = (-degreeNum) % numValidDegrees;
    wantedDegree = (absRem == 0) ? 0 : numValidDegrees - absRem;
  }
  int wantedOctaveShift = floor((float)degreeNum / (float)numValidDegrees);
  
  if (wantedDegree >= numChordDegrees)
    isMapped = false;
  else
    noteCodeOut = curChord[wantedDegree] + 12*wantedOctaveShift;
}

void mapPatternNote(NoteNumber referenceNote,
		    PatternNotesMapping::Enum mappingMode,
		    PatternNotesWraparound::Enum wrapMode,
		    const Chord& curChord,
		    NoteNumber noteCodeIn,
		    NoteNumber& noteCodeOut,
		    bool& isMapped) {
  isMapped = true;
  int offsetFromRef = noteCodeIn - referenceNote;
  
  switch (mappingMode) {
  case PatternNotesMapping::MAP_NOTHING:
    isMapped = false;
    break;
  case PatternNotesMapping::TRANSPOSE_FROM_FIRST_DEGREE:
    noteCodeOut = curChord[0] + offsetFromRef;
    break;
  case PatternNotesMapping::WHITE_NOTE_TO_DEGREE:
    isMapped = !MidiMessage::isMidiNoteBlack(noteCodeIn);
    if (isMapped) {
      // We need to correct the [referenceNote,noteCodeIn] interval for the amount
      // of black keys it contains:
      int sign = (offsetFromRef < 0) ? -1 : 1;
      int absOffset = sign * offsetFromRef;
      for (int i=referenceNote; i!=noteCodeIn; i+=sign)
	if (MidiMessage::isMidiNoteBlack(i))
	  absOffset--;
      mapToChordDegree(wrapMode, curChord, sign * absOffset, noteCodeOut, isMapped);
    }
    break;
  default:
    mapToChordDegree(wrapMode, curChord, offsetFromRef, noteCodeOut, isMapped);
    break;
  }
}

} // end namespace Mapping

// If this is called, we are either in Multi-channel mode, or a Pattern instance
// in Multi-instance mode:
void Arp::patternOrSingleInstanceWork(MidiBuffer& midibuf, InstanceBehaviour::Enum behaviour) {
  Array<MidiMessage> noteOnsToProcess, noteOffsToProcess, messagesToPassthrough;
  ChordStore* chordStore = &mLocalChordStore;
  
  // We get some parameter values here to make sure they will remain coherent
  // throughout the processing of the whole buffer:
  auto mappingMode = (PatternNotesMapping::Enum)patternNotesMapping->getIndex();
  auto wrapMode = (PatternNotesWraparound::Enum)patternNotesWraparound->getIndex();
  auto referenceNote = firstDegreeCode->getIndex();
  
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
      messagesToPassthrough.add(msg);
    }
    else if (msg.isNoteOn())
      noteOnsToProcess.add(msg);
    else if (msg.isNoteOff())
      noteOffsToProcess.add(msg);
    else
      messagesToPassthrough.add(msg);
  }

  midibuf.clear();
  
  if (behaviour < InstanceBehaviour::IS_PATTERN)
    // We are a Multi-channel instance, we update our chord store:
    updateChordStore(chordStore);

  // Pass non-processable messages through:
  for (auto msg : messagesToPassthrough)
    midibuf.addEvent(msg, 0);

  Chord curChord;
  bool shouldProcess, shouldSilence;
  chordStore->getCurrentChord(curChord, shouldProcess, shouldSilence);
  
  if (shouldSilence)
    noteOnsToProcess.clear();
  
  // Process and add processable messages:
  for (auto msg : noteOffsToProcess) { // Note OFFs first
    int chan = msg.getChannel() - 1;
    NoteNumber noteCodeIn = msg.getNoteNumber();
    NoteNumber noteCodeOut = mCurMappings[chan][noteCodeIn];
    if (noteCodeOut != ~0)
      msg.setNoteNumber(noteCodeOut);
    midibuf.addEvent(msg, 0);
  }
  for (auto msg : noteOnsToProcess) { // Then note ONs
    bool isMapped = true;
    int chan = msg.getChannel() - 1;
    NoteNumber noteCodeIn = msg.getNoteNumber();
    NoteNumber noteCodeOut = noteCodeIn;
    if (shouldProcess)
      Mapping::mapPatternNote(referenceNote,
			      mappingMode,
			      wrapMode,
			      curChord,
			      noteCodeIn,
			      noteCodeOut,
			      isMapped);
    mCurMappings[chan][noteCodeIn] = noteCodeOut;
    if (isMapped) {
      msg.setNoteNumber(noteCodeOut);
      midibuf.addEvent(msg, 0);
    }
  }
}
