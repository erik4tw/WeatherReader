// WeatherReaderTests.cpp: Tests for the METAR evaluation and the texts of the box.
//
// The program needs neither EuroScope nor MFC. It includes the source files MetarParser.cpp,
// DisplayLogic.cpp and MeepSound.cpp directly and runs as a simple console program:
//
//   Visual Studio: open a "Developer Command Prompt for VS", change to this folder and
//                  run run_tests.bat.
//
// Return value 0 = all tests passed. To add a test case, just append it to the matching group below.

#define PCH_H		// prevents the source files from loading the MFC header pch.h

#ifndef _MSC_VER		// sprintf_s only exists in the Microsoft compiler; replacement for other compilers (e.g. g++)
#include <cstdarg>
#include <cstdio>
template <size_t N> int sprintf_s(char (&buffer)[N], const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vsnprintf(buffer, N, format, args);
	va_end(args);
	return result;
}
#endif

#include "../MetarParser.cpp"
#include "../DisplayLogic.cpp"
#include "../MeepSound.cpp"
#include "../AtisParser.cpp"

#include <cstdio>
#include <cstring>

namespace
{
	int g_tests = 0;
	int g_failures = 0;

	void CheckTrue(bool condition, const char* expression, int line)
	{
		g_tests++;
		if (!condition)
		{
			g_failures++;
			printf("FAILURE line %d: %s\n", line, expression);
		}
	}

	void CheckText(const std::string& actual, const std::string& expected, int line)
	{
		g_tests++;
		if (actual != expected)
		{
			g_failures++;
			printf("FAILURE line %d:\n   expected: [%s]\n   actual:   [%s]\n", line, expected.c_str(), actual.c_str());
		}
	}

	void CheckInt(int actual, int expected, int line)
	{
		g_tests++;
		if (actual != expected)
		{
			g_failures++;
			printf("FAILURE line %d: expected %d, actual %d\n", line, expected, actual);
		}
	}
}

#define CHECK(cond)          CheckTrue((cond), #cond, __LINE__)
#define CHECK_TEXT(a, e)     CheckText((a), (e), __LINE__)
#define CHECK_INT(a, e)      CheckInt((a), (e), __LINE__)

// Abbreviations for the table
#define V FlightRules::VMC
#define Y FlightRules::ImcYellow
#define O FlightRules::ImcOrange
#define U FlightRules::Unknown
#define L FlightRules::Lvo

// ---------------------------------------------------------------------
// State (VMC / IMC yellow / IMC orange / LVO) from the METAR
// ---------------------------------------------------------------------

