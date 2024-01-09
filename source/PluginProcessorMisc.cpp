// This file contains all the functions needed by JUCE to interact with the DAWs
#include "PluginEditor.h"
#include "PluginProcessor.h"

const juce::String RaveAP::getName() const { return JucePlugin_Name; }

bool RaveAP::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool RaveAP::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool RaveAP::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double RaveAP::getTailLengthSeconds() const { return 0.0; }

int RaveAP::getNumPrograms() {
  // NB: some hosts don't cope very well if you tell them there are 0
  // programs, so this should be at least 1, even if you're not really
  // implementing programs.
  return 1;
}

int RaveAP::getCurrentProgram() { return 0; }

void RaveAP::setCurrentProgram(int /*index*/) {}

const juce::String RaveAP::getProgramName(int /*index*/) { return {}; }

void RaveAP::changeProgramName(int /*index*/,
                               const juce::String & /*newName*/) {}

bool RaveAP::hasEditor() const {
  return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *RaveAP::createEditor() {
  return new RaveAPEditor(*this, _avts);
}


void RaveAP::mute() {
 _smoothStatus = RAVESmoothingStatus::needsMute;
}

void RaveAP::unmute() { 
 _smoothStatus = RAVESmoothingStatus::needsDemute;
}

const bool RaveAP::getIsMuted() {
   return _isMuted.load(); 
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RaveAP::isBusesLayoutSupported(const BusesLayout &layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}
#endif
