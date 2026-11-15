#include "PluginEditor.h"
juce::AudioProcessorEditor* GroovulationProcessor::createEditor() { return new juce::GenericAudioProcessorEditor (*this); }
