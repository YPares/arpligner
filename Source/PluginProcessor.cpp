/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

    It is limited to JUCE boilerplate and parameters definition, storage and
    loading. Arp logic happens in Arp.cpp/.h

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "ChordStore.h"

//==============================================================================
ArplignerAudioProcessor::ArplignerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
  : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
    .withInput("Input", AudioChannelSet::stereo(), true)
#endif
    .withOutput("Output", AudioChannelSet::stereo(), true)
#endif
  )
#endif
{
  auto behVals = StringArray{ "Bypass" };
  for (int i = 1; i <= 16; i++)
    behVals.add(String("[Multi-chan] Chords on chan ") + String(i));
  behVals.add("[Multi-instance] Global chord instance");
  behVals.add("[Multi-instance] Pattern instance");
  addParameter
  (instanceBehaviour = new AudioParameterChoice
  ("chordChan", "Instance behaviour", behVals, 16));

  addParameter
  (whenNoChordNote = new AudioParameterChoice
  ("whenNoChordNote", "When no chord note",
    StringArray{ "Silence", "Use pattern notes as final notes", "Latch last chord" },
    WhenNoChordNote::LATCH_LAST_CHORD
  ));

  addParameter
  (whenSingleChordNote = new AudioParameterChoice
  ("whenSingleChordNote", "When single chord note",
    StringArray{ "Silence", "Use pattern notes as final notes", "Use as one-note chord", "Powerchord", "Transpose last chord" },
    WhenSingleChordNote::TRANSPOSE_LAST_CHORD
  ));

  addParameter
  (numMillisecsOfLatency = new AudioParameterInt
  ("numMillisecsOfLatency", "Global chord track lookahead (ms)", 0, 50, 15));

  StringArray notes;
  for (int i = 0; i <= 127; i++)
    notes.add(String(i) + " (" + MidiMessage::getMidiNoteName(i, true, true, 3) + ")");
  addParameter(firstDegreeCode = new AudioParameterChoice
  ("firstDegreeCode", "Reference pattern note", notes, 60));

  addParameter
  (chordToScale = new AudioParameterChoice
   ("chordToScale", "Turn chord into scale",
    StringArray{"None", "Add whole steps (nat7 by default)", "Add whole steps (b7 by default)"},
    ChordToScale::NONE));
  
  addParameter
  (patternNotesMapping = new AudioParameterChoice
  ("patternNotesMapping", "Pattern notes mapping",
    StringArray{ "Always leave unmapped", "Semitone to degree", "White key to degree"},
    PatternNotesMapping::SEMITONE_TO_DEGREE
  ));

  StringArray waModes = StringArray
  { "No wraparound", "[Dynamic] After all degrees", "[Fixed] Every 3rd pattern note" };
  for (int i = 3; i <= 12; i++)
    waModes.add(String("[Fixed] Every ") + String(i + 1) + "th pattern note");
  addParameter
  (patternNotesWraparound = new AudioParameterChoice
  ("patternNotesWraparound", "Pattern octave wraparound", waModes,
    PatternNotesWraparound::AFTER_ALL_CHORD_DEGREES));

  StringArray unmappedBehs = StringArray
  { "Silence", "Use as is", "Transpose from 1st degree", "Play all degrees up to note" };
  addParameter
  (unmappedNotesBehaviour = new AudioParameterChoice
  ("unmappedNotesBehaviour", "Unmapped notes behaviour", unmappedBehs,
    UnmappedNotesBehaviour::SILENCE));
}

ArplignerAudioProcessor::~ArplignerAudioProcessor()
{
}

//==============================================================================
const String ArplignerAudioProcessor::getName() const
{
  return JucePlugin_Name;
}

bool ArplignerAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool ArplignerAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool ArplignerAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double ArplignerAudioProcessor::getTailLengthSeconds() const
{
  return 0.0;
}

int ArplignerAudioProcessor::getNumPrograms()
{
  return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
  // so this should be at least 1, even if you're not really implementing programs.
}

int ArplignerAudioProcessor::getCurrentProgram()
{
  return 0;
}

void ArplignerAudioProcessor::setCurrentProgram(int index)
{
}

const String ArplignerAudioProcessor::getProgramName(int index)
{
  return {};
}

void ArplignerAudioProcessor::changeProgramName(int index, const String& newName)
{
}

void ArplignerAudioProcessor::releaseResources()
{
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ArplignerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
  ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
    && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
    return false;

  // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}
#endif

void ArplignerAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
  ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();

  // In case we have more outputs than inputs, this code clears any output
  // channels that didn't contain input data, (because these aren't
  // guaranteed to be empty - they may contain garbage).
  // This is here to avoid people getting screaming feedback
  // when they first compile a plugin, but obviously you don't need to keep
  // this code if your algorithm always overwrites all the output channels.
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  runArp(midiMessages);
}

//==============================================================================
bool ArplignerAudioProcessor::hasEditor() const
{
  return true;
}

AudioProcessorEditor* ArplignerAudioProcessor::createEditor()
{
  return new GenericAudioProcessorEditor(*this);
}

// Save state info
void ArplignerAudioProcessor::getStateInformation(MemoryBlock& destData)
{
  auto s = MemoryOutputStream(destData, true);
  s.writeInt(*instanceBehaviour);
  s.writeInt(*firstDegreeCode);
  s.writeInt(*whenNoChordNote);
  s.writeInt(*whenSingleChordNote);
  s.writeInt(*patternNotesMapping);
  s.writeInt(*numMillisecsOfLatency);
  s.writeInt(*patternNotesWraparound);
  s.writeInt(*unmappedNotesBehaviour);
  s.writeInt(*chordToScale);
}

// Reload state info
void ArplignerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
  auto s = MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false);
  *instanceBehaviour = s.readInt();
  *firstDegreeCode = s.readInt();
  *whenNoChordNote = s.readInt();
  *whenSingleChordNote = s.readInt();
  *patternNotesMapping = s.readInt();
  *numMillisecsOfLatency = s.readInt();
  *patternNotesWraparound = s.readInt();
  *unmappedNotesBehaviour = s.readInt();
  *chordToScale = s.readInt();
}
