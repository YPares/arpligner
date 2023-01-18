/*
  ==============================================================================

    Arp.cpp
    Created: 12 Jan 2023 11:46:17pm
    Author:  yves

  ==============================================================================
*/

#include "Arp.h"


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Arp();
}


void Arp::processMIDIMessage(const Chord& curChord, MidiMessage& msg) {
  int chan = msg.getChannel() - 1;
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

void Arp::runArp(MidiBuffer& midibuf) {
  auto behaviour = instanceBehaviour->getIndex();
  if (behaviour == InstanceBehaviour::BYPASS)
    return;
  
  Array<MidiMessage> messagesToProcess, messagesToPassthrough;
  ChordStore* chordStore;
  
  if (behaviour < InstanceBehaviour::IS_GLOBAL_CHORD_TRACK)
    // We read chords from MIDI channel indicated by instanceBehaviour and use
    // the local chord store
    chordStore = &mLocalChordStore;
  else
    // We read chords from the global chord store
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
	if (!(ignoreBlackKeysInPatterns->get() && MidiMessage::isMidiNoteBlack(msg.getNoteNumber())))
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
  else if (behaviour == InstanceBehaviour::IS_PATTERN_TRACK_DELAY_1_BUFFER) {
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
