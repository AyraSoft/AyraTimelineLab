/** @file SessionCodec.h
    Codec esclusivo del Lab, non compatibile con .aym/.ayt.
    Input JSON limitato a 2 MiB e validato atomicamente prima della sostituzione.
    @see SessionCodec.cpp, fixtures/session.json */
#pragma once
#include <juce_core/juce_core.h>
#include "LabModel.h"
namespace ayra::lab
{
juce::String encodeSession(const Session&);
juce::Result decodeSession(const juce::String&,Session& destination);
}