static void TestFlightRules()
{
	struct Case { const char* metar; FlightRules expected; };
	Case cases[] = {
 // --- the first 15 METARs (VMC/IMC, now with yellow/orange)
 {"EDDF 061820Z AUTO 21003KT 170V240 9999 0550 R07L/P2000N R07C/P2000N R07R/P2000N R18/P2000N BCFG NCD 04/02 Q1023 NOSIG",V},
 {"EDDF 061850Z AUTO VRB02KT 9999 1800 BR NCD 04/02 Q1023 NOSIG",V},
 {"EDDF 061920Z AUTO 19003KT 150V220 9999 0400 R07L/P2000N R07C/P2000N R07R/P2000N R18/P2000N BCFG NCD 04/02 Q1023 NOSIG",V},
 {"EDDF 062220Z AUTO VRB01KT 9000 3600 BR NSC M00/M01 Q1024 NOSIG",V},
 {"EDDF 062150Z AUTO VRB01KT 7000 NSC 00/M01 Q1024 NOSIG",V},
 {"EDDS 061620Z AUTO 07003KT 050V110 9999 OVC011 04/02 Q1021 BECMG OVC009",Y},
 {"EDDS 061250Z AUTO 04003KT 010V080 6000 BKN007 OVC010 04/03 Q1021 NOSIG",O},
 {"EDDS 060350Z AUTO 00000KT 0450 0400 R25/0650U FZFG FEW001 SCT009 BKN016 M02/M03 Q1021",O},
 {"EDDS 060220Z AUTO 18001KT 7000 NCD M01/M02 Q1022",V},
 {"EDDM 070050Z AUTO 05003KT 020V080 7000 FEW010 OVC017 03/02 Q1023 TEMPO 4000 BR BKN009 OVC015",V},
 {"EDDM 052350Z AUTO 05004KT 5000 0350 R08L/P2000N R08R/P2000N BCFG NCD M01/M01 Q1023 BECMG OVC008",V},
 {"EDDM 052320Z AUTO VRB01KT 9999 2700 BR NCD M02/M03 Q1023 BECMG FM0000 OVC008",V},
 {"EDDM 060050Z AUTO 06003KT 020V110 7000 FEW012 M01/M01 Q1023 BECMG OVC009",V},
 {"EDDM 060120Z AUTO 04002KT 7000 3400 BR BKN010 M01/M01 Q1023 BECMG OVC008",Y},
 {"EDDM 061750Z AUTO 10004KT 9999 SCT008 OVC016 03/02 Q1023 NOSIG",V},
 // --- the 8 new METARs
 {"EDDM 251250Z AUTO VRB02KT 0800 0500 R26R/0800N R26L/1500U FZFG OVC002 M03/M03 Q0996 TEMPO 0600",O},
 {"EDDM 252050Z AUTO 24005KT 3200 -SN BR FEW007 SCT040 M03/M03 Q0994 TEMPO 2000 SN BKN006",O},
 {"EDDM 251750Z AUTO 28003KT 3300 -SN BR BKN008 BKN018 M02/M03 Q0994 TEMPO 2000",O},
 {"EDDM 131220Z AUTO 20004KT 9999 -SHRA FEW///TCU 07/04 Q1016 NOSIG",V},
 {"EDDM 121750Z AUTO VRB01KT 3100 SHRA BR SCT002 OVC020 FEW///CB 01/01 Q1015 TEMPO 22005KT 6000 RA",O},
 {"EDDM 121250Z AUTO 13003KT 100V170 9000 -SHRA BKN014 BKN039 BKN046 FEW///CB 00/M01 Q1016 RESHRA BECMG NSW",Y},
 {"EDDM 121020Z AUTO 11004KT 080V140 6000 -FZDZ BKN009 BKN018 BKN026 M03/M04 Q1018 TEMPO 4000 SCT009 BKN015",O},
 {"EDDM 120950Z AUTO 12005KT 6000 -FZRADZ FEW009 BKN029 BKN037 M03/M05 Q1018 RESN TEMPO 4000",V},
 // --- edge cases yellow/orange (ceiling 1200 ft, visibility 9999)
 {"EDDM 121250Z AUTO 13003KT 9999 BKN012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 -SN BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 SN BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 +SN BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 -SHSN BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 +TSGR BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 -SHRASN BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 RASN BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 BLSN BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 SG BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 PL BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 GR BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 GS BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 RA BKN012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 -DZ BKN012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 FZRA BKN012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 FZFG BKN012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 UP BKN012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 RESN BKN012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 BKN012 00/M01 Q1016 NOSIG TEMPO SN",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 BKN012 00/M01 Q1016 BECMG 3000 SN BKN004",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 BKN012 FEW///CB 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 BKN010 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 9999 BKN009 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 VV009 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 VV012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 5000 BKN012 00/M01 Q1016",Y},
 {"EDDM 121250Z AUTO 13003KT 4999 BKN012 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 4999 NCD 00/M01 Q1016",O},
 {"EDDM 121250Z AUTO 13003KT 9999 SN BKN020 00/M01 Q1016",V},	// VMC stays green, even with snow
 // --- clouds without height
 {"EDDM 131220Z AUTO 20004KT 9999 FEW///TCU 07/04 Q1016",V},
 {"EDDM 131220Z AUTO 20004KT 4000 FEW///TCU 07/04 Q1016",O},
 {"EDDM 131220Z AUTO 20004KT 9999 BKN///CB 07/04 Q1016",V},
 {"EDDM 131220Z AUTO 20004KT 9999 OVC///TCU 07/04 Q1016",V},
 {"EDDM 131220Z AUTO 20004KT 9999 BKN012 BKN///CB 07/04 Q1016",Y},
 {"EDDM 131220Z AUTO 20004KT 9999 BKN/// 07/04 Q1016",U},
 {"EDDM 131220Z AUTO 20004KT 9999 FEW/// 07/04 Q1016",V},
 {"EDDM 131220Z AUTO 20004KT 9999 VV/// 07/04 Q1016",L},

 // --- the 17 LVO METARs
 {"EKCH 050820Z 24006KT 1200 R22L/P1500U R04L/0900U R12/P1500U BCFG SCT002 05/05 Q1033 BECMG 5000",O},
 {"EKCH 050650Z 24003KT 0400 R22L/1200U R04L/0550D R12/0600U FG VV002 02/02 Q1034 BECMG 1200",L},
 {"EDDF 210820Z AUTO VRB02KT 2200 0400 R25R/P2000N R25C/P2000N R25L/1000U R18/1800U BCFG SCT002 M05/M06 Q1010 TEMPO 1000",O},
 {"EDDF 210720Z AUTO VRB02KT 0400 0350 R25R/1300D R25C/0500N R25L/1000U R18/0500N FZFG VV002 M06/M06 Q1010 NOSIG",L},
 {"EDDF 210620Z AUTO 23003KT 0600 0450 R25R/1500D R25C/0800U R25L/0650N R18/1000U FZFG VV003 M06/M06 Q1011 NOSIG",O},
 {"EDDM 301420Z AUTO VRB02KT 0400 0250 R08L/0700U R08R/1200D FG VV002 00/00 Q1000 TEMPO 1200 BR",O},
 {"EDDM 302050Z AUTO 09002KT 0450 0400 R08L/0700N R08R/0500N FZFG VV003 M03/M03 Q1001 NOSIG",L},
 {"EDDM 181720Z AUTO 27003KT 6000 NCD M03/M03 Q1019 TEMPO 3000 BR BKN005",V},
 {"EDDM 170820Z AUTO 08005KT 1300 1100 R08L/1800N R08R/1500N BR OVC002 M02/M02 Q1020 TEMPO 0800 FZFG",O},
 {"EDDM 170750Z AUTO 09004KT 1100 0900 R08L/1900N R08R/1200N BCFG OVC001 M02/M02 Q1020 TEMPO 0800 FZFG",L},
 {"EDDM 121350Z AUTO 14004KT 110V170 2600 -RA BR FEW006 BKN012 OVC019 00/M00 Q1015 BECMG 20006KT 6000 RA SCT012 BKN030",O},
 {"EDDM 112220Z AUTO 07001KT 0800 0400 R26R/1200D R26L/1900N FZFG NCD M11/M11 Q1024 NOSIG",O},
 {"EDDC 150250Z AUTO 15005KT 5000 0750 R22/P2000N BCFG FEW001 BKN039 BKN045 03/03 Q1016",V},
 {"EDDC 150120Z AUTO VRB01KT 3200 0550 R22/P2000N BCFG FEW001 BKN012 BKN037 03/03 Q1016",O},
 {"EDDC 130550Z AUTO 28005KT 240V310 1300 0900 R22/P2000N BCFG BKN001 OVC005 02/02 Q1011 TEMPO 1500 BR BKN003",L},
 {"EDDC 142050Z AUTO 18003KT 0800 0650 R22/1000N FG FEW001 OVC013 04/04 Q1016 TEMPO 0300 FG OVC001",O},
 {"EDDC 141920Z AUTO 22004KT 0900 0600 R22/1100N FG BKN002 OVC016 05/05 Q1016 NOSIG",O},
 // --- own LVO edge cases
 {"EDDM 112220Z AUTO 07001KT 0800 R26R/0600N NCD M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0800 R26R/0601N NCD M11/M11 Q1024",O},
 {"EDDM 112220Z AUTO 07001KT 0800 R26R/M0050N NCD M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0800 R26R/P2000N NCD M11/M11 Q1024",O},
 {"EDDM 112220Z AUTO 07001KT 0800 R26R/0400V0800N NCD M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0800 R26R/0800V0400D NCD M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0800 R26R/0700V1200 NCD M11/M11 Q1024",O},
 {"EDDM 112220Z AUTO 07001KT 0800 R26R/0500 NCD M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0800 R26L///// NCD M11/M11 Q1024",O},
 {"EDDM 112220Z AUTO 07001KT 0800 R26L/////N NCD M11/M11 Q1024",O},
 {"EDDM 112220Z AUTO 07001KT 0800 R26L/////  R26R/0300N NCD M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0800 R26L/1500N R26R/0300N R08L/0900N NCD M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0800 R26L/1500N NCD M11/M11 Q1024 TEMPO R26L/0300N",O},
 {"EDDM 112220Z AUTO 07001KT 0800 R26L/1500N NCD M11/M11 Q1024 RMK R26L/0300N",O},
 {"EDDM 112220Z AUTO 07001KT 9999 VV001 M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 9999 VV002 M11/M11 Q1024",O},
 {"EDDM 112220Z AUTO 07001KT 9999 BKN002 M11/M11 Q1024",O},
 {"EDDM 112220Z AUTO 07001KT 9999 BKN001 M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 9999 OVC001 M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 9999 FEW001 SCT001 BKN015 M11/M11 Q1024",V},
 {"EDDM 112220Z AUTO 07001KT 9999 VV/// M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT VV/// M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0300 R26R/0500N M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 0300 FG NCD M11/M11 Q1024",O},
 {"EDDM 112220Z AUTO 07001KT 9999 BKN///CB M11/M11 Q1024",V},
 {"EDDM 112220Z AUTO 07001KT 9999 BKN/// R26R/0500N M11/M11 Q1024",L},
 {"EDDM 112220Z AUTO 07001KT 9999 BKN/// M11/M11 Q1024",U},
 {"EDDM 112220Z AUTO 07001KT 9999 NCD R26R/0600N M11/M11 Q1024 NOSIG",L},
 {"EDDM 112220Z AUTO 07001KT 9999 NCD R26R/0601N M11/M11 Q1024 NOSIG",V},
 {"EDDM 112220Z AUTO 07001KT SN NCD R26R/0601N M11/M11 Q1024",U},
 {"edmm 112220z auto 07001kt 0800 r26r/0500n ncd",L},
 // --- miscellaneous
 {"EDDF 061820Z 21003KT CAVOK 04/02 Q1023 NOSIG",V},
 {"EDDF 061820Z 21003KT 0200 FG VV002 04/04 Q1023",O},
 {"METAR EDDF 061820Z AUTO 21003KT 9999 NCD 04/02 Q1023 NOSIG",V},
 {"EDDF 061820Z AUTO 21003KT 9999 NCD 04/02 Q1023 RMK BKN005",V},
 {"eddf 061820z auto 21003kt 9999 bkn012 -sn 04/02 q1023",O},
 {"EDDF 061820Z AUTO 21003KT 04/02 Q1023 NOSIG",U},
 {"EDDF 061820Z AUTO 21003KT 9999 04/02 Q1023 NOSIG",U},
 {"",U},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		FlightRules actual = DetermineFlightRules(cases[i].metar);
		g_tests++;
		if (actual != cases[i].expected)
		{
			g_failures++;
			printf("FAILURE state case %d: %s -> %d (expected %d)\n", (int)i + 1, cases[i].metar, (int)actual, (int)cases[i].expected);
		}
	}

	CHECK(DetermineFlightRules(nullptr) == FlightRules::Unknown);
}

