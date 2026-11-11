// DisplayLogic.cpp: Texts and decisions for the display in the box (see DisplayLogic.h)

#include "pch.h"
#include "DisplayLogic.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>

std::string BuildDetail(const MetarAssessment& a)
{
	std::string detail;
	char buffer[32];

	// LVO caused by the ceiling: only the ceiling is shown. (An RVR is shown in its own rows;
	// if LVO is caused by the RVR alone, the normal values below are shown.)
	bool ceilingCausesLvo = a.rules == FlightRules::Lvo &&
		(a.verticalVisibilityLost || (a.haveCeiling && a.ceilingFt < LVO_BELOW_CEILING_FT));
	if (ceilingCausesLvo)
	{
		if (a.verticalVisibilityLost)
		{
			detail = "CEIL ///";
		}
		else
		{
			sprintf_s(buffer, "CEIL %d", a.ceilingFt);
			detail = buffer;
		}
		return detail;
	}

	switch (a.rules)
	{
	case FlightRules::Lvo:
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

namespace
{
	// Splits a runway designator such as "25L", "07C" or "18" into number and side letter (0 = none).
	bool ParseRunwayDesignator(const std::string& name, int& number, char& side)
	{
		size_t i = 0;
		int value = 0;
		while (i < name.size() && isdigit((unsigned char)name[i]) && i < 2)
			value = value * 10 + (name[i++] - '0');
		if (i == 0 || value < 1 || value > 36)
			return false;

		number = value;
		side = 0;
		if (i < name.size())
		{
			char letter = (char)toupper((unsigned char)name[i]);
			if (letter == 'L' || letter == 'C' || letter == 'R')
				side = letter;
		}
		return true;
	}
}

std::vector<std::string> BuildRvrLines(const MetarAssessment& a, const std::vector<std::string>& landingRunways)
{
	std::vector<std::string> lines;
	if (a.rvrGroups.empty())
		return lines;

	// Runway ends that are active for arrival
	std::vector<RvrGroup> landing;
	for (size_t i = 0; i < landingRunways.size(); i++)
	{
		RvrGroup runway;
		if (ParseRunwayDesignator(landingRunways[i], runway.runway, runway.side))
			landing.push_back(runway);
	}

	// RVR groups of these runways; if there are none, all groups
	std::vector<std::string> selected;
	for (size_t g = 0; g < a.rvrGroups.size(); g++)
	{
		for (size_t r = 0; r < landing.size(); r++)
		{
			if (a.rvrGroups[g].runway == landing[r].runway &&
				(a.rvrGroups[g].side == 0 || landing[r].side == 0 || a.rvrGroups[g].side == landing[r].side))
			{
				selected.push_back(a.rvrGroups[g].text);
				break;
			}
		}
	}
	if (selected.empty())
	{
		for (size_t g = 0; g < a.rvrGroups.size(); g++)
			selected.push_back(a.rvrGroups[g].text);
	}

	for (size_t i = 0; i < selected.size(); i += RVR_GROUPS_PER_LINE)
	{
		std::string line = selected[i];
		for (size_t j = i + 1; j < selected.size() && j < i + RVR_GROUPS_PER_LINE; j++)
			line += "  " + selected[j];
		lines.push_back(line);
	}
	return lines;
}
