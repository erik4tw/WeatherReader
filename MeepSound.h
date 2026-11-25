// MeepSound.h: The short sound on a change of the weather state and of the QNH, generated as a WAV file in memory.

#pragma once

#include <vector>

// Generates the "meep": a recreation of the original sound of a real system, built from nine measured partials
// with a short fade-in and fade-out (about 0.18 s), as a WAV file (16 bit, mono, 22050 Hz).
// Partials, duration and volume are set in MeepSound.cpp.
std::vector<unsigned char> BuildMeepWav();
