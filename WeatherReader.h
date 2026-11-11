// WeatherReader.h: Main header file for the WeatherReader DLL
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include "es/EuroScopePlugIn.h"	// EuroScope plugin SDK
#include "WeatherBox.h"		// info box
#include "MetarParser.h"	// VMC/IMC from the METAR
#include "AtisFetcher.h"	// ATIS letters from the VATSIM data feed
#include <map>
#include <string>
#include <vector>


// CWeatherReaderApp
// The MFC application object of the DLL (see WeatherReader.cpp).
//

class CWeatherReaderApp : public CWinApp
{
public:
	CWeatherReaderApp();

// Overrides
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};


// CWeatherReaderPlugIn
// The actual EuroScope plugin class.
//

class CWeatherReaderPlugIn : public EuroScopePlugIn::CPlugIn
{
public:
	CWeatherReaderPlugIn();
	virtual ~CWeatherReaderPlugIn();

	// Called when a dot command is entered in the EuroScope command line.
	//   .wbox                 show/hide the box
	//   .wbox metar <METAR>   use a test METAR instead of the live METAR (for testing)
	//   .wbox metar           (without text) or ".wbox metar off": use the live METAR again
	virtual bool OnCompileCommand(const char* sCommandLine);

	// Called when a radar screen is opened. We return a radar screen
	// into which the box is drawn.
	virtual EuroScopePlugIn::CRadarScreen* OnRadarScreenCreated(const char* sDisplayName,
		bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated);

	// Called when EuroScope receives a new METAR (at the same time as EuroScope).
	virtual void OnNewMetarReceived(const char* sStation, const char* sFullMetar);

	// Called by EuroScope once per second (detects login/logout and station changes).
	virtual void OnTimer(int Counter);

	CWeatherBox& GetBox() { return m_Box; }

	// Saves/loads position and collapsed state of the box of a screen type (sScreenKey e.g. "Ground")
	// in the EuroScope settings (settings file).
	void SaveBoxLayout(const char* sScreenKey, const CWeatherBox& box);
	void LoadBoxLayout(const char* sScreenKey, CWeatherBox& box);

private:
	// Updates station and weather state in the box.
	void UpdateBox();

	// Returns the runway numbers (e.g. 25 for 25L, 25C and 25R; 18 for 18) that are active in the airport selector
	// as departure or arrival runway for the airport station, ascending and without duplicates.
	std::vector<int> FindActiveRunways(const std::string& station);

	// Plays the short "meep" (change of the weather state).
	void PlayChangeSound();

	// Returns the airport that is active in the "Active airport/runway selector dialog" (e.g. "EDDK"),
	// empty if none is active.
	std::string FindActiveAirport();

	CWeatherBox m_Box;	// the info box with station and weather state

	// Last displayed state: on a change at the same airport the box flashes
	std::string m_LastStation;
	FlightRules m_LastRules;
	std::string m_LastQnh;	// last displayed QNH: on a change at the same airport the QNH flashes

	// Test METAR (set via ".wbox metar ..."); empty = use the live METAR
	std::string m_TestMetar;

	std::vector<unsigned char> m_MeepWav;	// the generated "meep" as a WAV in memory

	CAtisFetcher m_Atis;	// background download of the ATIS letters (VATSIM data feed)

	std::map<std::string, std::string> m_Metars;	// station (upper case) -> last METAR
};


// CWeatherRadarScreen
// The radar screen into which the box is drawn. When the box is moved with the
// mouse, EuroScope calls OnMoveScreenObject.
//

class CWeatherRadarScreen : public EuroScopePlugIn::CRadarScreen
{
public:
	CWeatherRadarScreen(CWeatherReaderPlugIn* pOwner, const char* sScreenKey);

	virtual void OnRefresh(HDC hDC, int Phase);
	virtual void OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released);
	virtual void OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button);
	virtual void OnAsrContentToBeClosed(void);

private:
	// Radar area without the top bar (toolbar): the box may lie here.
	RECT GetAllowedArea();

	CWeatherReaderPlugIn* m_pOwner;
	std::string m_Key;	// screen type under which position and collapsed state are saved
	CWeatherBox m_Box;	// the box of this screen: own position and own collapsed state
};
