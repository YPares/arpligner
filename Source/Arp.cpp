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


bool Arp::shouldDiscardPatternNote(NoteNumber nn) {
  switch (patternNotesMapping->getIndex()) {
  case PatternNotesMapping::WHITE_NOTES_TO_DEGREES:
    return MidiMessage::isMidiNoteBlack(nn);
  default:
    return false;
  }
}

int Arp::getDegreeNumber(NoteNumber nn) {
  int offset = nn - firstDegreeCode->getIndex();
  switch (patternNotesMapping->getIndex()) {
  case PatternNotesMapping::WHITE_NOTES_TO_DEGREES: {
    // We need to correct the [firstDegreeCode,nn] interval for the amount
    // of black keys it contains:
    int sign = (offset < 0) ? -1 : 1;
    int absOffset = sign * offset;
    for (int i=firstDegreeCode->getIndex(); i!=nn; i=i+sign)
      if (MidiMessage::isMidiNoteBlack(i))
	absOffset--;
    return sign * absOffset;
    }
  default:
    return offset;
  }
}

void Arp::processMIDIMessage(const Chord& curChord, MidiMessage& msg) {
  int chan = msg.getChannel() - 1;
  NoteNumber noteCodeIn = msg.getNoteNumber();
  if (msg.isNoteOn()) {
    int numChordNotes = curChord.size();
    int degreeNum = getDegreeNumber(noteCodeIn);
    int wantedDegree;
    if (degreeNum >= 0)
	wantedDegree = degreeNum % numChordNotes;
    else {
      int absRem = (-degreeNum) % numChordNotes;
      wantedDegree = (absRem == 0) ? 0 : numChordNotes - absRem;
    }
    int wantedOctaveShift = floor((float)degreeNum / (float)numChordNotes);
    NoteNumber finalNote = curChord[wantedDegree] + 12*wantedOctaveShift;
    curMappings[chan].set(noteCodeIn, finalNote);
    msg.setNoteNumber(finalNote);
  }
  else if (msg.isNoteOff() && curMappings[chan].contains(noteCodeIn)) {
    msg.setNoteNumber(curMappings[chan][noteCodeIn]);
  }
}

void Arp::runArp(MidiBuffer& midibuf) {
  auto behaviour = instanceBehaviour->getIndex();
  if (behaviour == InstanceBehaviour::BYPASS)
    return;
  
  Array<MidiMessage> messagesToProcess, messagesToPassthrough;
  ChordStore* chordStore;
  
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
      if (chordNotesPassthrough->get() || !IS_NOTE_MESSAGE(msg))
	messagesToPassthrough.add(msg);
    }
    else {
      if (IS_NOTE_MESSAGE(msg)) {
	if (!shouldDiscardPatternNote(msg.getNoteNumber()))
	  messagesToProcess.add(msg);
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
