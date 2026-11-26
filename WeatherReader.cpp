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

	// Type of the screen objects for the airport rows/tabs (several airports; the object id is the ICAO code)
	const int SCREEN_OBJECT_AIRPORT = 4;

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
		"1.3.0",
		"Erik J. - 1583954",
		"See LICENSE in the repository")
{
	// Appears in the EuroScope message window when the plugin is loaded.
	DisplayUserMessage("Message", "WeatherReader", "WeatherReader 1.3.0 loaded.",
		true, true, true, false, false);

	m_Atis.Start();
	UpdateBox();
}

CWeatherReaderPlugIn::~CWeatherReaderPlugIn()
{
	m_Atis.Stop();		// the background thread must be gone before the DLL is unloaded
}

std::vector<std::string> CWeatherReaderPlugIn::FindActiveAirports()
{
	std::vector<std::string> airports;

	// Search all airports of the active sector file: an airport is active if it is selected in the
	// airport selector for departure or arrival (order as in the sector file).
	SelectActiveSectorfile();

	for (EuroScopePlugIn::CSectorElement el = SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT);
		el.IsValid();
		el = SectorFileElementSelectNext(el, EuroScopePlugIn::SECTOR_ELEMENT_AIRPORT))
	{
		if (el.IsElementActive(true) || el.IsElementActive(false))
			AddActiveAirport(airports, el.GetName());
	}

	// If an airport itself is not reported as active: also look at the active runways (appended after the others).
	for (EuroScopePlugIn::CSectorElement el = SectorFileElementSelectFirst(EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY);
		el.IsValid();
		el = SectorFileElementSelectNext(el, EuroScopePlugIn::SECTOR_ELEMENT_RUNWAY))
	{
		bool active = false;
		for (int index = 0; index < 2 && !active; index++)
			active = el.IsElementActive(true, index) || el.IsElementActive(false, index);

		if (active)
			AddActiveAirport(airports, el.GetAirportName());
	}

	return airports;
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
	// Stations = the active airports from the airport selector (the same as for the METAR in the
	// top bar of EuroScope). If none is active, a dash is shown.
	std::vector<std::string> stations = FindActiveAirports();

	// Forget airports that are no longer active (they start without alerts when they become active again)
	for (std::map<std::string, AirportMemory>::iterator it = m_Memory.begin(); it != m_Memory.end(); )
	{
		if (std::find(stations.begin(), stations.end(), it->first) == stations.end())
			it = m_Memory.erase(it);
		else
			++it;
	}

	bool multi = stations.size() > 1;
	bool playSound = false;
	std::vector<WeatherBoxEntry> entries;
	for (size_t i = 0; i < stations.size(); i++)
		entries.push_back(BuildEntry(stations[i], i == 0 && !m_TestMetar.empty(), multi, playSound));

	if (entries.empty())
	{
		WeatherBoxEntry none;
		none.station = "-";
		none.text = "-";
		none.colour = RGB(170, 170, 170);
		entries.push_back(none);
	}

	m_Box.SetEntries(entries);

	// One sound for all changes of this update
	if (playSound)
		PlayChangeSound();
}

