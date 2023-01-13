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