// ---------------------------------------------------------------------
// Evaluated values (visibility, ceiling, RVR, wintry weather)
// ---------------------------------------------------------------------

static void TestAssessedValues()
{
	MetarAssessment a;

	a = AssessMetar("EDDM 121250Z AUTO 13003KT 100V170 9000 -SHRA BKN014 BKN039 BKN046 FEW///CB 00/M01 Q1016 RESHRA BECMG NSW");
	CHECK(a.rules == FlightRules::ImcYellow);
	CHECK(a.haveVisibility && a.visibilityM == 9000);
	CHECK(a.haveCeiling && a.ceilingFt == 1400);
	CHECK(!a.haveRvr);
	CHECK(a.wintryCode[0] == 0);
	CHECK(!a.verticalVisibilityLost);

	a = AssessMetar("EDDM 121250Z AUTO 13003KT 9999 -SN BKN012 00/M01 Q1016");
	CHECK(a.rules == FlightRules::ImcOrange);
	CHECK(strcmp(a.wintryCode, "SN") == 0);

	a = AssessMetar("EDDM 121250Z AUTO 13003KT 9999 +TSGR BKN012 00/M01 Q1016");
	CHECK(strcmp(a.wintryCode, "GR") == 0);

	a = AssessMetar("EDDM 121250Z AUTO 13003KT 9999 RASN BKN012 00/M01 Q1016");
	CHECK(strcmp(a.wintryCode, "SN") == 0);

	a = AssessMetar("EDDM 121250Z AUTO 13003KT 9999 RESN BKN012 00/M01 Q1016");	// recent weather does not count
	CHECK(a.wintryCode[0] == 0);

	a = AssessMetar("EDDM 112220Z AUTO 07001KT 0800 0400 R26R/1200D R26L/0550N NCD M11/M11 Q1024");
	CHECK(a.rules == FlightRules::Lvo);
	CHECK(a.haveRvr && a.minRvrM == 550);
	CHECK(!a.haveCeiling);
	CHECK(a.haveVisibility && a.visibilityM == 800);

	a = AssessMetar("EDDM 112220Z AUTO 07001KT 9999 VV/// M11/M11 Q1024");
	CHECK(a.rules == FlightRules::Lvo);
	CHECK(a.verticalVisibilityLost);

	a = AssessMetar("EDDM 112220Z AUTO 07001KT 9999 VV001 M11/M11 Q1024");
	CHECK(a.rules == FlightRules::Lvo);
	CHECK(a.haveCeiling && a.ceilingFt == 100);
	CHECK(!a.verticalVisibilityLost);

	a = AssessMetar("EDDM 121250Z AUTO 13003KT 9999 NCD 00/M01 Q1016");
	CHECK(a.rules == FlightRules::VMC);
	CHECK(!a.haveCeiling);
	CHECK(a.visibilityM == 9999);

	a = AssessMetar(nullptr);
	CHECK(a.rules == FlightRules::Unknown);
	CHECK(!a.haveVisibility);

	a = AssessMetar("");
	CHECK(a.rules == FlightRules::Unknown);
}