WeatherBoxEntry CWeatherReaderPlugIn::BuildEntry(const std::string& station, bool useTestMetar, bool multi, bool& playSound)
{
	std::string metar;
	if (useTestMetar)
	{
		metar = m_TestMetar;	// test mode: the given METAR applies to this airport
	}
	else
	{
		std::map<std::string, std::string>::const_iterator it = m_Metars.find(station);
		if (it != m_Metars.end())
			metar = it->second;
	}

	// Weather state from the METAR: red = LVO, green = VMC, yellow = IMC (Reduced Runway Separation possible),
	// orange = IMC (no Reduced Runway Separation), grey "-" = unknown.
	// In test mode an asterisk follows the station.
	MetarAssessment assessment = AssessMetar(metar.c_str());
	FlightRules rules = assessment.rules;

	WeatherBoxEntry entry;
	entry.icao = station;

	// Header: station, with an asterisk in test mode, followed by the time of the METAR (e.g. "EDDF <dot> 1420z")
	std::string stationLabel = station;
	if (useTestMetar)
		stationLabel += "*";
	if (assessment.haveObsTime)
	{
		char timeText[16];
		sprintf_s(timeText, "%02d%02dz", assessment.obsHour, assessment.obsMinute);
		stationLabel += SEPARATOR;
		stationLabel += timeText;
	}
	entry.station = stationLabel;

	// Active runways and wind components. With yellow weather the tailwind per runway decides whether
	// Reduced Runway Separation is possible; if RRS is possible on no runway, the IMC is shown in orange.
	std::vector<int> activeRunways = FindActiveRunways(station);
	std::vector<bool> tailwindOverLimit;
	std::vector<std::string> windLines = BuildWindLines(assessment, activeRunways, tailwindOverLimit);
	RrsResult rrs = EvaluateRrs(assessment, activeRunways, tailwindOverLimit);
	rules = rrs.shown;

	// If the displayed state changes at the same airport (e.g. VMC -> IMC), the airport flashes: for 10 seconds with
	// one airport, until it is clicked with several (it might be out of sight). If the QNH changes at the same
	// airport, the QNH flashes inverted until the user clicks on it. Both play the same sound (once per update).
	// There is no flashing or sound on the first display of an airport or for "unknown" (-) / a missing QNH.
	bool known = m_Memory.find(station) != m_Memory.end();
	AirportMemory& memory = m_Memory[station];
	AlertDecision decision = DecideAlerts(known, memory.rules, rules, memory.qnh, assessment.qnh);
	if (decision.stateChanged)
		memory.alert.StartFlash(multi ? 0 : FLASH_DURATION_MS);
	if (decision.qnhChanged)
		memory.alert.StartQnhAlert();
	if (assessment.qnh.empty())
		memory.alert.Acknowledge(true);		// nothing to acknowledge without QNH
	if (decision.stateChanged || decision.qnhChanged)
		playSound = true;
	memory.rules = rules;
	memory.qnh = assessment.qnh;
	entry.alert = memory.alert;

	switch (rules)
	{
	case FlightRules::Lvo:
		entry.text = "LVO";
		entry.colour = RGB(230, 50, 50);
		break;
	case FlightRules::VMC:
		entry.text = "VMC";
		entry.colour = RGB(60, 200, 90);
		break;
	case FlightRules::ImcYellow:
		entry.text = "IMC";
		entry.colour = RGB(240, 200, 40);
		break;
	case FlightRules::ImcOrange:
		entry.text = "IMC";
		entry.colour = RGB(255, 140, 0);
		break;
	default:
		entry.text = "-";
		entry.colour = RGB(170, 170, 170);
		break;
	}

	// Wind group and QNH as written in the METAR (bold row above the state)
	entry.wind = assessment.windGroup;
	entry.qnh = assessment.qnh;

	// RVR rows: the RVR groups of the METAR for the landing runways (in the order of the METAR)
	entry.rvrLines = BuildRvrLines(assessment, FindLandingRunways(station));

	// ATIS letter(s) of the airport from the VATSIM data feed (left in the header)
	m_Atis.GetLabel(station, entry.atis);

	// RRS note, wind lines and the small line with the decisive values
	RrsNote note = BuildRrsNote(rrs);
	if (note.present)
	{
		entry.noteBefore = note.before;
		entry.noteBold = note.bold;
		entry.noteAfter = note.after;
	}

	// Lines with too much tailwind turn orange as long as the weather would otherwise allow RRS.
	entry.windLines = windLines;
	if (rrs.weatherAllows)
		entry.windWarn = tailwindOverLimit;
	entry.detail = BuildDetail(assessment);

	return entry;
}

void CWeatherReaderPlugIn::AcknowledgeAirport(const std::string& icao, bool qnhOnly)
{
	std::map<std::string, AirportMemory>::iterator it = m_Memory.find(icao);
	if (it != m_Memory.end())
		it->second.alert.Acknowledge(qnhOnly);

	m_Box.AcknowledgeAirport(icao, qnhOnly);
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
			message = std::string("Test METAR active for the (first) active airport: ") + result + ". Remove with \".wbox metar off\".";
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

		// Several airports: every row (collapsed) or tab (expanded) can be clicked to select the airport
		const std::vector<CWeatherBox::AirportHit>& hits = box.GetAirportHits();
		for (size_t i = 0; i < hits.size(); i++)
			AddScreenObject(SCREEN_OBJECT_AIRPORT, hits[i].icao.c_str(), hits[i].rc, false, "");

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

	// Several airports: a click on a row or tab selects the airport, stops its flashing and, if the box is
	// collapsed, expands it to show the airport in detail. (To be safe, also a click on the box itself that
	// falls on a row or tab counts.)
	std::string clickedAirport;
	if (ObjectType == SCREEN_OBJECT_AIRPORT && sObjectId != nullptr)
	{
		clickedAirport = sObjectId;
	}
	else if (ObjectType == SCREEN_OBJECT_BOX)
	{
		const std::vector<CWeatherBox::AirportHit>& hits = box.GetAirportHits();
		for (size_t i = 0; i < hits.size() && clickedAirport.empty(); i++)
			if (PtInRect(&hits[i].rc, Pt) != FALSE)
				clickedAirport = hits[i].icao;
	}
	if (!clickedAirport.empty())
	{
		box.SelectAirport(clickedAirport);
		box.AcknowledgeAirport(clickedAirport, false);
		m_pOwner->AcknowledgeAirport(clickedAirport, false);
		if (box.IsCollapsed())
		{
			box.SetCollapsed(false);
			m_pOwner->SaveBoxLayout(m_Key.c_str(), box);
		}
		RequestRefresh();
		return;
	}

	// A click on the flashing QNH acknowledges the change. The central state (content for all screens) is the one
	// that counts, this screen's copy is updated at the next refresh.
	bool qnh = (ObjectType == SCREEN_OBJECT_QNH);
	if (!qnh && ObjectType == SCREEN_OBJECT_BOX && box.IsQnhAlert())
		qnh = (PtInRect(&box.GetQnhRect(), Pt) != FALSE);
	if (qnh)
	{
		std::string selected = box.GetSelectedIcao();
		box.AcknowledgeAirport(selected, true);
		m_pOwner->AcknowledgeAirport(selected, true);
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
