// DisplayLogic.h: Texts and decisions for the display in the box.
//
// Contains everything that follows from the METAR evaluation (MetarAssessment) and the active runways
// but has nothing to do with Windows or EuroScope: the detail line, the wind lines per runway and the
// check for Reduced Runway Separation (RRS). This way everything can be tested without EuroScope.

#pragma once

#include "MetarParser.h"

#include <string>
#include <vector>

// Separator between items: middle dot (U+00B7), UTF-8 encoded (0xC2 0xB7). All texts of the box are
// UTF-8 and are drawn with the Unicode (W) GDI functions, so the dot looks the same on every system,
// independent of its ANSI code page. The literal is split so that the hex escape does not merge
// with the following character.
const char SEPARATOR[] = " \xC2\xB7" " ";

// Small line with the decisive values, e.g. "CEIL 1200 <dot> VIS 9999", "NO CEIL <dot> VIS 9999" or, if the ceiling
// causes LVO, "CEIL 100" (<dot> = SEPARATOR). The RVR is not part of it (it has its own rows, see BuildRvrLines).
// Empty if the state is unknown.
std::string BuildDetail(const MetarAssessment& a);

// Maximum number of RVR groups per line of the RVR rows
const int RVR_GROUPS_PER_LINE = 2;

// RVR rows: the RVR groups as written in the METAR (e.g. "R25L/P2000N"), at most RVR_GROUPS_PER_LINE per line,
// separated by two spaces. Only the groups of the landing runways are shown (the controller has to read
// them out to every landing aircraft); landingRunways are the designators of the runway ends that are active
// for arrival, e.g. "25L", "07C", "18". A group belongs to a landing runway if the number matches and the
// side letters (L/C/R) agree (a missing letter on either side matches any). If no landing runway is known or
// none of them has an RVR group, all RVR groups of the METAR are shown. Empty if the METAR has no RVR.
std::vector<std::string> BuildRvrLines(const MetarAssessment& a, const std::vector<std::string>& landingRunways);

// Wind lines, one per active runway number (runways, ascending), e.g. "RWY 25 HW: 5 kts <dot> CW: 3 kts".
// HW = headwind, TW = tailwind, CW = crosswind. The runway heading is the runway number * 10
// (runway 25 = 250 degrees). With gusts, the gust value is shown in brackets and "kts" only once at the
// end of the line: "RWY 18 TW: 13 (23) <dot> CW: 5 (8) kts". With a variable wind direction (VRB)
// nothing can be computed; then there is only one line with the wind. Without a wind group or without an
// active runway there are no lines.
//
// tailwindOverLimit receives true per line if the mean tailwind on the runway is above
// RRS_MAX_TAILWIND_KT; it stays empty if no components can be computed (no wind, VRB).
std::vector<std::string> BuildWindLines(const MetarAssessment& a, const std::vector<int>& runways,
	std::vector<bool>& tailwindOverLimit);

// Result of the Reduced Runway Separation check over all active runways
struct RrsResult
{
	FlightRules shown;					// state to display: yellow IMC becomes orange if RRS is possible on no runway
	bool weatherAllows;					// the weather allows RRS (yellow IMC, or VMC without SN/SG/PL/GR/GS)
	std::vector<int> allowedRunways;	// runways on which RRS is possible (empty if the weather does not allow RRS)
	bool allAllowed;					// RRS possible on all active runways (also without an active runway)
};

// As far as the weather is concerned, RRS is possible with yellow IMC and with VMC without SN, SG, PL, GR, GS,
// and then only on runways whose tailwind is at most RRS_MAX_TAILWIND_KT. If the wind component is unknown
// (no wind, VRB: tailwindOverLimit empty) or there is no active runway, only the weather decides.
// With yellow IMC the state becomes orange if RRS is possible on no runway; VMC stays VMC.
// With LVO, orange IMC and unknown state there is no RRS statement (allAllowed stays true).
RrsResult EvaluateRrs(const MetarAssessment& a, const std::vector<int>& runways,
	const std::vector<bool>& tailwindOverLimit);

// Note below the large text: normal, bold, normal (e.g. "RRS " + "NOT" + " POSSIBLE")
struct RrsNote
{
	bool present = false;
	std::string before;
	std::string bold;
	std::string after;
};

// Note on RRS matching the displayed state (rrs.shown):
//   yellow IMC: "RRS POSSIBLE", or like VMC if restricted
//   orange IMC: "RRS NOT POSSIBLE"
//   VMC: only if restricted, "RRS NOT POSSIBLE" or "RRS ONLY RWY 07, 18"
//   LVO and unknown: no note
RrsNote BuildRrsNote(const RrsResult& rrs);