// ---------------------------------------------------------------------
// Time of the METAR
// ---------------------------------------------------------------------

static void TestObservationTime()
{
	MetarAssessment a;

	a = AssessMetar("EDDF 081420Z 21005KT 9999 NCD");
	CHECK(a.haveObsTime && a.obsHour == 14 && a.obsMinute == 20);

	a = AssessMetar("METAR EDDF 082359Z AUTO 21005KT 9999 NCD");
	CHECK(a.haveObsTime && a.obsHour == 23 && a.obsMinute == 59);

	a = AssessMetar("EDDF 080000Z 21005KT 9999 NCD");
	CHECK(a.haveObsTime && a.obsHour == 0 && a.obsMinute == 0);

	a = AssessMetar("eddf 081420z 21005kt 9999 ncd");
	CHECK(a.haveObsTime && a.obsHour == 14 && a.obsMinute == 20);

	a = AssessMetar("EDDF 082460Z 21005KT 9999 NCD");			// invalid time
	CHECK(!a.haveObsTime);

	a = AssessMetar("EDDF 21005KT 9999 NCD");					// no time
	CHECK(!a.haveObsTime);

	a = AssessMetar("EDDF 21005KT 9999 NCD BECMG 1200Z 081500Z");	// nothing counts after the trend
	CHECK(!a.haveObsTime);
}

// ---------------------------------------------------------------------
// Wind group
// ---------------------------------------------------------------------

static void TestWindParsing()
{
	MetarAssessment a;

	a = AssessMetar("EDDF 081420Z 25008KT 9999 NCD");
	CHECK(a.haveWind && !a.windVariable && a.windDirDeg == 250 && a.windSpeedKt == 8 && a.windGustKt == 0);

	a = AssessMetar("EDDF 081420Z 25008G18KT 9999 NCD");
	CHECK(a.haveWind && a.windDirDeg == 250 && a.windSpeedKt == 8 && a.windGustKt == 18);

	a = AssessMetar("EDDF 081420Z VRB03KT 9999 NCD");
	CHECK(a.haveWind && a.windVariable && a.windSpeedKt == 3);

	a = AssessMetar("EDDF 081420Z VRB03G15KT 9999 NCD");
	CHECK(a.haveWind && a.windVariable && a.windSpeedKt == 3 && a.windGustKt == 15);

	a = AssessMetar("EDDF 081420Z 00000KT 9999 NCD");
	CHECK(a.haveWind && !a.windVariable && a.windDirDeg == 0 && a.windSpeedKt == 0);

	a = AssessMetar("EDDF 081420Z 250100KT 9999 NCD");			// three-digit speed
	CHECK(a.haveWind && a.windSpeedKt == 100);

	a = AssessMetar("eddf 081420z 25008g18kt 9999 ncd");
	CHECK(a.haveWind && a.windSpeedKt == 8 && a.windGustKt == 18);

	a = AssessMetar("EDDF 081420Z 25008KT 220V290 9999 NCD");	// varying direction is ignored
	CHECK(a.haveWind && a.windDirDeg == 250 && a.windSpeedKt == 8);

	a = AssessMetar("EDDF 081420Z 9999 NCD");					// no wind group
	CHECK(!a.haveWind);

	a = AssessMetar("EDDF 081420Z /////KT 9999 NCD");			// wind not available
	CHECK(!a.haveWind);

	a = AssessMetar("EDDF 081420Z 18010KT 9999 NCD BECMG 25015KT");	// only the wind before the trend
	CHECK(a.haveWind && a.windDirDeg == 180 && a.windSpeedKt == 10);
}

// ---------------------------------------------------------------------
// Wind group and QNH as written in the METAR
// ---------------------------------------------------------------------

static void TestWindGroupAndQnh()
{
	MetarAssessment a;

	a = AssessMetar("EDDF 081420Z 25024G35KT 9999 NCD 04/02 Q1019 NOSIG");
	CHECK_TEXT(a.windGroup, "25024G35KT");
	CHECK_TEXT(a.qnh, "Q1019");

	a = AssessMetar("EDDF 081420Z 09010KT 060V130 9999 NCD 04/02 Q1013");	// only the main wind group
	CHECK_TEXT(a.windGroup, "09010KT");

	a = AssessMetar("EDDF 081420Z VRB05KT 9999 NCD 04/02 Q0998");
	CHECK_TEXT(a.windGroup, "VRB05KT");
	CHECK_TEXT(a.qnh, "Q0998");

	a = AssessMetar("eddf 081420z 25008kt 9999 ncd q1019");		// lower case input
	CHECK_TEXT(a.windGroup, "25008KT");
	CHECK_TEXT(a.qnh, "Q1019");

	a = AssessMetar("KJFK 081420Z 25008KT 9999 NCD A2992");		// altimeter in inches
	CHECK_TEXT(a.qnh, "A2992");

	a = AssessMetar("EDDF 081420Z 9999 NCD 04/02 Q////");		// nothing available
	CHECK_TEXT(a.windGroup, "");
	CHECK_TEXT(a.qnh, "");

	a = AssessMetar("EDDF 081420Z 18010KT 9999 NCD Q1019 BECMG 25015KT Q1000");	// only values before the trend
	CHECK_TEXT(a.windGroup, "18010KT");
	CHECK_TEXT(a.qnh, "Q1019");

	a = AssessMetar("");
	CHECK_TEXT(a.windGroup, "");
	CHECK_TEXT(a.qnh, "");
}

