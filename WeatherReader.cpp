// WeatherReader.cpp: EuroScope plugin "WeatherReader" - shows the weather state of the active airport.
//

#include "pch.h"
#include "framework.h"
#include "WeatherReader.h"
#include "DisplayLogic.h"
#include "MeepSound.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mmsystem.h>		// PlaySound

#pragma comment(lib, "winmm.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//
// Note on the MFC state: this regular MFC DLL has no exported functions that call into MFC.
// Should one be added while linking MFC dynamically (Debug configuration), its first statement
// must be AFX_MANAGE_STATE(AfxGetStaticModuleState()); (see MFC Technical Notes 33 and 58).
//

// CWeatherReaderApp

BEGIN_MESSAGE_MAP(CWeatherReaderApp, CWinApp)
END_MESSAGE_MAP()


// CWeatherReaderApp construction

CWeatherReaderApp::CWeatherReaderApp()
{
}


// The only CWeatherReaderApp object

CWeatherReaderApp theApp;


// CWeatherReaderApp initialisation

BOOL CWeatherReaderApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}


// ---------------------------------------------------------------------
// EuroScope-Plugin
// ---------------------------------------------------------------------

namespace
{
	// Type of the screen object with which EuroScope makes the box movable (freely selectable)
	const int SCREEN_OBJECT_BOX = 1;

	// Type of the screen object for the collapse/expand arrow
	const int SCREEN_OBJECT_TOGGLE = 2;

	// Type of the screen object for the QNH (clicking acknowledges a changed QNH)
	const int SCREEN_OBJECT_QNH = 3;

	// How long the box flashes after a change of the weather state
	const unsigned int FLASH_DURATION_MS = 10000;
}

// Pointer to the only plugin instance (created in Init, deleted in Exit)
static CWeatherReaderPlugIn* pMyPlugIn = nullptr;

// Called by EuroScope when the DLL is loaded.
// The declaration is in EuroScopePlugIn.h (C++ linkage, hence no extern "C").
void __declspec(dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
	*ppPlugInInstance = pMyPlugIn = new CWeatherReaderPlugIn();
}

// Called by EuroScope when the DLL is unloaded.
void __declspec(dllexport) EuroScopePlugInExit(void)
{
	delete pMyPlugIn;
	pMyPlugIn = nullptr;
}

CWeatherReaderPlugIn::CWeatherReaderPlugIn()
	: CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
		"WeatherReader",
		"1.2.0",
		"Erik J. - 1583954",
		"See LICENSE in the repository")
	, m_LastRules(FlightRules::Unknown)
{
	// Appears in the EuroScope message window when the plugin is loaded.
	DisplayUserMessage("Message", "WeatherReader", "WeatherReader 1.2.0 loaded.",
		true, true, true, false, false);

	m_Atis.Start();
	UpdateBox();
}

CWeatherReaderPlugIn::~CWeatherReaderPlugIn()
{
	m_Atis.Stop();		// the background thread must be gone before the DLL is unloaded
}

std::string CWeatherReaderPlugIn::FindActiveAirport()
{
	std::string result;

	// Search all airports of the active sector file: an airport is active if it is selected in the
	// airport selector for departure or arrival (order as in the sector file).
	SelectActiveSectorfile();

	for (EuroScopePlugIn::CSectorElement el = SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT);
		el.IsValid();
		el = SectorFileElementSelectNext(el, EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT))
	{
		if (el.IsElementActive(true) || el.IsElementActive(false))
		{
			const char* name = el.GetName();
			if (name != nullptr)
				result = name;
			break;
		}
	}

	// If the airport itself is not reported as active: search via the active runways.
	if (result.empty())
	{
		for (EuroScopePlugIn::CSectorElement el = SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY);
			el.IsValid();
			el = SectorFileElementSelectNext(el, EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY))
		{
			bool active = false;
			for (int index = 0; index < 2 && !active; index++)
				active = el.IsElementActive(true, index) || el.IsElementActive(false, index);

			if (active)
			{
				const char* name = el.GetAirportName();
				if (name != nullptr)
					result = name;
				break;
			}
		}
	}

	for (size_t i = 0; i < result.size(); i++)
		result[i] = (char)toupper((unsigned char)result[i]);

	return result;
}

