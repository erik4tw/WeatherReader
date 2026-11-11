// MetarParser.h: Determines the weather state of the control zone from a METAR.
//
// LVO (Low Visibility Operations) takes precedence over everything else and applies if
//   - the ceiling is below 200 ft (strictly below; VV001 yes, VV002 no; VV/// yes) OR
//   - the lowest RVR of all RVR groups is 600 m or less.
//   Ground visibility and phenomena such as FG do not matter for this.
//
// Rules (German stations only; values in metres / feet; all limits inclusive):
//   - Ceiling = lowest BKN, OVC or VV group (FEW/SCT do not count).
//     CAVOK, NCD, NSC (also SKC, CLR) mean: no ceiling.
//     Cloud groups with CB/TCU and unknown height (e.g. "FEW///TCU") are assumed to be at least 1500 ft.
//   - Visibility = first standalone 4-digit value (prevailing visibility); CAVOK = 9999.
//   - VMC: ceiling at least 1500 ft (or none) AND visibility at least 5000 m.
//   - Otherwise IMC. Yellow IMC (Reduced Runway Separation possible as far as the weather is concerned) if in addition:
//     visibility at least 5000 m, ceiling at least 1000 ft and no SN, SG, PL, GR or GS
//     in the current weather. Otherwise orange IMC. The tailwind per runway (at most
//     RRS_MAX_TAILWIND_KT) is checked in addition by the plugin, see DisplayLogic.h.
//   - Everything from BECMG, TEMPO, NOSIG, RMK on (trend/remarks) and recent weather (RE...) is ignored.

#pragma once

#include <string>
#include <vector>

enum class FlightRules
{
	Unknown,	// METAR empty or visibility/clouds cannot be evaluated
	Lvo,		// Low Visibility Operations (ceiling < 200 ft or RVR <= 600 m)
	VMC,
	ImcYellow,	// IMC, Reduced Runway Separation still allowed
	ImcOrange	// IMC, no Reduced Runway Separation
};

// One RVR group of the METAR, e.g. "R25L/P2000N"
struct RvrGroup
{
	std::string text;		// the group as written in the METAR (upper case)
	int  runway = 0;		// runway number (e.g. 25)
	char side = 0;			// 'L', 'C', 'R' or 0 (none)
};

// Result of evaluating a METAR: the state and the values it was derived from.
struct MetarAssessment
{
	FlightRules rules = FlightRules::Unknown;

	bool haveVisibility = false;	// visibility found
	int  visibilityM = 0;			// prevailing visibility in metres

	bool haveCeiling = false;		// false = no ceiling (or unknown height with CB/TCU)
	int  ceilingFt = 0;				// lowest BKN/OVC/VV group in feet

	bool verticalVisibilityLost = false;	// VV/// (vertical visibility not measurable)

	bool haveRvr = false;			// at least one RVR group found
	int  minRvrM = 0;				// lowest RVR in metres
	std::vector<RvrGroup> rvrGroups;	// all valid RVR groups in the order of the METAR

	char wintryCode[3] = { 0, 0, 0 };	// first SN/SG/PL/GR/GS found in the current weather, otherwise empty

	bool haveObsTime = false;		// observation time (group ddhhmmZ) found
	int  obsHour = 0;				// UTC hour of the METAR
	int  obsMinute = 0;				// UTC minute of the METAR

	bool haveWind = false;			// wind group (e.g. 25008KT, VRB03KT) found
	bool windVariable = false;		// VRB: no fixed wind direction
	int  windDirDeg = 0;			// wind direction in degrees (direction the wind comes from)
	int  windSpeedKt = 0;			// mean wind in knots
	int  windGustKt = 0;			// gusts in knots, 0 = none
	std::string windGroup;			// the main wind group as written in the METAR (e.g. "25024G35KT", "VRB05KT"); empty = none

	std::string qnh;				// pressure group as written in the METAR (e.g. "Q1019", or "A2992" in inches); empty = none
};

// Headwind/tailwind and crosswind component for a runway.
struct WindComponents
{
	int headwindKt;		// positive = headwind, negative = tailwind
	int crosswindKt;	// magnitude of the crosswind component
};

// Computes the wind components for the runway with heading runwayHeadingDeg (e.g. 250 for runway 25).
// The wind comes from windDirDeg with windSpeedKt knots; the results are rounded to whole knots.
WindComponents ComputeWindComponents(int windDirDeg, int windSpeedKt, int runwayHeadingDeg);

// Evaluates a complete METAR (e.g. "EDDF 061820Z AUTO 21003KT 9999 NCD 04/02 Q1023 NOSIG").
MetarAssessment AssessMetar(const char* sMetar);

// Short form: the state only
FlightRules DetermineFlightRules(const char* sMetar);

// Limits for LVO: ceiling below, RVR at most
const int LVO_BELOW_CEILING_FT = 200;
const int LVO_MAX_RVR_M = 600;

// Minimum values for VMC
const int VMC_MIN_CEILING_FT = 1500;
const int VMC_MIN_VISIBILITY_M = 5000;

// Highest tailwind (mean wind, in knots, inclusive) at which Reduced Runway Separation is allowed on a runway
const int RRS_MAX_TAILWIND_KT = 5;

// Minimum values for Reduced Runway Separation (yellow IMC)
const int REDUCED_MIN_CEILING_FT = 1000;
const int REDUCED_MIN_VISIBILITY_M = 5000;
