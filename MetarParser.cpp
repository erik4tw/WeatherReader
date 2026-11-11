// MetarParser.cpp: Determines the weather state of the control zone from a METAR.

#include "pch.h"
#include "MetarParser.h"

#include <string>
#include <vector>
#include <sstream>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cmath>

namespace
{
	bool IsDigits(const std::string& s, size_t from, size_t count)
	{
		if (s.size() < from + count)
			return false;
		for (size_t i = from; i < from + count; i++)
			if (!isdigit((unsigned char)s[i]))
				return false;
		return true;
	}

	bool StartsWith(const std::string& s, const char* prefix)
	{
		return s.compare(0, strlen(prefix), prefix) == 0;
	}

	// Standalone visibility: exactly 4 digits (optionally with NDV at the end)
	bool IsVisibilityToken(const std::string& t)
	{
		return IsDigits(t, 0, 4) && (t.size() == 4 || t == t.substr(0, 4) + "NDV");
	}

	// Pressure group: "Q" (hPa) or "A" (inches of mercury) followed by exactly 4 digits, e.g. "Q1019", "A2992"
	bool IsQnhToken(const std::string& t)
	{
		return t.size() == 5 && (t[0] == 'Q' || t[0] == 'A') && IsDigits(t, 1, 4);
	}

	// Wind group, e.g. "25008KT", "25008G18KT", "VRB03KT", "250100KT" (3-digit speed).
	// Returns direction (not for VRB), speed and gusts (0 = none) in knots.
	bool ParseWindToken(const std::string& t, bool& variable, int& dirDeg, int& speedKt, int& gustKt)
	{
		if (t.size() < 7 || t.compare(t.size() - 2, 2, "KT") != 0)
			return false;

		std::string body = t.substr(0, t.size() - 2);	// without "KT"

		if (body.compare(0, 3, "VRB") == 0)
			variable = true;
		else if (IsDigits(body, 0, 3))
			variable = false;
		else
			return false;

		size_t pos = 3;
		size_t speedDigits = 0;
		while (pos + speedDigits < body.size() && isdigit((unsigned char)body[pos + speedDigits]))
			speedDigits++;
		if (speedDigits < 2 || speedDigits > 3)
			return false;

		int speed = atoi(body.substr(pos, speedDigits).c_str());
		pos += speedDigits;

		int gust = 0;
		if (pos < body.size())
		{
			if (body[pos] != 'G')
				return false;
			pos++;

			size_t gustDigits = body.size() - pos;
			if (gustDigits < 2 || gustDigits > 3 || !IsDigits(body, pos, gustDigits))
				return false;
			gust = atoi(body.substr(pos).c_str());
		}

		dirDeg = variable ? 0 : atoi(body.substr(0, 3).c_str());
		speedKt = speed;
		gustKt = gust;
		return true;
	}

	// Trend and remarks start with these groups and are not part of the current weather.
	bool IsTrendStart(const std::string& t)
	{
		return t == "BECMG" || t == "TEMPO" || t == "NOSIG" || t == "RMK";
	}

	// RVR group, e.g. "R25L/0650U", "R22/P2000N", "R08/M0050", "R26L/0400V0800D".
	// Returns the lowest value in rvrM (the smaller one for a varying RVR). P (more than) and M (less
	// than) count with the stated value. An unavailable RVR (e.g. "R26L/////") is not a valid group.
	bool ParseRvrToken(const std::string& t, int& rvrM)
	{
		if (t.size() < 8 || t[0] != 'R' || !IsDigits(t, 1, 2))
			return false;

		size_t pos = 3;
		if (pos < t.size() && (t[pos] == 'L' || t[pos] == 'C' || t[pos] == 'R'))
			pos++;

		if (pos >= t.size() || t[pos] != '/')
			return false;
		pos++;

		int values[2] = { 0, 0 };
		int count = 0;

		while (count < 2)
		{
			if (pos < t.size() && (t[pos] == 'P' || t[pos] == 'M'))
				pos++;
			if (!IsDigits(t, pos, 4))
				return false;

			values[count++] = atoi(t.substr(pos, 4).c_str());
			pos += 4;

			// Varying RVR: "<low>V<high>"
			if (count == 1 && pos < t.size() && t[pos] == 'V')
				pos++;
			else
				break;
		}

		// optional trend
		if (pos < t.size() && (t[pos] == 'U' || t[pos] == 'D' || t[pos] == 'N'))
			pos++;

		if (pos != t.size())
			return false;

		rvrM = values[0];
		if (count == 2 && values[1] < rvrM)
			rvrM = values[1];
		return true;
	}