std::vector<int> CWeatherReaderPlugIn::FindActiveRunways(const std::string& station)
{
	std::vector<int> runways;

	SelectActiveSectorfile();

	for (EuroScopePlugIn::CSectorElement el = SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY);
		el.IsValid();
		el = SectorFileElementSelectNext(el, EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY))
	{
		// Only runways of the selected airport. GetAirportName() returns the ICAO code padded with
		// spaces (e.g. "EDDF "); therefore only the first characters count, and no further
		// letter or digit may follow.
		const char* airport = el.GetAirportName();
		if (airport == nullptr)
			continue;
		size_t icaoLength = station.size();
		if (_strnicmp(airport, station.c_str(), icaoLength) != 0 || isalnum((unsigned char)airport[icaoLength]))
			continue;

		// A runway has two ends (index 0 and 1, e.g. "25C" and "07C"); an end is active if it is ticked
		// in the selector for departure or arrival.
		for (int index = 0; index < 2; index++)
		{
			if (!el.IsElementActive(true, index) && !el.IsElementActive(false, index))
				continue;

			const char* name = el.GetRunwayName(index);
			if (name == nullptr)
				continue;

			// Runway number = the leading digits ("25C" -> 25); parallel runways thus collapse into one
			int number = atoi(name);
			if (number < 1 || number > 36)
				continue;

			bool known = false;
			for (size_t i = 0; i < runways.size(); i++)
				if (runways[i] == number)
					known = true;
			if (!known)
				runways.push_back(number);
		}
	}

	std::sort(runways.begin(), runways.end());
	return runways;
}

std::vector<std::string> CWeatherReaderPlugIn::FindLandingRunways(const std::string& station)
{
	std::vector<std::string> runways;

	SelectActiveSectorfile();

	for (EuroScopePlugIn::CSectorElement el = SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY);
		el.IsValid();
		el = SectorFileElementSelectNext(el, EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY))
	{
		// Only runways of the selected airport (see FindActiveRunways)
		const char* airport = el.GetAirportName();
		if (airport == nullptr)
			continue;
		size_t icaoLength = station.size();
		if (_strnicmp(airport, station.c_str(), icaoLength) != 0 || isalnum((unsigned char)airport[icaoLength]))
			continue;

		// IsElementActive(false, index): the runway end is ticked as arrival runway
		for (int index = 0; index < 2; index++)
		{
			if (!el.IsElementActive(false, index))
				continue;

			const char* name = el.GetRunwayName(index);
			if (name == nullptr)
				continue;

			bool known = false;
			for (size_t i = 0; i < runways.size(); i++)
				if (_stricmp(runways[i].c_str(), name) == 0)
					known = true;
			if (!known)
				runways.push_back(name);
		}
	}

	return runways;
}