// ---------------------------------------------------------------------
// Wind components (headwind HW positive, tailwind negative)
// ---------------------------------------------------------------------

static void TestWindComponents()
{
	struct Case { int dir, speed, runway, head, cross; };
	Case cases[] = {
		{ 250,  8, 250,   8, 0 },	// pure headwind
		{ 250,  8,  70,  -8, 0 },	// pure tailwind
		{ 250,  8, 340,   0, 8 },	// pure crosswind
		{ 250,  8, 160,   0, 8 },
		{ 200, 10, 250,   6, 8 },
		{ 200, 10, 180,   9, 3 },
		{ 360, 10, 360,  10, 0 },
		{  10, 10, 350,   9, 3 },	// across north
		{ 340, 14, 180, -13, 5 },
		{ 340, 24, 180, -23, 8 },
		{   0,  0, 250,   0, 0 },	// calm wind
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		WindComponents w = ComputeWindComponents(cases[i].dir, cases[i].speed, cases[i].runway);
		g_tests++;
		if (w.headwindKt != cases[i].head || w.crosswindKt != cases[i].cross)
		{
			g_failures++;
			printf("FAILURE wind components case %d: wind %03d/%d, runway %03d -> HW %d CW %d (expected %d / %d)\n",
				(int)i + 1, cases[i].dir, cases[i].speed, cases[i].runway,
				w.headwindKt, w.crosswindKt, cases[i].head, cases[i].cross);
		}
	}
}

// ---------------------------------------------------------------------
// Wind lines per runway
// ---------------------------------------------------------------------

static std::vector<int> Runways(int a = 0, int b = 0, int c = 0)
{
	std::vector<int> runways;
	if (a) runways.push_back(a);
	if (b) runways.push_back(b);
	if (c) runways.push_back(c);
	return runways;
}

static void TestWindLines()
{
	std::vector<bool> over;
	std::vector<std::string> lines;
	std::string dot = SEPARATOR;

	// several runways, without gusts
	lines = BuildWindLines(AssessMetar("EDDF 081420Z 25008KT 9999 NCD"), Runways(7, 18, 25), over);
	CHECK_INT((int)lines.size(), 3);
	CHECK_TEXT(lines[0], "RWY 07 TW: 8 kts" + dot + "CW: 0 kts");
	CHECK_TEXT(lines[1], "RWY 18 HW: 3 kts" + dot + "CW: 8 kts");
	CHECK_TEXT(lines[2], "RWY 25 HW: 8 kts" + dot + "CW: 0 kts");
	CHECK(over.size() == 3 && over[0] && !over[1] && !over[2]);	// only 07 has more than 5 kt tailwind

	// limit: exactly 5 kt tailwind is allowed, 6 kt is not
	BuildWindLines(AssessMetar("EDDF 081420Z 25005KT 9999 NCD"), Runways(7), over);
	CHECK(over.size() == 1 && !over[0]);
	BuildWindLines(AssessMetar("EDDF 081420Z 25006KT 9999 NCD"), Runways(7), over);
	CHECK(over.size() == 1 && over[0]);

	// gusts: gust value in brackets, "kts" only once at the end
	lines = BuildWindLines(AssessMetar("EDDF 101650Z 34014G24KT 300V360 CAVOK 32/09"), Runways(18, 25), over);
	CHECK_INT((int)lines.size(), 2);
	CHECK_TEXT(lines[0], "RWY 18 TW: 13 (23)" + dot + "CW: 5 (8) kts");
	CHECK_TEXT(lines[1], "RWY 25 HW: 0" + dot + "CW: 14 (24) kts");
	CHECK(over.size() == 2 && over[0] && !over[1]);

	// gusts do not count for the tailwind limit
	BuildWindLines(AssessMetar("EDDF 081420Z 25004G20KT 6000 BKN012"), Runways(7), over);
	CHECK(over.size() == 1 && !over[0]);

	// gust not greater than the mean wind: normal format
	lines = BuildWindLines(AssessMetar("EDDF 081420Z 25008G08KT 9999 NCD"), Runways(25), over);
	CHECK_TEXT(lines[0], "RWY 25 HW: 8 kts" + dot + "CW: 0 kts");

	// calm wind
	lines = BuildWindLines(AssessMetar("EDDF 081420Z 00000KT 9999 NCD"), Runways(25), over);
	CHECK_TEXT(lines[0], "RWY 25 HW: 0 kts" + dot + "CW: 0 kts");

	// variable wind direction: one line, no components
	lines = BuildWindLines(AssessMetar("EDDF 081420Z VRB03KT 9999 NCD"), Runways(18, 25), over);
	CHECK_INT((int)lines.size(), 1);
	CHECK_TEXT(lines[0], "WIND VRB 3 kts");
	CHECK(over.empty());

	lines = BuildWindLines(AssessMetar("EDDF 081420Z VRB03G15KT 9999 NCD"), Runways(25), over);
	CHECK_TEXT(lines[0], "WIND VRB 3 (15) kts");

	// no wind in the METAR or no active runway: no lines
	lines = BuildWindLines(AssessMetar("EDDF 081420Z 9999 NCD"), Runways(25), over);
	CHECK(lines.empty() && over.empty());
	lines = BuildWindLines(AssessMetar("EDDF 081420Z 25008KT 9999 NCD"), Runways(), over);
	CHECK(lines.empty() && over.empty());
}

// ---------------------------------------------------------------------
// Reduced Runway Separation and its note
// ---------------------------------------------------------------------

// Fully evaluates METAR and active runways and returns the note text (empty = no note)
static std::string RrsText(const char* metar, const std::vector<int>& runways, FlightRules* shown = nullptr)
{
	MetarAssessment a = AssessMetar(metar);
	std::vector<bool> over;
	BuildWindLines(a, runways, over);
	RrsResult rrs = EvaluateRrs(a, runways, over);
	if (shown != nullptr)
		*shown = rrs.shown;

	RrsNote note = BuildRrsNote(rrs);
	return note.present ? note.before + "|" + note.bold + "|" + note.after : "";
}

