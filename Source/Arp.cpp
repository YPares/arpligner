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
    Array<NoteNumber>& thisNoteMappings) {
    int numChordDegrees = curChord.size();

    if ((degreeNum < 0 || degreeNum >= numChordDegrees) &&
      wrapMode == PatternNotesWraparound::NO_WRAPAROUND ||
      numChordDegrees == 0)
      return;

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

    if (wantedDegree < numChordDegrees)
      thisNoteMappings.add(curChord[wantedDegree] + 12 * wantedOctaveShift);
  }

  void mapPatternNote(NoteNumber referenceNote,
    PatternNotesMapping::Enum mappingMode,
    PatternNotesWraparound::Enum wrapMode,
    UnmappedNotesBehaviour::Enum unmappedBeh,
    const Chord& curChord,
    NoteNumber noteCodeIn,
    Array<NoteNumber>& thisNoteMappings) {
    int offsetFromRef = noteCodeIn - referenceNote;

    switch (mappingMode) {
    case PatternNotesMapping::ALWAYS_LEAVE_UNMAPPED:
      break;
    case PatternNotesMapping::WHITE_NOTE_TO_DEGREE:
      if (!MidiMessage::isMidiNoteBlack(noteCodeIn)) {
        // We need to correct the [referenceNote,noteCodeIn] interval for the amount
        // of black keys it contains:
        int sign = (offsetFromRef < 0) ? -1 : 1;
        int absOffset = sign * offsetFromRef;
        for (int i = referenceNote; i != noteCodeIn; i += sign)
          if (MidiMessage::isMidiNoteBlack(i))
            absOffset--;
        mapToChordDegree(wrapMode, curChord, sign * absOffset, thisNoteMappings);
      }
      break;
    default:
      mapToChordDegree(wrapMode, curChord, offsetFromRef, thisNoteMappings);
      break;
    }

    if (thisNoteMappings.size() == 0) { // If the note has not been mapped yet:
      switch (unmappedBeh) {
      case UnmappedNotesBehaviour::SILENCE:
        break;
      case UnmappedNotesBehaviour::PLAY_FULL_CHORD_UP_TO_NOTE:
        for (NoteNumber chdNote : curChord) {
          if (chdNote <= noteCodeIn)
            thisNoteMappings.add(chdNote);
        }
        break;
      case UnmappedNotesBehaviour::TRANSPOSE_FROM_FIRST_DEGREE:
        thisNoteMappings.add(curChord[0] + offsetFromRef);
        break;
      case UnmappedNotesBehaviour::USE_AS_IS:
        thisNoteMappings.add(noteCodeIn);
        break;
      }
    }
  }

  NoteOnChan getNoteOnChan(const MidiMessage& msg) {
    return msg.getNoteNumber() | ((msg.getChannel() - 1) << 8);
  }

} // end namespace Mapping


void Arp::prepareToPlay(double sampleRate, int samplesPerBlock) {
  auto behaviour = (InstanceBehaviour::Enum)instanceBehaviour->getIndex();

  int latency = 0;
  if (behaviour == InstanceBehaviour::IS_CHORD) {
    int wanted = numMillisecsOfLatency->get();
    latency = (samplesPerBlock * sampleRate * wanted) / 512000;
  }
  setLatencySamples(latency);

  if (behaviour != InstanceBehaviour::IS_PATTERN)
    getChordStore(behaviour)->flushCurrentChord();
}

void Arp::runArp(MidiBuffer& midibuf) {
  auto behaviour = (InstanceBehaviour::Enum)instanceBehaviour->getIndex();

  if (behaviour == InstanceBehaviour::BYPASS || midibuf.isEmpty())
    return;
  else if (behaviour == InstanceBehaviour::IS_CHORD) {
    /* We special-case the global chord instance behaviour so
       it can update the current chord as fast as possible
       (without having to sort events in the buffer first),
       and so we can lock just once for the whole midi buffer: */
    auto* chd = GlobalChordStore::getInstance();
    ScopedWriteLock l(chd->globalStoreLock);
    for (auto msgMD : midibuf) {
      auto msg = msgMD.getMessage();
      if (msg.isNoteOn())
        chd->addChordNote(msg.getNoteNumber());
      if (msg.isNoteOff())
        chd->rmChordNote(msg.getNoteNumber());
    }
    updateChordStore(chd);
    return;
  }

  ArraySP<NoteNumber> chordNoteOns, chordNoteOffs;
  ArraySP<MidiMessage> ptrnNoteOns, ptrnNoteOffs, ptrnEventsToRemap, otherMsgs;

  for (auto msgMD : midibuf) {
    auto msg = msgMD.getMessage();
    if (msg.isNoteOn()) {
      if (behaviour == msg.getChannel())
        chordNoteOns.add({ msgMD.samplePosition, msg.getNoteNumber() });
      else
        ptrnNoteOns.add({ msgMD.samplePosition, msg });
    }
    else if (msg.isNoteOff()) {
      if (behaviour == msg.getChannel())
        chordNoteOffs.add({ msgMD.samplePosition, msg.getNoteNumber() });
      else
        ptrnNoteOffs.add({ msgMD.samplePosition, msg });
    }
    else if (behaviour != msg.getChannel() &&
      msg.isAftertouch()) {
      ptrnEventsToRemap.add({ msgMD.samplePosition, msg });
    }
    else
      otherMsgs.add({ msgMD.samplePosition, msg });
  }

  midibuf.clear();

  for (auto& msg : otherMsgs)
    midibuf.addEvent(msg.item, msg.samplePosition);

  ChordStore* chd = getChordStore(behaviour);

  if (behaviour != InstanceBehaviour::IS_PATTERN) {
    for (auto& n : chordNoteOns)
      chd->addChordNote(n.item);
    for (auto& n : chordNoteOffs)
      chd->rmChordNote(n.item);
    updateChordStore(chd);
  }

  processPatternNotes(chd, ptrnNoteOns, ptrnNoteOffs, ptrnEventsToRemap, midibuf);
}

