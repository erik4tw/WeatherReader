// MeepSound.h: The short sound on a change of the weather state, generated as a WAV file in memory.

#pragma once

#include <vector>

// Generates the "meep": a short, rough tone (sawtooth wave) as a WAV file (16 bit, mono, 22050 Hz).
// Pitch, duration and volume are set at the top of the function.
std::vector<unsigned char> BuildMeepWav();
