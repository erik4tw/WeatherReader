// MeepSound.cpp: The short sound on a change of the weather state (see MeepSound.h)

#include "pch.h"
#include "MeepSound.h"

#include <cmath>

namespace
{
	// Second-order low-pass filter (biquad, Butterworth characteristic): cuts the high overtones of the sawtooth
	std::vector<double> LowPass(const std::vector<double>& input, double cutoffHz, double sampleRate)
	{
		const double q = 0.707;
		const double pi = 3.14159265358979323846;
		double w0 = 2.0 * pi * cutoffHz / sampleRate;
		double alpha = sin(w0) / (2.0 * q);
		double c = cos(w0);
		double a0 = 1.0 + alpha;
		double b0 = (1.0 - c) / 2.0 / a0;
		double b1 = (1.0 - c) / a0;
		double b2 = b0;
		double a1 = -2.0 * c / a0;
		double a2 = (1.0 - alpha) / a0;

		std::vector<double> output(input.size());
		double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
		for (size_t i = 0; i < input.size(); i++)
		{
			double y = b0 * input[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
			x2 = x1; x1 = input[i];
			y2 = y1; y1 = y;
			output[i] = y;
		}
		return output;
	}
}

// Layout of the WAV file: RIFF header, "fmt " chunk, "data" chunk (16 bit, mono, 22050 Hz).
std::vector<unsigned char> BuildMeepWav()
{
	const int sampleRate = 22050;
	const double frequencyLow = 415.3;		// the two tones in Hz (sounding at the same time)
	const double frequencyHigh = 493.6;
	const double cutoff = 1200.0;			// low-pass (applied twice): the higher, the brighter the sound
	const int durationMs = 250;				// duration
	const double volume = 0.22;				// volume (1.0 = full scale)

	int count = sampleRate * durationMs / 1000;
	int attack = sampleRate * 5 / 1000;		// 5 ms fade-in (against clicks)
	int release = sampleRate * 80 / 1000;	// 80 ms fade-out

	// Two sawtooth tones (phase 0..1 rises steadily and then jumps back: rough sound), added up
	std::vector<double> raw(count);
	for (int i = 0; i < count; i++)
	{
		double low = 2.0 * fmod(frequencyLow * i / sampleRate, 1.0) - 1.0;
		double high = 2.0 * fmod(frequencyHigh * i / sampleRate, 1.0) - 1.0;
		raw[i] = 0.5 * (low + high);
	}

	// Duller sound: filter twice, then bring the loudest point back to full scale before the volume is applied
	std::vector<double> filtered = LowPass(LowPass(raw, cutoff, sampleRate), cutoff, sampleRate);
	double peak = 0.0;
	for (int i = 0; i < count; i++)
		if (fabs(filtered[i]) > peak)
			peak = fabs(filtered[i]);
	if (peak <= 0.0)
		peak = 1.0;

	std::vector<short> samples;
	for (int i = 0; i < count; i++)
	{
		double envelope = 1.0;
		if (i < attack)
			envelope = (double)i / attack;
		else if (i > count - release)
			envelope = (double)(count - i) / release;

		samples.push_back((short)(volume * envelope * (filtered[i] / peak) * 32767.0));
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