Array<NoteNumber>& Arp::remapEvent(const WithSP<MidiMessage>& msg, MidiBuffer& midibuf) {
  Array<NoteNumber>& thisNoteMappings =
    mCurMappings.getReference(Mapping::getNoteOnChan(msg.item));
  for (NoteNumber nn : thisNoteMappings) {
    MidiMessage newMsg(msg.item);
    newMsg.setNoteNumber(nn);
    midibuf.addEvent(newMsg, msg.samplePosition);
  }
  return thisNoteMappings;
}

void Arp::processPatternNotes(ChordStore* chd, ArraySP<MidiMessage>& noteOns, ArraySP<MidiMessage>& noteOffs, ArraySP<MidiMessage>& eventsToRemap, MidiBuffer& midibuf) {
  auto mappingMode = (PatternNotesMapping::Enum)patternNotesMapping->getIndex();
  auto wrapMode = (PatternNotesWraparound::Enum)patternNotesWraparound->getIndex();
  auto unmappedBeh = (UnmappedNotesBehaviour::Enum)unmappedNotesBehaviour->getIndex();
  auto referenceNote = firstDegreeCode->getIndex();

  Chord curChord;
  bool shouldProcess, shouldSilence;
  chd->getCurrentChord(curChord, shouldProcess, shouldSilence);

  if (shouldSilence)
    noteOns.clear();

  // Process and add processable messages:

  for (auto& msg : noteOffs) { // Note OFFs first
    Array<NoteNumber>& thisNoteMappings =
      mCurMappings.getReference(Mapping::getNoteOnChan(msg.item));
    for (NoteNumber nn : thisNoteMappings) {
      MidiMessage newMsg(msg.item);
      newMsg.setNoteNumber(nn);
      midibuf.addEvent(newMsg, msg.samplePosition);
    }
    thisNoteMappings.clear();
  }

  for (auto& msg : noteOns) { // Then note ONs
    NoteNumber noteCodeIn = msg.item.getNoteNumber();
    // This should create a default (empty) array if the note has not
    // been mapped yet:
    Array<NoteNumber>& thisNoteMappings =
      mCurMappings.getReference(Mapping::getNoteOnChan(msg.item));
    // If we already have mappings for this note, it means we received 2+ NOTE ONs
    // in a row for it and no NOTE OFF, so first we off those mappings:
    for (NoteNumber nn : thisNoteMappings)
      midibuf.addEvent(MidiMessage::noteOff(msg.item.getChannel(), nn), msg.samplePosition);
    thisNoteMappings.clear();

    if (shouldProcess) // The ChordStore tells us to process
      Mapping::mapPatternNote(referenceNote,
        mappingMode,
        wrapMode,
        unmappedBeh,
        curChord,
        noteCodeIn,
        thisNoteMappings);
    else // We map the note to itself
      thisNoteMappings.add(noteCodeIn);

    // We send NOTE ONs for all newly mapped notes:
    for (NoteNumber nn : thisNoteMappings) {
      MidiMessage newMsg(msg.item);
      newMsg.setNoteNumber(nn);
      midibuf.addEvent(newMsg, msg.samplePosition);
    }
  }

  // Then other events concerning a specific note (poly aftertouch notably):
  for (auto& msg : eventsToRemap) {
    int note = msg.item.getNoteNumber();
    int chan = msg.item.getChannel();
    Array<NoteNumber>& thisNoteMappings =
      mCurMappings.getReference(Mapping::getNoteOnChan(msg.item));
    int size = thisNoteMappings.size();
    for (NoteNumber nn : thisNoteMappings) {
      MidiMessage newMsg(msg.item);
      newMsg.setNoteNumber(nn);
      midibuf.addEvent(newMsg, msg.samplePosition);
    }
  }
}