	// Group of the form [intensity][VC][descriptor][phenomena...], e.g. "-SHRA", "+TSGR", "BR", "VCSH".
	// Returns false if the group is not valid current weather (e.g. "RESN" = recent weather).
	// wintryCode is set to the first SN, SG, PL, GR or GS contained (if still empty).
	bool ParseWeatherToken(const std::string& t, std::string& wintryCode)
	{
		static const char* descriptors[] = { "MI", "BC", "PR", "DR", "BL", "SH", "TS", "FZ" };
		static const char* phenomena[] = { "DZ", "RA", "SN", "SG", "IC", "PL", "GR", "GS", "UP",
			"BR", "FG", "FU", "VA", "DU", "SA", "HZ", "PO", "SQ", "FC", "SS", "DS" };
		static const char* wintryCodes[] = { "SN", "SG", "PL", "GR", "GS" };

		size_t pos = 0;
		bool any = false;
		std::string firstWintry;

		if (pos < t.size() && (t[pos] == '-' || t[pos] == '+'))
			pos++;

		if (t.compare(pos, 2, "VC") == 0)
		{
			pos += 2;
			any = true;
		}

		for (size_t i = 0; i < sizeof(descriptors) / sizeof(descriptors[0]); i++)
		{
			if (t.compare(pos, 2, descriptors[i]) == 0)
			{
				pos += 2;
				any = true;
				break;
			}
		}

		while (pos < t.size())
		{
			std::string code = t.substr(pos, 2);

			bool known = false;
			for (size_t i = 0; i < sizeof(phenomena) / sizeof(phenomena[0]); i++)
				if (code == phenomena[i])
					known = true;
			if (!known)
				return false;

			for (size_t i = 0; i < sizeof(wintryCodes) / sizeof(wintryCodes[0]); i++)
				if (code == wintryCodes[i] && firstWintry.empty())
					firstWintry = code;

			pos += 2;
			any = true;
		}

		if (!any)
			return false;

		if (wintryCode.empty())
			wintryCode = firstWintry;
		return true;
	}
}