static void TestRrs()
{
	FlightRules shown;

	// yellow IMC (visibility 6000, ceiling 1200): RRS on all runways if the tailwind is at most 5 kt
	CHECK_TEXT(RrsText("EDDF 081420Z 25005KT 6000 BKN012", Runways(7, 25), &shown), "RRS ||POSSIBLE");
	CHECK(shown == FlightRules::ImcYellow);

	// 07 has 6 kt tailwind: only 25 remains
	CHECK_TEXT(RrsText("EDDF 081420Z 25006KT 6000 BKN012", Runways(7, 25), &shown), "RRS |ONLY| RWY 25");
	CHECK(shown == FlightRules::ImcYellow);

	// only 07 active and too much tailwind: orange IMC
	CHECK_TEXT(RrsText("EDDF 081420Z 25008KT 6000 BKN012", Runways(7), &shown), "RRS |NOT| POSSIBLE");
	CHECK(shown == FlightRules::ImcOrange);

	// several allowed runways
	CHECK_TEXT(RrsText("EDDF 081420Z 07008KT 6000 BKN012", Runways(7, 18, 25), &shown), "RRS |ONLY| RWY 07, 18");
	CHECK(shown == FlightRules::ImcYellow);

	// without active runway, without wind or with VRB only the weather decides
	CHECK_TEXT(RrsText("EDDF 081420Z 25008KT 6000 BKN012", Runways(), &shown), "RRS ||POSSIBLE");
	CHECK(shown == FlightRules::ImcYellow);
	CHECK_TEXT(RrsText("EDDF 081420Z 6000 BKN012", Runways(7), &shown), "RRS ||POSSIBLE");
	CHECK_TEXT(RrsText("EDDF 081420Z VRB08KT 6000 BKN012", Runways(7), &shown), "RRS ||POSSIBLE");

	// weather does not allow RRS (orange IMC): always "NOT POSSIBLE", regardless of the wind
	CHECK_TEXT(RrsText("EDDF 081420Z 25008KT 3000 BKN012", Runways(7, 25), &shown), "RRS |NOT| POSSIBLE");
	CHECK(shown == FlightRules::ImcOrange);
	CHECK_TEXT(RrsText("EDDF 081420Z 25008KT 6000 -SN BKN012", Runways(25), &shown), "RRS |NOT| POSSIBLE");

	// VMC: without restriction no note, the display stays VMC
	CHECK_TEXT(RrsText("EDDF 101650Z 25008KT CAVOK", Runways(18, 25), &shown), "");
	CHECK(shown == FlightRules::VMC);
	CHECK_TEXT(RrsText("EDDF 101650Z VRB05KT CAVOK", Runways(7), &shown), "");
	CHECK_TEXT(RrsText("EDDF 101650Z 25008KT 9000 -RA FEW040", Runways(25), &shown), "");

	// VMC with too much tailwind on some of the runways or on all of them
	CHECK_TEXT(RrsText("EDDF 101650Z AUTO 34014G24KT 300V360 CAVOK 32/09 Q1014 BECMG 32009KT", Runways(18, 25), &shown), "RRS |ONLY| RWY 25");
	CHECK(shown == FlightRules::VMC);
	CHECK_TEXT(RrsText("EDDF 101650Z 34014G24KT CAVOK", Runways(18), &shown), "RRS |NOT| POSSIBLE");
	CHECK(shown == FlightRules::VMC);

	// VMC with SN, SG, PL, GR or GS: RRS not possible, even without wind
	CHECK_TEXT(RrsText("EDDF 101650Z 25008KT 9000 -SN FEW040", Runways(25), &shown), "RRS |NOT| POSSIBLE");
	CHECK(shown == FlightRules::VMC);
	CHECK_TEXT(RrsText("EDDF 101650Z 9000 GR FEW040", Runways(25), &shown), "RRS |NOT| POSSIBLE");

	// LVO and unknown: no RRS statement
	CHECK_TEXT(RrsText("EDDF 101650Z 25008KT 0500 FG VV001", Runways(7), &shown), "");
	CHECK(shown == FlightRules::Lvo);
	CHECK_TEXT(RrsText("EDDF 101650Z 25008KT 04/02", Runways(7), &shown), "");
	CHECK(shown == FlightRules::Unknown);
}

// ---------------------------------------------------------------------
// Detail line at the bottom of the box
// ---------------------------------------------------------------------

static void TestDetail()
{
	std::string dot = SEPARATOR;

	// the separator is the middle dot U+00B7 in UTF-8 (the box converts its texts from UTF-8 to UTF-16)
	CHECK_TEXT(dot, " \xC2\xB7 ");

	CHECK_TEXT(BuildDetail(AssessMetar("EDDM 121250Z AUTO 13003KT 100V170 9000 -SHRA BKN014 BKN039 BKN046 FEW///CB 00/M01 Q1016 RESHRA BECMG NSW")),
		"CEIL 1400" + dot + "VIS 9000");
	CHECK_TEXT(BuildDetail(AssessMetar("EDDM 121250Z AUTO 13003KT 9999 -SN BKN012 00/M01 Q1016")),
		"CEIL 1200" + dot + "VIS 9999" + dot + "SN");
	CHECK_TEXT(BuildDetail(AssessMetar("EDDM 112220Z AUTO 07001KT 0800 0400 R26R/1200D R26L/0550N NCD M11/M11 Q1024")),
		"NO CEIL" + dot + "VIS 800");		// LVO caused by the RVR alone: the RVR is not part of the detail line
	CHECK_TEXT(BuildDetail(AssessMetar("EDDM 112220Z AUTO 07001KT 9999 VV/// M11/M11 Q1024")), "CEIL ///");
	CHECK_TEXT(BuildDetail(AssessMetar("EDDM 112220Z AUTO 07001KT 9999 VV001 R26L/0500N M11/M11 Q1024")),
		"CEIL 100");
	CHECK_TEXT(BuildDetail(AssessMetar("EDDM 121250Z AUTO 13003KT 9999 NCD 00/M01 Q1016")),
		"NO CEIL" + dot + "VIS 9999");
	CHECK_TEXT(BuildDetail(AssessMetar("EDDM 121250Z AUTO 13003KT 4000 BR NCD 00/M01 Q1016")),
		"NO CEIL" + dot + "VIS 4000");

	// also show the wintry phenomenon with VMC because it rules out RRS
	CHECK_TEXT(BuildDetail(AssessMetar("EDDF 101650Z 25008KT 9000 -SN FEW040")),
		"NO CEIL" + dot + "VIS 9000" + dot + "SN");

	// cannot be evaluated: empty
	CHECK_TEXT(BuildDetail(AssessMetar("EDDF 061820Z 21003KT 04/02 Q1023")), "");
}

