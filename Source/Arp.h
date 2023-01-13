/*
  ==============================================================================

    Arp.h
    Created: 12 Jan 2023 11:16:13pm
    Author:  Yves Parès

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"


class Arp : public ArplignerJuceAudioProcessor
{
private:
  int curChan;

public:
  Arp() : ArplignerJuceAudioProcessor(), curChan(0)
  {
  }

  void runArp(juce::MidiBuffer& midibuf)
  {
    DBG(curChan << " - " << param->get());
  }
};