void CWeatherReaderPlugIn::UpdateBox()
{
	// Station = active airport from the airport selector (the same as for the METAR in the
	// top bar of EuroScope). If none is active, a dash is shown.
	std::string station = FindActiveAirport();

	std::string metar;
	if (!station.empty())
	{
		if (!m_TestMetar.empty())
		{
			metar = m_TestMetar;	// test mode: the given METAR applies to the active airport
		}
		else
		{
			std::map<std::string, std::string>::const_iterator it = m_Metars.find(station);
			if (it != m_Metars.end())
				metar = it->second;
		}
	}

	// Weather state from the METAR: red = LVO, green = VMC, yellow = IMC (Reduced Runway Separation possible),
	// orange = IMC (no Reduced Runway Separation), grey "-" = unknown.
	// In test mode an asterisk follows the station.
	MetarAssessment assessment = AssessMetar(metar.c_str());
	FlightRules rules = assessment.rules;

	// Header: station, with an asterisk in test mode, followed by the time of the METAR (e.g. "EDDF <dot> 1420z")
	std::string stationLabel = station.empty() ? "-" : station;
	if (!station.empty() && !m_TestMetar.empty())
		stationLabel += "*";
	if (!station.empty() && assessment.haveObsTime)
	{
		char timeText[16];
		sprintf_s(timeText, "%02d%02dz", assessment.obsHour, assessment.obsMinute);
		stationLabel += SEPARATOR;
		stationLabel += timeText;
	}
	const char* stationText = stationLabel.c_str();

	// Active runways and wind components. With yellow weather the tailwind per runway decides whether
	// Reduced Runway Separation is possible; if RRS is possible on no runway, the IMC is shown in orange.
	std::vector<int> activeRunways;
	if (!station.empty())
		activeRunways = FindActiveRunways(station);
	std::vector<bool> tailwindOverLimit;
	std::vector<std::string> windLines = BuildWindLines(assessment, activeRunways, tailwindOverLimit);
	RrsResult rrs = EvaluateRrs(assessment, activeRunways, tailwindOverLimit);
	rules = rrs.shown;

	// If the displayed state changes at the same airport (e.g. VMC -> IMC), the box flashes for 10 seconds.
	// If the QNH changes at the same airport, the QNH flashes inverted until the user clicks on it. Both play the same sound
	// (once, if both change at the same time). There is no flashing or sound on the first display, on an
	// airport change or for "unknown" (-) / a missing QNH.
	bool stateChanged = station == m_LastStation &&
		rules != m_LastRules &&
		rules != FlightRules::Unknown &&
		m_LastRules != FlightRules::Unknown;
	bool qnhChanged = station == m_LastStation &&
		!assessment.qnh.empty() &&
		!m_LastQnh.empty() &&
		assessment.qnh != m_LastQnh;
	if (stateChanged)
		m_Box.StartFlash(FLASH_DURATION_MS);
	if (qnhChanged)
		m_Box.StartQnhAlert();
	if (station != m_LastStation || assessment.qnh.empty())
		m_Box.AcknowledgeQnh();		// nothing to acknowledge for another airport or without QNH
	if (stateChanged || qnhChanged)
		PlayChangeSound();
	m_LastStation = station;
	m_LastRules = rules;
	m_LastQnh = assessment.qnh;

	switch (rules)
	{
	case FlightRules::Lvo:
		m_Box.SetContent(stationText, "LVO", RGB(230, 50, 50));
		break;
	case FlightRules::VMC:
		m_Box.SetContent(stationText, "VMC", RGB(60, 200, 90));
		break;
	case FlightRules::ImcYellow:
		m_Box.SetContent(stationText, "IMC", RGB(240, 200, 40));
		break;
	case FlightRules::ImcOrange:
		m_Box.SetContent(stationText, "IMC", RGB(255, 140, 0));
		break;
	default:
		m_Box.SetContent(stationText, "-", RGB(170, 170, 170));
		break;
	}

	// Wind group and QNH as written in the METAR (bold row above the state)
	m_Box.SetInfo(assessment.windGroup.c_str(), assessment.qnh.c_str());

	// RVR rows: the RVR groups of the METAR for the landing runways (in the order of the METAR)
	std::vector<std::string> landingRunways;
	if (!station.empty())
		landingRunways = FindLandingRunways(station);
	m_Box.SetRvr(BuildRvrLines(assessment, landingRunways));

	// ATIS letter(s) of the airport from the VATSIM data feed (left in the header)
	std::string atisLabel;
	if (!station.empty())
		m_Atis.GetLabel(station, atisLabel);
	m_Box.SetAtis(atisLabel.c_str());

	// RRS note, wind lines and the small line with the decisive values
	// (all after SetContent, which clears them).
	RrsNote note = BuildRrsNote(rrs);
	if (note.present)
		m_Box.SetNote(note.before.c_str(), note.bold.c_str(), note.after.c_str());

	// Lines with too much tailwind turn orange as long as the weather would otherwise allow RRS.
	std::vector<bool> warn;
	if (rrs.weatherAllows)
		warn = tailwindOverLimit;
	m_Box.SetWindLines(windLines, warn);
	m_Box.SetDetail(BuildDetail(assessment).c_str());
}

