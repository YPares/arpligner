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

  void mapToNoteSet(PatternNotesWraparound::Enum wrapMode,
    const NoteSet& curNoteSet,
    int degreeNum,
    Array<NoteNumber>& thisNoteMappings) {
    int curNoteSetSize = curNoteSet.size();

    if ((degreeNum < 0 || degreeNum >= curNoteSetSize) &&
      wrapMode == PatternNotesWraparound::NO_WRAPAROUND ||
      curNoteSetSize == 0)
      return;

    int numValidDegrees =
      (wrapMode <= PatternNotesWraparound::AFTER_ALL_DEGREES)
      ? curNoteSetSize
      : wrapMode;
    int wantedDegree;
    if (degreeNum >= 0)
      wantedDegree = degreeNum % numValidDegrees;
    else {
      int absRem = (-degreeNum) % numValidDegrees;
      wantedDegree = (absRem == 0) ? 0 : numValidDegrees - absRem;
    }
    int wantedOctaveShift = floor((float)degreeNum / (float)numValidDegrees);

    if (wantedDegree < curNoteSetSize)
      thisNoteMappings.add(curNoteSet[wantedDegree] + 12 * wantedOctaveShift);
  }

  void mapPatternNote(NoteNumber referenceNote,
    PatternNotesMapping::Enum mappingMode,
    PatternNotesWraparound::Enum wrapMode,
    UnmappedNotesBehaviour::Enum unmappedBeh,
    const NoteSet& curNoteSet,
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
        mapToNoteSet(wrapMode, curNoteSet, sign * absOffset, thisNoteMappings);
      }
      break;
    default:
      mapToNoteSet(wrapMode, curNoteSet, offsetFromRef, thisNoteMappings);
      break;
    }

    if (thisNoteMappings.size() == 0) { // If the note has not been mapped yet:
      switch (unmappedBeh) {
      case UnmappedNotesBehaviour::SILENCE:
        break;
      case UnmappedNotesBehaviour::PLAY_ALL_DEGREES_UP_TO_NOTE:
        for (NoteNumber chdNote : curNoteSet) {
          if (chdNote <= noteCodeIn)
            thisNoteMappings.add(chdNote);
        }
        break;
      case UnmappedNotesBehaviour::TRANSPOSE_FROM_FIRST_DEGREE:
        thisNoteMappings.add(curNoteSet[0] + offsetFromRef);
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

  if (behaviour == InstanceBehaviour::BYPASS)
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

  Array<NoteNumber> chordNoteOns, chordNoteOffs;
  Array<MidiMessage> ptrnNoteOns, ptrnNoteOffs, otherMsgs;

  for (auto msgMD : midibuf) {
    auto msg = msgMD.getMessage();
    if (msg.isNoteOn()) {
      if (behaviour == msg.getChannel())
        chordNoteOns.add(msg.getNoteNumber());
      else
        ptrnNoteOns.add(msg);
    }
    else if (msg.isNoteOff()) {
      if (behaviour == msg.getChannel())
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

  if (behaviour != InstanceBehaviour::IS_PATTERN) {
    for (int n : chordNoteOns)
      chd->addChordNote(n);
    for (int n : chordNoteOffs)
      chd->rmChordNote(n);
    updateChordStore(chd);
  }

  processPatternNotes(chd, ptrnNoteOns, ptrnNoteOffs, midibuf);
}

void Arp::turnNoteSetToUseIntoScale(PreMappingChordProcessing::Enum chordProc) {
  int numChordNotes = mNoteSetToUse.size();
  NoteNumber chordRoot = mNoteSetToUse[0];
  
  // We first compute the octave shift to apply, by computing the distance
  // between the lowest note and the average of the rest of the notes
  float avgNote = 0;
  for (int i = 1; i<numChordNotes; i++)
    avgNote += mNoteSetToUse[i];
  avgNote = avgNote / (numChordNotes - 1);
  NoteNumber scaleRoot = chordRoot + 12*round((avgNote - chordRoot)/12.0f);
  
  NoteSet compactChord;
  // Then we "regroup" all the chord notes so they fit into one octave,
  // starting from the lowest chord note transposed to the average wanted
  // octave:
  for (auto note : mNoteSetToUse)
    compactChord.add(scaleRoot + (note - chordRoot)%12);

  NoteSet scale(compactChord);
  // Then we fill in the gaps by adding 1 whole step to each degree just
  // before a gap (a "gap" being any interval strictly larger than a whole
  // step)...
  for (int i=0; i<scale.size()-1; i++)
    if (scale[i+1] - scale[i] > 2)
      scale.add(scale[i] + 2);
  // ...and to the last degree, until we have a full scale up to a 7th:
  while (scale.getLast() < scaleRoot + 10)
    scale.add(scale.getLast() + 2);

  // Then if needed, we correct the 4th, 5th and 7th:
  if (scale.size() == 7 &&
      chordProc == PreMappingChordProcessing::ADD_WHOLE_STEPS_DEF_P4_P5_MAJ7) {
    if (!compactChord.contains(scale[3])) {
      scale.remove(3);
      scale.add(scaleRoot + 5); // Perfect 4th
    }
    if (!compactChord.contains(scale[4])) {
      scale.remove(4);
      scale.add(scaleRoot + 7); // Perfect 5th
    }
    if (!compactChord.contains(scale[6])) {
      scale.remove(6);
      scale.add(scaleRoot + 11); // Major 7th
    }
  }
  
  // Finally, we override mNoteSetToUse:
  mNoteSetToUse.swapWith(scale); 
}

void Arp::processPatternNotes(ChordStore* chd, Array<MidiMessage>& noteOns, Array<MidiMessage>& noteOffs, MidiBuffer& midibuf) {
  auto chordProc = (PreMappingChordProcessing::Enum)preMappingChordProcessing->getIndex();
  auto mappingMode = (PatternNotesMapping::Enum)patternNotesMapping->getIndex();
  auto wrapMode = (PatternNotesWraparound::Enum)patternNotesWraparound->getIndex();
  auto unmappedBeh = (UnmappedNotesBehaviour::Enum)unmappedNotesBehaviour->getIndex();
  auto referenceNote = firstDegreeCode->getIndex();
  auto updateState = !holdCurState->get();
  
  if (updateState) {
    chd->getCurrentChord(mNoteSetToUse, mShouldProcess, mShouldSilence);
    
    if (mShouldProcess && !mShouldSilence) {
      // If wanted, pre-process the current chord:
      if (chordProc == PreMappingChordProcessing::NONE) {}
      else if (chordProc == PreMappingChordProcessing::IGNORE_BASS_NOTE)
	mNoteSetToUse.remove(0);
      else if (mNoteSetToUse.size() >= 2)
	turnNoteSetToUseIntoScale(chordProc);
    }
  }

  if (mShouldSilence)
    noteOns.clear();
  
  // Process and add processable messages:

  for (auto& msg : noteOffs) { // Note OFFs first
    Array<NoteNumber>& thisNoteMappings =
      mCurMappings.getReference(Mapping::getNoteOnChan(msg));
    for (NoteNumber nn : thisNoteMappings) {
      MidiMessage newMsg(msg);
      newMsg.setNoteNumber(nn);
      midibuf.addEvent(newMsg, 0);
    }
    thisNoteMappings.clear();
  }

  for (auto& msg : noteOns) { // Then note ONs
    NoteNumber noteCodeIn = msg.getNoteNumber();
    // This should create a default (empty) array if the note has not
    // been mapped yet:
    Array<NoteNumber>& thisNoteMappings =
      mCurMappings.getReference(Mapping::getNoteOnChan(msg));
    // If we already have mappings for this note, it means we received 2+ NOTE ONs
    // in a row for it and no NOTE OFF, so first we off those mappings:
    for (NoteNumber nn : thisNoteMappings)
      midibuf.addEvent(MidiMessage::noteOff(msg.getChannel(), nn), 0);
    thisNoteMappings.clear();

    if (mShouldProcess) // The ChordStore tells us to process
      Mapping::mapPatternNote(referenceNote,
        mappingMode,
        wrapMode,
        unmappedBeh,
        mNoteSetToUse,
        noteCodeIn,
        thisNoteMappings);
    else // We map the note to itself
      thisNoteMappings.add(noteCodeIn);

    // We send NOTE ONs for all newly mapped notes:
    for (NoteNumber nn : thisNoteMappings) {
      MidiMessage newMsg(msg);
      newMsg.setNoteNumber(nn);
      midibuf.addEvent(newMsg, 0);
    }
  }
}
