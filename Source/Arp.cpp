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
    const Chord& curChord,
    NoteNumber noteCodeIn,
    Array<NoteNumber>& thisNoteMappings) {
    int offsetFromRef = noteCodeIn - referenceNote;

    switch (mappingMode) {
    case PatternNotesMapping::MAP_NOTHING:
      break;
    case PatternNotesMapping::TRANSPOSE_FROM_FIRST_DEGREE:
      thisNoteMappings.add(curChord[0] + offsetFromRef);
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

  if (behaviour == InstanceBehaviour::BYPASS)
    return;

  Array<NoteNumber> chordNoteOns, chordNoteOffs;
  Array<MidiMessage> ptrnNoteOns, ptrnNoteOffs, otherMsgs;

  for (auto msgMD : midibuf) {
    auto msg = msgMD.getMessage();
    if (msg.isNoteOn()) {
      if (behaviour == InstanceBehaviour::IS_CHORD ||
        behaviour == msg.getChannel())
        chordNoteOns.add(msg.getNoteNumber());
      else
        ptrnNoteOns.add(msg);
    }
    else if (msg.isNoteOff()) {
      if (behaviour == InstanceBehaviour::IS_CHORD ||
        behaviour == msg.getChannel())
        chordNoteOffs.add(msg.getNoteNumber());
      else
        ptrnNoteOffs.add(msg);
    }
    else
      otherMsgs.add(msg);
  }

  midibuf.clear();

  for (auto& msg : otherMsgs)
    midibuf.addEvent(msg, 0);

  ChordStore* chd = getChordStore(behaviour);

  for (int n : chordNoteOns)
    chd->addChordNote(n);
  for (int n : chordNoteOffs)
    chd->rmChordNote(n);
  if (behaviour != InstanceBehaviour::IS_PATTERN)
    updateChordStore(chd);

  processPatternNotes(chd, ptrnNoteOns, ptrnNoteOffs, midibuf);
}

void Arp::processPatternNotes(ChordStore* chd, Array<MidiMessage>& noteOns, Array<MidiMessage>& noteOffs, MidiBuffer& midibuf) {
  auto mappingMode = (PatternNotesMapping::Enum)patternNotesMapping->getIndex();
  auto wrapMode = (PatternNotesWraparound::Enum)patternNotesWraparound->getIndex();
  auto referenceNote = firstDegreeCode->getIndex();

  Chord curChord;
  bool shouldProcess, shouldSilence;
  chd->getCurrentChord(curChord, shouldProcess, shouldSilence);

  if (shouldSilence)
    noteOns.clear();

  // Process and add processable messages:

  for (auto& msg : noteOffs) { // Note OFFs first
    Array<NoteNumber>& thisNoteMappings = mCurMappings.getReference(Mapping::getNoteOnChan(msg));
    if (thisNoteMappings.size() > 0) {
      for (NoteNumber nn : thisNoteMappings) {
        MidiMessage newMsg(msg);
        newMsg.setNoteNumber(nn);
        midibuf.addEvent(newMsg, 0);
      }
      thisNoteMappings.clear();
    }
    else // No mappings means we add the NOTE OFF as it is:
      midibuf.addEvent(msg, 0);
  }

  for (auto& msg : noteOns) { // Then note ONs
    NoteNumber noteCodeIn = msg.getNoteNumber();
    Array<NoteNumber>& thisNoteMappings = mCurMappings.getReference(Mapping::getNoteOnChan(msg));
    // If we already have mappings for this note, it means we received 2+ NOTE ONs
    // in a row for it and no NOTE OFF, so first we off those mappings:
    for (NoteNumber nn : thisNoteMappings)
      midibuf.addEvent(MidiMessage::noteOff(msg.getChannel(), nn), 0);
    thisNoteMappings.clear();
    if (shouldProcess) {
      Mapping::mapPatternNote(referenceNote,
        mappingMode,
        wrapMode,
        curChord,
        noteCodeIn,
        thisNoteMappings);
      // We send NOTE ONs for all newly mapped notes:
      for (NoteNumber nn : thisNoteMappings) {
        MidiMessage newMsg(msg);
        newMsg.setNoteNumber(nn);
        midibuf.addEvent(newMsg, 0);
      }
    }
    else { // We map the note to itself
      thisNoteMappings.add(noteCodeIn);
      midibuf.addEvent(msg, 0);
    }
  }
}
