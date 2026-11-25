// MeepSound.cpp: The short sound on a change of the weather state and of the QNH (see MeepSound.h)

#include "pch.h"
#include "MeepSound.h"

#include <cmath>

namespace
{
	// One partial of the sound: frequency in Hz, relative amplitude (loudest = 1.0) and phase in radians.
	// The values were measured on the original recording of a real system (least-squares fit, 98 % of the variance).
	struct Partial
	{
		double frequency;
		double amplitude;
		double phase;
	};

	const Partial PARTIALS[] =
	{
		{  517.03, 0.1554, -2.4465 },
		{  636.54, 0.3964,  2.8537 },
		{  777.72, 1.0000,  2.8041 },
		{ 1033.64, 0.2912,  2.7139 },
		{ 1273.33, 0.3207,  0.5147 },
		{ 1556.23, 0.2657, -2.9015 },
		{ 1909.97, 0.1168, -0.0455 },
		{ 2332.81, 0.0901, -0.4917 },
		{ 3183.59, 0.0185, -1.1256 },
	};

	// The phases refer to a point in time 12 ms after the start of the original recording
	const double PHASE_REFERENCE_SECONDS = 0.012;
}

// Layout of the WAV file: RIFF header, "fmt " chunk, "data" chunk (16 bit, mono, 22050 Hz).
std::vector<unsigned char> BuildMeepWav()
{
	const int sampleRate = 22050;
	const double durationSeconds = 0.181;	// duration of the original sound
	const double volume = 0.22;				// volume (1.0 = full scale)
	const double pi = 3.14159265358979323846;

	int count = (int)(sampleRate * durationSeconds + 0.5);
	int attack = sampleRate * 8 / 1000;		// 8 ms fade-in (against clicks)
	int release = sampleRate * 20 / 1000;	// 20 ms fade-out

	// Add up the partials
	std::vector<double> raw(count);
	double peak = 0.0;
	for (int i = 0; i < count; i++)
	{
		double t = (double)i / sampleRate - PHASE_REFERENCE_SECONDS;
		double sum = 0.0;
		for (size_t p = 0; p < sizeof(PARTIALS) / sizeof(PARTIALS[0]); p++)
			sum += PARTIALS[p].amplitude * cos(2.0 * pi * PARTIALS[p].frequency * t + PARTIALS[p].phase);

		double envelope = 1.0;
		if (i < attack)
			envelope = (double)i / attack;
		else if (i >= count - release)
			envelope = (double)(count - i) / release;

		raw[i] = sum * envelope;
		if (fabs(raw[i]) > peak)
			peak = fabs(raw[i]);
	}
	if (peak <= 0.0)
		peak = 1.0;

	// Bring the loudest point back to full scale before the volume is applied
	std::vector<short> samples;
	for (int i = 0; i < count; i++)
		samples.push_back((short)(volume * (raw[i] / peak) * 32767.0));

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
