// DisplayLogic.cpp: Texts and decisions for the display in the box (see DisplayLogic.h)

#include "pch.h"
#include "DisplayLogic.h"

#include <cstdio>
#include <cstdlib>

std::string BuildDetail(const MetarAssessment& a)
{
	std::string detail;
	char buffer[32];

	switch (a.rules)
	{
	case FlightRules::Lvo:
		// The values that trigger LVO
		if (a.haveRvr && a.minRvrM <= LVO_MAX_RVR_M)
		{
			sprintf_s(buffer, "RVR %d", a.minRvrM);
			detail = buffer;
		}
		if (a.verticalVisibilityLost || (a.haveCeiling && a.ceilingFt < LVO_BELOW_CEILING_FT))
		{
			if (!detail.empty())
				detail += SEPARATOR;
			if (a.verticalVisibilityLost)
			{
				detail += "CEIL ///";
			}
			else
			{
				sprintf_s(buffer, "CEIL %d", a.ceilingFt);
				detail += buffer;
			}
		}
		break;

	case FlightRules::VMC:
	case FlightRules::ImcYellow:
	case FlightRules::ImcOrange:
		if (a.haveCeiling)
		{
			sprintf_s(buffer, "CEIL %d", a.ceilingFt);
			detail = buffer;
		}
		else
		{
			detail = "NO CEIL";
		}

		detail += SEPARATOR;
		detail += "VIS ";
		if (a.haveVisibility)
		{
			sprintf_s(buffer, "%d", a.visibilityM);
			detail += buffer;
		}
		else
		{
			detail += "-";
		}

		// In addition the wintry phenomenon found (SN, SG, PL, GR, GS), which rules out RRS
		if (a.wintryCode[0] != '\0')
		{
			detail += SEPARATOR;
			detail += a.wintryCode;
		}
		break;

	default:
		break;
	}

	return detail;
}

namespace
{
	// Value of the wind component, with the gust value in brackets for gusts, e.g. "13" or "13 (23)".
	// If the gust value equals the mean value (e.g. crosswind 0), it is not shown separately.
	std::string FormatComponent(int meanKt, int gustKt, bool hasGust)
	{
		char buffer[32];
		if (hasGust && gustKt != meanKt)
			sprintf_s(buffer, "%d (%d)", meanKt, gustKt);
		else
			sprintf_s(buffer, "%d", meanKt);
		return buffer;
	}
}

std::vector<std::string> BuildWindLines(const MetarAssessment& a, const std::vector<int>& runways,
	std::vector<bool>& tailwindOverLimit)
{
	std::vector<std::string> lines;
	tailwindOverLimit.clear();
	if (!a.haveWind || runways.empty())
		return lines;

	bool hasGust = a.windGustKt > a.windSpeedKt;
	char buffer[64];

	if (a.windVariable)
	{
		if (hasGust)
			sprintf_s(buffer, "WIND VRB %d (%d) kts", a.windSpeedKt, a.windGustKt);
		else
			sprintf_s(buffer, "WIND VRB %d kts", a.windSpeedKt);
		lines.push_back(buffer);
		return lines;
	}

	for (size_t i = 0; i < runways.size(); i++)
	{
		WindComponents mean = ComputeWindComponents(a.windDirDeg, a.windSpeedKt, runways[i] * 10);
		WindComponents gust = hasGust ? ComputeWindComponents(a.windDirDeg, a.windGustKt, runways[i] * 10) : mean;

		// Tailwind if the component is negative (at 0 the gust value decides)
		int reference = (mean.headwindKt != 0) ? mean.headwindKt : gust.headwindKt;
		bool tailwind = reference < 0;
		tailwindOverLimit.push_back(-mean.headwindKt > RRS_MAX_TAILWIND_KT);

		sprintf_s(buffer, "RWY %02d %s: ", runways[i], tailwind ? "TW" : "HW");
		std::string line = buffer;
		line += FormatComponent(abs(mean.headwindKt), abs(gust.headwindKt), hasGust);
		if (!hasGust)
			line += " kts";

		line += SEPARATOR;
		line += "CW: ";
		line += FormatComponent(mean.crosswindKt, gust.crosswindKt, hasGust);
		line += " kts";

		lines.push_back(line);
	}

	return lines;
}

RrsResult EvaluateRrs(const MetarAssessment& a, const std::vector<int>& runways,
	const std::vector<bool>& tailwindOverLimit)
{
	RrsResult result;
	result.shown = a.rules;
	result.weatherAllows = false;
	result.allAllowed = true;

	bool wintry = a.wintryCode[0] != '\0';
	if (a.rules == FlightRules::VMC && wintry)
	{
		result.allAllowed = false;		// VMC, but SN/SG/PL/GR/GS: RRS on no runway
		return result;
	}

	if (a.rules != FlightRules::ImcYellow && a.rules != FlightRules::VMC)
		return result;

	result.weatherAllows = true;

	bool windKnown = !runways.empty() && tailwindOverLimit.size() == runways.size();
	for (size_t i = 0; i < runways.size(); i++)
	{
		if (windKnown && tailwindOverLimit[i])
			result.allAllowed = false;
		else
			result.allowedRunways.push_back(runways[i]);
	}

	if (a.rules == FlightRules::ImcYellow && !runways.empty() && result.allowedRunways.empty())
		result.shown = FlightRules::ImcOrange;

	return result;
}

RrsNote BuildRrsNote(const RrsResult& rrs)
{
	RrsNote note;

	switch (rrs.shown)
	{
	case FlightRules::ImcOrange:
		note.present = true;
		note.before = "RRS ";
		note.bold = "NOT";
		note.after = " POSSIBLE";
		return note;

	case FlightRules::ImcYellow:
		if (rrs.allAllowed)
		{
			note.present = true;
			note.before = "RRS ";
			note.after = "POSSIBLE";
			return note;
		}
		break;		// restricted: like VMC

	case FlightRules::VMC:
		break;

	default:
		return note;	// LVO and unknown: no note
	}

	if (rrs.allAllowed)
		return note;	// VMC without restriction: no note

	note.present = true;
	note.before = "RRS ";
	if (rrs.allowedRunways.empty())
	{
		note.bold = "NOT";
		note.after = " POSSIBLE";
	}
	else
	{
		// RRS on only some of the active runways, e.g. "RRS ONLY RWY 07, 18"
		note.bold = "ONLY";
		note.after = " RWY";
		for (size_t i = 0; i < rrs.allowedRunways.size(); i++)
		{
			char number[16];
			sprintf_s(number, "%s%02d", i == 0 ? " " : ", ", rrs.allowedRunways[i]);
			note.after += number;
		}
	}
	return note;
}
