// MeepSound.cpp: The short sound on a change of the weather state (see MeepSound.h)

#include "pch.h"
#include "MeepSound.h"

#include <cmath>

// Layout of the WAV file: RIFF header, "fmt " chunk, "data" chunk (16 bit, mono, 22050 Hz).
std::vector<unsigned char> BuildMeepWav()
{
	const int sampleRate = 22050;
	const double frequency = 440.0;		// pitch in Hz
	const int durationMs = 250;			// duration
	const double volume = 0.22;			// volume (1.0 = full scale)

	int count = sampleRate * durationMs / 1000;
	int attack = sampleRate * 5 / 1000;		// 5 ms fade-in (against clicks)
	int release = sampleRate * 80 / 1000;	// 80 ms fade-out

	std::vector<short> samples;
	for (int i = 0; i < count; i++)
	{
		double envelope = 1.0;
		if (i < attack)
			envelope = (double)i / attack;
		else if (i > count - release)
			envelope = (double)(count - i) / release;

		// Sawtooth: phase 0..1 rises steadily and then jumps back (hence the rough sound)
		double phase = fmod(frequency * i / sampleRate, 1.0);
		double sawtooth = 2.0 * phase - 1.0;

		samples.push_back((short)(volume * envelope * sawtooth * 32767.0));
	}

	unsigned int dataBytes = (unsigned int)(samples.size() * sizeof(short));
	std::vector<unsigned char> wav;

	struct Writer
	{
		static void U32(std::vector<unsigned char>& v, unsigned int x)
		{
			for (int i = 0; i < 4; i++) v.push_back((unsigned char)((x >> (8 * i)) & 0xFF));
		}
		static void U16(std::vector<unsigned char>& v, unsigned int x)
		{
			for (int i = 0; i < 2; i++) v.push_back((unsigned char)((x >> (8 * i)) & 0xFF));
		}
		static void Tag(std::vector<unsigned char>& v, const char* tag)
		{
			for (int i = 0; i < 4; i++) v.push_back((unsigned char)tag[i]);
		}
	};

	Writer::Tag(wav, "RIFF");
	Writer::U32(wav, 36 + dataBytes);
	Writer::Tag(wav, "WAVE");
	Writer::Tag(wav, "fmt ");
	Writer::U32(wav, 16);					// size of the fmt chunk
	Writer::U16(wav, 1);					// PCM
	Writer::U16(wav, 1);					// mono
	Writer::U32(wav, sampleRate);
	Writer::U32(wav, sampleRate * 2);		// bytes per second
	Writer::U16(wav, 2);					// bytes per sample
	Writer::U16(wav, 16);					// bits per sample
	Writer::Tag(wav, "data");
	Writer::U32(wav, dataBytes);
	for (size_t i = 0; i < samples.size(); i++)
		Writer::U16(wav, (unsigned short)samples[i]);

	return wav;
}