// ---------------------------------------------------------------------
// RVR rows
// ---------------------------------------------------------------------

static std::string Join(const std::vector<std::string>& lines)
{
	std::string result;
	for (size_t i = 0; i < lines.size(); i++)
		result += (i > 0 ? "|" : "") + lines[i];
	return result;
}

static void TestRvrRows()
{
	std::vector<std::string> none;
	std::vector<std::string> l25L(1, "25L");
	std::vector<std::string> l25LR; l25LR.push_back("25L"); l25LR.push_back("25R");

	// groups are stored as written, with runway number and side
	MetarAssessment a = AssessMetar("EDDF 061820Z 25008KT 1500 R25L/P2000N R25C/0800V1200U R07R/0350D R18/0900 BKN010 04/02 Q1023");
	CHECK_INT((int)a.rvrGroups.size(), 4);
	CHECK_TEXT(a.rvrGroups[0].text, "R25L/P2000N");
	CHECK_INT(a.rvrGroups[0].runway, 25);
	CHECK(a.rvrGroups[0].side == 'L');
	CHECK_TEXT(a.rvrGroups[1].text, "R25C/0800V1200U");
	CHECK(a.rvrGroups[1].side == 'C');
	CHECK_INT(a.rvrGroups[3].runway, 18);
	CHECK(a.rvrGroups[3].side == 0);

	// no RVR in the METAR -> no rows; unavailable or after BECMG/RMK -> not a group
	CHECK(BuildRvrLines(AssessMetar("EDDF 061820Z 25008KT 9999 NCD 04/02 Q1023"), l25L).empty());
	CHECK(AssessMetar("EDDF 061820Z 25008KT 0300 R25L///// NCD 04/02 Q1023").rvrGroups.empty());
	CHECK(AssessMetar("EDDF 061820Z 25008KT 9999 NCD 04/02 Q1023 BECMG R25L/0400N").rvrGroups.empty());

	// only the groups of the landing runways (an RVR group belongs to the runway end)
	CHECK_TEXT(Join(BuildRvrLines(a, l25L)), "R25L/P2000N");
	CHECK_TEXT(Join(BuildRvrLines(a, l25LR)), "R25L/P2000N");		// there is no group for 25R
	std::vector<std::string> l25LC07R; l25LC07R.push_back("25L"); l25LC07R.push_back("25C"); l25LC07R.push_back("07R");
	CHECK_TEXT(Join(BuildRvrLines(a, l25LC07R)), "R25L/P2000N  R25C/0800V1200U|R07R/0350D");	// two per line

	// designator without side letter matches any side and vice versa
	std::vector<std::string> l25; l25.push_back("25");
	CHECK_TEXT(Join(BuildRvrLines(a, l25)), "R25L/P2000N  R25C/0800V1200U");
	std::vector<std::string> l18L(1, "18L");
	CHECK_TEXT(Join(BuildRvrLines(a, l18L)), "R18/0900");

	// no landing runway selected, or none of them has an RVR group -> all groups
	CHECK_TEXT(Join(BuildRvrLines(a, none)), "R25L/P2000N  R25C/0800V1200U|R07R/0350D  R18/0900");
	std::vector<std::string> l36(1, "36");
	CHECK_TEXT(Join(BuildRvrLines(a, l36)), "R25L/P2000N  R25C/0800V1200U|R07R/0350D  R18/0900");

	// invalid designators are ignored
	std::vector<std::string> junk; junk.push_back(""); junk.push_back("XY"); junk.push_back("99");
	CHECK_TEXT(Join(BuildRvrLines(a, junk)), "R25L/P2000N  R25C/0800V1200U|R07R/0350D  R18/0900");

	// lower case designators from the sector file
	std::vector<std::string> lower(1, "07r");
	CHECK_TEXT(Join(BuildRvrLines(a, lower)), "R07R/0350D");

	// the LVO decision still uses the lowest RVR of all groups
	CHECK(a.rules == FlightRules::Lvo);
	CHECK_INT(a.minRvrM, 350);
}

// ---------------------------------------------------------------------
// ATIS letters from the VATSIM data feed
// ---------------------------------------------------------------------

static std::string AtisEntryJson(const char* callsign, const char* codeJson)
{
	return std::string("{\"cid\":1234567,\"name\":\"Max \\\"Mustermann\\\"\",\"callsign\":\"") + callsign +
		"\",\"frequency\":\"118.025\",\"facility\":4,\"atis_code\":" + codeJson +
		",\"text_atis\":[\"INFORMATION K, \\\"atis\\\" [x]\",\"line 2\"]}";
}

static std::string AtisFeed(const std::string& entries)
{
	// Other top-level keys before and after; the pilot contains the word "atis" and brackets in a string
	return std::string("{\"general\":{\"version\":3,\"atis\":5},\"pilots\":[{\"callsign\":\"DLH1\",\"flight_plan\":"
		"{\"remarks\":\"\\\"atis\\\": [ { ] }\"}}],\"controllers\":[],\"atis\":[") + entries +
		"],\"servers\":[{\"ident\":\"X\"}],\"prefiles\":[]}";
}

static std::string AtisLabelOf(const std::string& entries, const char* icao)
{
	std::map<std::string, std::string> labels;
	if (!ParseAtisFeed(AtisFeed(entries), labels))
		return "<error>";
	std::map<std::string, std::string>::const_iterator it = labels.find(icao);
	return it == labels.end() ? "" : it->second;
}

