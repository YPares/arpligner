/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

    It is limited to JUCE boilerplate and parameters definition, storage and
    loading. Arp logic happens in Arp.cpp/.h

  ==============================================================================
*/

#include "PluginProcessor.h"

//==============================================================================
ArplignerJuceAudioProcessor::ArplignerJuceAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
  addParameter (chordChan = new juce::AudioParameterInt ("chordChan", "Chord channel", 1, 16, 16));
  addParameter (firstDegreeCode = new juce::AudioParameterInt ("firstDegreeCode", "First degree MIDI code", 0, 127, 60));
  addParameter (chordNotesPassthrough = new juce::AudioParameterBool("chordNotesPassthrough", "Chord notes passthrough", false));
  addParameter
    (whenNoChordNote = new juce::AudioParameterChoice
     ("whenNoChordNote", "When no chord note",
      juce::StringArray {"Latch last chord", "Silence", "Use patterns as notes"},
      WhenNoChordNote::LATCH_LAST_CHORD
      ));
  addParameter
    (whenSingleChordNote = new juce::AudioParameterChoice
     ("whenSingleChordNote", "When single chord note",
      juce::StringArray {"Transpose last chord", "Powerchord", "Use as is", "Silence", "Use patterns as notes"},
      WhenSingleChordNote::TRANSPOSE_LAST_CHORD
      ));
  addParameter (ignoreBlackKeysInPatterns = new juce::AudioParameterBool("ignoreBlackKeysInPatterns", "Ignore black keys in patterns", false));
}

ArplignerJuceAudioProcessor::~ArplignerJuceAudioProcessor()
{
}

//==============================================================================
const juce::String ArplignerJuceAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ArplignerJuceAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ArplignerJuceAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ArplignerJuceAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ArplignerJuceAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ArplignerJuceAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ArplignerJuceAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ArplignerJuceAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ArplignerJuceAudioProcessor::getProgramName (int index)
{
    return {};
}

void ArplignerJuceAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ArplignerJuceAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void ArplignerJuceAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ArplignerJuceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
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

void ArplignerJuceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    runArp(midiMessages);
}

//==============================================================================
bool ArplignerJuceAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ArplignerJuceAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

// Save state info
void ArplignerJuceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
  auto s = juce::MemoryOutputStream(destData, true);
  s.writeInt(*chordChan);
  s.writeInt(*firstDegreeCode);
  s.writeBool(*chordNotesPassthrough);
  s.writeInt(*whenNoChordNote);
  s.writeInt(*whenSingleChordNote);
  s.writeBool(*ignoreBlackKeysInPatterns);
}

// Reload state info
void ArplignerJuceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
  auto s = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false);
  *chordChan = s.readInt();
  *firstDegreeCode = s.readInt();
  *chordNotesPassthrough = s.readBool();
  *whenNoChordNote = s.readInt();
  *whenSingleChordNote = s.readInt();
  *ignoreBlackKeysInPatterns = s.readBool();
}