MetarAssessment AssessMetar(const char* sMetar)
{
	MetarAssessment result;

	if (sMetar == nullptr)
		return result;

	// Convert to upper case and split into groups (separated by spaces)
	std::vector<std::string> tokens;
	{
		std::istringstream stream(sMetar);
		std::string token;
		while (stream >> token)
		{
			for (size_t i = 0; i < token.size(); i++)
				token[i] = (char)toupper((unsigned char)token[i]);
			tokens.push_back(token);
		}
	}

	// Observation time (first group of the form ddhhmmZ, e.g. "061820Z"), wind (first wind group) and
	// pressure (first Q/A group), each only before the trend
	for (size_t i = 0; i < tokens.size(); i++)
	{
		const std::string& t = tokens[i];
		if (IsTrendStart(t))
			break;

		if (!result.haveObsTime && t.size() == 7 && IsDigits(t, 0, 6) && t[6] == 'Z')
		{
			int hour = atoi(t.substr(2, 2).c_str());
			int minute = atoi(t.substr(4, 2).c_str());
			if (hour < 24 && minute < 60)
			{
				result.haveObsTime = true;
				result.obsHour = hour;
				result.obsMinute = minute;
			}
		}
		else if (!result.haveWind)
		{
			bool variable = false;
			int dirDeg = 0, speedKt = 0, gustKt = 0;
			if (ParseWindToken(t, variable, dirDeg, speedKt, gustKt))
			{
				result.haveWind = true;
				result.windVariable = variable;
				result.windDirDeg = dirDeg;
				result.windSpeedKt = speedKt;
				result.windGustKt = gustKt;
				result.windGroup = t;
			}
		}

		if (result.qnh.empty() && IsQnhToken(t))
			result.qnh = t;
	}

	bool haveVisibility = false;
	int  visibilityM = 0;
	bool haveCloudInfo = false;		// CAVOK/NCD/NSC/... or at least one cloud group found
	bool unknownCeiling = false;	// BKN/OVC/VV without height (and without CB/TCU): cannot be evaluated
	bool haveCeiling = false;
	int  ceilingFt = 0;
	std::string wintryCode;			// first SN, SG, PL, GR or GS in the current weather
	bool verticalVisibilityLost = false;	// VV/// : vertical visibility not measurable (counts as LVO)
	bool haveRvr = false;
	int  minRvrM = 0;				// lowest RVR of all groups
	std::vector<RvrGroup> rvrGroups;	// all groups

	int rvrValue = 0;

	for (size_t i = 0; i < tokens.size(); i++)
	{
		const std::string& t = tokens[i];

		if (IsTrendStart(t))
			break;

		if (t == "CAVOK")
		{
			// CAVOK: visibility 10 km or more (= 9999) and no clouds
			if (!haveVisibility)
			{
				haveVisibility = true;
				visibilityM = 9999;
			}
			haveCloudInfo = true;
		}
		else if (t == "NCD" || t == "NSC" || t == "SKC" || t == "CLR")
		{
			haveCloudInfo = true;	// no clouds or no ceiling
		}
		else if (IsVisibilityToken(t))
		{
			// Only the first value counts (prevailing visibility)
			if (!haveVisibility)
			{
				haveVisibility = true;
				visibilityM = atoi(t.substr(0, 4).c_str());
			}
		}
		else if (StartsWith(t, "FEW") || StartsWith(t, "SCT") ||
		         StartsWith(t, "BKN") || StartsWith(t, "OVC") || StartsWith(t, "VV"))
		{
			bool isVV = StartsWith(t, "VV");
			bool isCeilingType = !StartsWith(t, "FEW") && !StartsWith(t, "SCT");	// BKN, OVC, VV
			size_t numStart = isVV ? 2 : 3;

			if (IsDigits(t, numStart, 3))		// height in hundreds of feet; the rest (CB/TCU) does not matter
			{
				haveCloudInfo = true;

				if (isCeilingType)
				{
					int heightFt = atoi(t.substr(numStart, 3).c_str()) * 100;
					if (!haveCeiling || heightFt < ceilingFt)
					{
						haveCeiling = true;
						ceilingFt = heightFt;
					}
				}
			}
			else if (t.size() >= numStart + 3 && t.compare(numStart, 3, "///") == 0)
			{
				// Height unknown. With CB/TCU a height of at least 1500 ft is assumed
				// (affects neither VMC nor IMC). Otherwise the ceiling cannot be determined.
				haveCloudInfo = true;

				if (isCeilingType)
				{
					std::string rest = t.substr(numStart + 3);
					if (rest != "CB" && rest != "TCU")
						unknownCeiling = true;
				}

				if (isVV)
					verticalVisibilityLost = true;
			}
		}
		else if (t[0] == 'R' && ParseRvrToken(t, rvrValue))
		{
			if (!haveRvr || rvrValue < minRvrM)
			{
				haveRvr = true;
				minRvrM = rvrValue;
			}

			// Keep the group itself (for the RVR row of the box): "R25L/..." -> runway 25, side L
			RvrGroup group;
			group.text = t;
			group.runway = atoi(t.substr(1, 2).c_str());
			if (t[3] == 'L' || t[3] == 'C' || t[3] == 'R')
				group.side = t[3];
			rvrGroups.push_back(group);
		}
		else
		{
			// Current weather (recent weather such as "RESN" is not recognised and thus ignored)
			ParseWeatherToken(t, wintryCode);
		}
	}

	// The values read (for displaying the reason)
	result.haveVisibility = haveVisibility;
	result.visibilityM = visibilityM;
	result.haveCeiling = haveCeiling;
	result.ceilingFt = ceilingFt;
	result.verticalVisibilityLost = verticalVisibilityLost;
	result.haveRvr = haveRvr;
	result.minRvrM = minRvrM;
	result.rvrGroups = rvrGroups;
	if (!wintryCode.empty())
	{
		result.wintryCode[0] = wintryCode[0];
		result.wintryCode[1] = wintryCode[1];
	}

	// LVO takes precedence and needs neither visibility nor a fully evaluable cloud report
	bool lvo = verticalVisibilityLost ||
	           (haveCeiling && ceilingFt < LVO_BELOW_CEILING_FT) ||
	           (haveRvr && minRvrM <= LVO_MAX_RVR_M);
	if (lvo)
	{
		result.rules = FlightRules::Lvo;
		return result;
	}

	if (!haveVisibility || !haveCloudInfo || unknownCeiling)
		return result;		// state stays "Unknown"

	bool vmc = (!haveCeiling || ceilingFt >= VMC_MIN_CEILING_FT) &&
	           visibilityM >= VMC_MIN_VISIBILITY_M;
	if (vmc)
	{
		result.rules = FlightRules::VMC;
		return result;
	}

	bool reducedSeparation = visibilityM >= REDUCED_MIN_VISIBILITY_M &&
	                         (!haveCeiling || ceilingFt >= REDUCED_MIN_CEILING_FT) &&
	                         wintryCode.empty();

	result.rules = reducedSeparation ? FlightRules::ImcYellow : FlightRules::ImcOrange;
	return result;
}

WindComponents ComputeWindComponents(int windDirDeg, int windSpeedKt, int runwayHeadingDeg)
{
	const double PI = 3.14159265358979323846;

	// Angle between the direction the wind comes from and the runway heading:
	// 0 degrees = wind straight from ahead (pure headwind), 180 degrees = pure tailwind
	double angle = (windDirDeg - runwayHeadingDeg) * PI / 180.0;

	WindComponents result;
	result.headwindKt = (int)std::lround(windSpeedKt * std::cos(angle));
	result.crosswindKt = (int)std::lround(std::fabs(windSpeedKt * std::sin(angle)));
	return result;
}

FlightRules DetermineFlightRules(const char* sMetar)
{
	return AssessMetar(sMetar).rules;
}