static void TestAtisFeed()
{
	// Combined ATIS
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDF_ATIS", "\"K\""), "EDDF"), "K");

	// Lower case letter and callsign are normalised
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("eddf_atis", "\"k\""), "EDDF"), "K");

	// Arrival and departure with equal letters -> one letter; different -> both
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDM_A_ATIS", "\"B\"") + "," + AtisEntryJson("EDDM_D_ATIS", "\"B\""), "EDDM"), "B");
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDM_A_ATIS", "\"L\"") + "," + AtisEntryJson("EDDM_D_ATIS", "\"K\""), "EDDM"), "D:K A:L");

	// Only one of the two
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDL_A_ATIS", "\"C\""), "EDDL"), "A:C");
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDL_D_ATIS", "\"D\""), "EDDL"), "D:D");

	// Empty, null or invalid letter: nothing to show
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDH_ATIS", "\"\""), "EDDH"), "");
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDH_ATIS", "null"), "EDDH"), "");
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDH_ATIS", "\"AB\""), "EDDH"), "");
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDH_ATIS", "5"), "EDDH"), "");

	// Combined ATIS is ignored if arrival/departure ATIS exist; other callsigns are ignored
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDB_ATIS", "\"A\"") + "," + AtisEntryJson("EDDB_D_ATIS", "\"F\""), "EDDB"), "D:F");
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDB_1_ATIS", "\"A\""), "EDDB"), "");
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("EDDB_TWR", "\"A\""), "EDDB"), "");
	CHECK_TEXT(AtisLabelOf(AtisEntryJson("ATIS", "\"A\""), "ATIS"), "");

	// Several airports in one feed; other airports are not affected
	std::string two = AtisEntryJson("EDDF_ATIS", "\"K\"") + "," + AtisEntryJson("EDDK_ATIS", "\"M\"");
	CHECK_TEXT(AtisLabelOf(two, "EDDF"), "K");
	CHECK_TEXT(AtisLabelOf(two, "EDDK"), "M");
	CHECK_TEXT(AtisLabelOf(two, "EDDM"), "");

	// Empty atis array: valid feed, no results
	{
		std::map<std::string, std::string> labels;
		CHECK(ParseAtisFeed(AtisFeed(""), labels));
		CHECK(labels.empty());
	}

	// No usable feed
	{
		std::map<std::string, std::string> labels;
		labels["EDDF"] = "K";
		CHECK(!ParseAtisFeed("", labels));
		CHECK(labels.empty());
		CHECK(!ParseAtisFeed("not json", labels));
		CHECK(!ParseAtisFeed("{\"pilots\":[],\"controllers\":[]}", labels));
		CHECK(!ParseAtisFeed("{\"atis\":null}", labels));
		CHECK(!ParseAtisFeed("[1,2,3]", labels));
	}

	// Truncated feed: must not crash; entries read so far may be returned
	{
		std::string feed = AtisFeed(AtisEntryJson("EDDF_ATIS", "\"K\""));
		for (size_t cut = 0; cut < feed.size(); cut += 7)
		{
			std::map<std::string, std::string> labels;
			ParseAtisFeed(feed.substr(0, cut), labels);
		}
		CHECK(true);
	}

	// Whitespace between the tokens (pretty-printed feed)
	{
		std::map<std::string, std::string> labels;
		CHECK(ParseAtisFeed("{\n  \"atis\" : [\n    {\n      \"callsign\" : \"EDDF_ATIS\" ,\n      \"atis_code\" : \"G\"\n    }\n  ]\n}", labels));
		CHECK_TEXT(labels["EDDF"], "G");
	}
}

// ---------------------------------------------------------------------
// Sound
// ---------------------------------------------------------------------

static unsigned int ReadU32(const std::vector<unsigned char>& v, size_t pos)
{
	return v[pos] | (v[pos + 1] << 8) | (v[pos + 2] << 16) | ((unsigned int)v[pos + 3] << 24);
}

static void TestMeepSound()
{
	std::vector<unsigned char> wav = BuildMeepWav();

	CHECK(wav.size() > 44);
	CHECK(memcmp(&wav[0], "RIFF", 4) == 0);
	CHECK(memcmp(&wav[8], "WAVE", 4) == 0);
	CHECK(memcmp(&wav[12], "fmt ", 4) == 0);
	CHECK(memcmp(&wav[36], "data", 4) == 0);
	CHECK_INT((int)ReadU32(wav, 4), (int)wav.size() - 8);		// length according to the RIFF header
	CHECK_INT((int)ReadU32(wav, 40), (int)wav.size() - 44);		// length of the sound data
	CHECK_INT((int)ReadU32(wav, 24), 22050);					// sample rate

	// 0.25 seconds at 22050 Hz with 16 bit
	CHECK_INT((int)(ReadU32(wav, 40) / 2), 22050 / 4);

	// the sound must be audible but not loud: the loudest sample is at about the volume setting (0.22 of full scale)
	int peak = 0;
	for (size_t i = 44; i + 1 < wav.size(); i += 2)
	{
		int sample = (short)(wav[i] | (wav[i + 1] << 8));
		if (abs(sample) > peak)
			peak = abs(sample);
	}
	CHECK(peak > 6500 && peak < 7500);

	// starts and ends with silence (fade-in and fade-out, no click)
	CHECK(abs((short)(wav[44] | (wav[45] << 8))) < 100);
	CHECK(abs((short)(wav[wav.size() - 2] | (wav[wav.size() - 1] << 8))) < 700);
}

// ---------------------------------------------------------------------

int main()
{
	TestFlightRules();
	TestAssessedValues();
	TestObservationTime();
	TestWindParsing();
	TestWindGroupAndQnh();
	TestWindComponents();
	TestWindLines();
	TestRrs();
	TestDetail();
	TestRvrRows();
	TestAtisFeed();
	TestMeepSound();

	printf("%d checks, %d failures\n", g_tests, g_failures);
	return g_failures == 0 ? 0 : 1;
}