void CWeatherReaderPlugIn::PlayChangeSound()
{
	// The sound is generated on first use and must stay in memory during asynchronous playback
	if (m_MeepWav.empty())
		m_MeepWav = BuildMeepWav();

	PlaySoundA((LPCSTR)&m_MeepWav[0], NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

void CWeatherReaderPlugIn::OnNewMetarReceived(const char* sStation, const char* sFullMetar)
{
	if (sStation == nullptr || sFullMetar == nullptr)
		return;

	std::string station = sStation;
	for (size_t i = 0; i < station.size(); i++)
		station[i] = (char)toupper((unsigned char)station[i]);

	m_Metars[station] = sFullMetar;
	UpdateBox();
}

void CWeatherReaderPlugIn::OnTimer(int Counter)
{
	// The VATSIM data feed is only downloaded while connected to the real network (not on Sweatbox,
	// as a simulator or in playback, where the feed knows nothing about our ATIS).
	int connection = GetConnectionType();
	m_Atis.SetActive(connection == EuroScopePlugIn::CONNECTION_TYPE_DIRECT ||
		connection == EuroScopePlugIn::CONNECTION_TYPE_VIA_PROXY);

	UpdateBox();
}

bool CWeatherReaderPlugIn::OnCompileCommand(const char* sCommandLine)
{
	// ".wbox metar <METAR>" sets a test METAR, ".wbox metar" or ".wbox metar off" removes it.
	const char* metarCommand = ".wbox metar";
	size_t metarCommandLength = strlen(metarCommand);
	if (_strnicmp(sCommandLine, metarCommand, metarCommandLength) == 0 &&
		(sCommandLine[metarCommandLength] == '\0' || sCommandLine[metarCommandLength] == ' '))
	{
		const char* argument = sCommandLine + metarCommandLength;
		while (*argument == ' ')
			argument++;

		std::string text = argument;
		while (!text.empty() && (text[text.size() - 1] == ' ' || text[text.size() - 1] == '\r' || text[text.size() - 1] == '\n'))
			text.erase(text.size() - 1);

		std::string message;
		if (text.empty() || _stricmp(text.c_str(), "off") == 0)
		{
			m_TestMetar.clear();
			message = "Test METAR removed, the live METAR is used again.";
		}
		else
		{
			m_TestMetar = text;

			const char* result = "cannot be evaluated (-)";
			switch (DetermineFlightRules(text.c_str()))
			{
			case FlightRules::Lvo:       result = "LVO"; break;
			case FlightRules::VMC:       result = "VMC"; break;
			case FlightRules::ImcYellow: result = "IMC yellow (weather allows RRS, tailwind per runway see box)"; break;
			case FlightRules::ImcOrange: result = "IMC orange (RRS not possible)"; break;
			default: break;
			}
			message = std::string("Test METAR active for the active airport: ") + result + ". Remove with \".wbox metar off\".";
		}

		UpdateBox();
		DisplayUserMessage("Message", "WeatherReader", message.c_str(), true, true, true, false, false);
		return true;
	}

	// ".wbox" shows or hides the box (visible after the next refresh of EuroScope).
	if (_stricmp(sCommandLine, ".wbox") == 0)
	{
		m_Box.ToggleVisible();
		DisplayUserMessage("Message", "WeatherReader", m_Box.IsVisible() ? "Box shown." : "Box hidden.",
			true, true, true, false, false);
		return true;	// command was handled by us
	}

	return false;		// not our command
}

void CWeatherReaderPlugIn::SaveBoxLayout(const char* sScreenKey, const CWeatherBox& box)
{
	// Only save a position chosen by the user, not the automatic start position.
	if (box.HasUserPosition())
	{
		POINT position = box.GetPosition();
		char value[64];
		sprintf_s(value, "%d,%d", (int)position.x, (int)position.y);

		std::string name = std::string("BoxPosition") + sScreenKey;
		std::string description = std::string("WeatherReader: box position (") + sScreenKey + ")";
		SaveDataToSettings(name.c_str(), description.c_str(), value);
	}

	std::string name = std::string("BoxCollapsed") + sScreenKey;
	std::string description = std::string("WeatherReader: box collapsed (") + sScreenKey + ")";
	SaveDataToSettings(name.c_str(), description.c_str(), box.IsCollapsed() ? "1" : "0");
}

void CWeatherReaderPlugIn::LoadBoxLayout(const char* sScreenKey, CWeatherBox& box)
{
	std::string positionName = std::string("BoxPosition") + sScreenKey;
	const char* position = GetDataFromSettings(positionName.c_str());
	if (position != nullptr)
	{
		int left = 0, top = 0;
		if (sscanf_s(position, "%d,%d", &left, &top) == 2)
			box.SetPosition(left, top);
	}

	std::string collapsedName = std::string("BoxCollapsed") + sScreenKey;
	const char* collapsed = GetDataFromSettings(collapsedName.c_str());
	if (collapsed != nullptr)
		box.SetCollapsed(strcmp(collapsed, "1") == 0);
}

EuroScopePlugIn::CRadarScreen* CWeatherReaderPlugIn::OnRadarScreenCreated(const char* sDisplayName,
	bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated)
{
	// Hook into the standard radar screen and the ground radar (like e.g. vSID).
	// The key separates the saved positions of the two screen types.
	if (_stricmp(sDisplayName, "Standard ES radar screen") == 0)
		return new CWeatherRadarScreen(this, "Standard");

	if (_stricmp(sDisplayName, "Ground Radar display") == 0)
		return new CWeatherRadarScreen(this, "Ground");

	return nullptr;
}


// ---------------------------------------------------------------------
// Radar screen with the box
// ---------------------------------------------------------------------

CWeatherRadarScreen::CWeatherRadarScreen(CWeatherReaderPlugIn* pOwner, const char* sScreenKey)
	: m_pOwner(pOwner)
	, m_Key(sScreenKey)
{
	// Load the saved position and collapsed state (if any)
	m_pOwner->LoadBoxLayout(m_Key.c_str(), m_Box);
}

RECT CWeatherRadarScreen::GetAllowedArea()
{
	RECT area = GetRadarArea();
	RECT toolbar = GetToolbarArea();

	// Exclude the top bar (green)
	if (toolbar.bottom > area.top && toolbar.bottom < area.bottom)
		area.top = toolbar.bottom;

	return area;
}

void CWeatherRadarScreen::OnRefresh(HDC hDC, int Phase)
{
	// After all lists but before chat, bar and popup menus: this way the box never lies
	// over the bar.
	if (Phase != EuroScopePlugIn::REFRESH_PHASE_AFTER_LISTS)
		return;

	// Content comes centrally from the plugin; position and collapsed state belong to this screen
	CWeatherBox& box = m_Box;
	box.CopyContentFrom(m_pOwner->GetBox());

	if (box.Draw(hDC, GetAllowedArea()))
	{
		// Register a screen object so that EuroScope lets the box be moved with the mouse
		AddScreenObject(SCREEN_OBJECT_BOX, "WeatherBox", box.GetRect(), true, "");

		// The arrow is registered after the box so that it takes precedence on a click
		AddScreenObject(SCREEN_OBJECT_TOGGLE, "WeatherBoxToggle", box.GetToggleRect(), false, "");

		// While the QNH flashes, it can be clicked to acknowledge it
		if (box.IsQnhAlert())
			AddScreenObject(SCREEN_OBJECT_QNH, "WeatherBoxQnh", box.GetQnhRect(), false, "");
	}
}

void CWeatherRadarScreen::OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released)
{
	if (ObjectType != SCREEN_OBJECT_BOX)
		return;

	// "Area" is the rectangle moved by EuroScope while dragging
	m_Box.MoveTo(Area, GetAllowedArea());

	// Save only on release, not at every step of the move
	if (Released)
		m_pOwner->SaveBoxLayout(m_Key.c_str(), m_Box);

	RequestRefresh();
}

void CWeatherRadarScreen::OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button)
{
	if (Button != EuroScopePlugIn::BUTTON_LEFT)
		return;

	CWeatherBox& box = m_Box;

	// A click on the flashing QNH acknowledges the change. The central box (content for all screens) is the one
	// that counts, this screen's copy is updated at the next refresh.
	bool qnh = (ObjectType == SCREEN_OBJECT_QNH);
	if (!qnh && ObjectType == SCREEN_OBJECT_BOX && box.IsQnhAlert())
		qnh = (PtInRect(&box.GetQnhRect(), Pt) != FALSE);
	if (qnh)
	{
		box.AcknowledgeQnh();
		m_pOwner->GetBox().AcknowledgeQnh();
		RequestRefresh();
		return;
	}

	bool toggle = (ObjectType == SCREEN_OBJECT_TOGGLE);

	// To be safe, also evaluate a click on the box itself that falls on the arrow
	if (!toggle && ObjectType == SCREEN_OBJECT_BOX)
		toggle = (PtInRect(&box.GetToggleRect(), Pt) != FALSE);

	if (toggle)
	{
		box.ToggleCollapsed();
		m_pOwner->SaveBoxLayout(m_Key.c_str(), box);
		RequestRefresh();
	}
}

void CWeatherRadarScreen::OnAsrContentToBeClosed(void)
{
	delete this;	// must be the last action (EuroScope requirement)
}
