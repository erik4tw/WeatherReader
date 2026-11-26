// WeatherBox.h: Small info box for the WeatherReader plugin
//
// The box shows the weather of one or more airports (every airport that is active in the airport selector).
// Each airport is one "entry" (WeatherBoxEntry).
//
// One airport (the usual case): at the top the header with the station (e.g. "WX EDDF"; at its left the ATIS
// letter, if known), below it the wind group and QNH (bold), then the state in large letters (e.g. "VMC"), a small
// note (e.g. "RRS POSSIBLE"), the wind components per runway and a line with the values. If the METAR has RVR
// groups, one or more bold rows with them follow below the wind/QNH row (expanded only). An arrow at the top
// right of the header collapses and expands the box; when collapsed only the header (with ATIS letter) and the
// wind/QNH row are visible.
//
// Several airports: when collapsed, the header only says "WX" and below it there is one row per airport with
// the ICAO code (in the colour of its weather state), the wind group and the QNH. A click on a row selects the
// airport and expands the box. When expanded, a row of tabs with the ICAO codes sits below the header; the
// selected airport is shown in detail as described above. A click on another tab selects that airport.
// A change at an airport (state or QNH) makes its row or tab flash until it is clicked.
//
// The box is at least 140 pixels wide and grows if the header, note, value line or rows need more room.
//
// The box is not a window of its own but is drawn by EuroScope into the radar screen
// (like e.g. the menu of vSID). This way it can never leave the EuroScope window. The class only
// knows position, content and drawing; moving it with the mouse is done by EuroScope via a
// "screen object" (see CWeatherRadarScreen).

#pragma once

#include <string>
#include <vector>

// "Until acknowledged": value for the end of a flash that stops only when the user clicks.
const ULONGLONG FLASH_UNTIL_ACKNOWLEDGED = (ULONGLONG)-1;

// Flashing of one airport: the state flashes (coloured area) until flashEnd, the QNH flashes (inverted) until
// it is acknowledged. All times are GetTickCount64() values.
struct WeatherBoxAlert
{
	ULONGLONG flashStart = 0;
	ULONGLONG flashEnd = 0;			// 0 = not flashing, FLASH_UNTIL_ACKNOWLEDGED = until clicked
	ULONGLONG qnhFlashStart = 0;
	bool qnhAlert = false;

	// Makes the state flash for durationMs milliseconds; 0 = until acknowledged.
	void StartFlash(unsigned int durationMs)
	{
		flashStart = GetTickCount64();
		flashEnd = (durationMs == 0) ? FLASH_UNTIL_ACKNOWLEDGED : flashStart + durationMs;
	}

	// Makes the QNH flash until acknowledged. If it is already flashing, nothing changes (keeps the rhythm).
	void StartQnhAlert()
	{
		if (qnhAlert)
			return;
		qnhFlashStart = GetTickCount64();
		qnhAlert = true;
	}

	// Stops the flashing: only the QNH, or both state and QNH.
	void Acknowledge(bool qnhOnly)
	{
		qnhAlert = false;
		if (!qnhOnly)
			flashEnd = 0;
	}
};

// Everything the box shows for one airport.
struct WeatherBoxEntry
{
	std::string icao;			// airport, e.g. "EDDF" (empty if there is none)
	std::string station;		// header text after "WX ", e.g. "EDDF <dot> 1420z" or "-"
	std::string text;			// the large text, e.g. "VMC"
	COLORREF colour = RGB(255, 255, 255);	// colour of the weather state

	std::string wind;			// wind group in the row above the large text, e.g. "25024G35KT"
	std::string qnh;			// QNH in the same row, e.g. "Q1019"
	std::string atis;			// ATIS letter(s) at the left of the header, e.g. "K" or "D:K A:L"
	std::vector<std::string> rvrLines;		// RVR rows (expanded only)

	std::string noteBefore;		// note below the large text: normal, bold, normal
	std::string noteBold;
	std::string noteAfter;

	std::vector<std::string> windLines;		// wind components, one line per runway
	std::vector<bool> windWarn;				// true = line orange
	std::string detail;			// small grey line at the very bottom, e.g. "CEIL 1200 <dot> VIS 9999"

	WeatherBoxAlert alert;
};

class CWeatherBox
{
public:
	CWeatherBox();

	// Takes over the content (all entries) and the visibility from another box. Position, collapsed state and
	// selected airport stay unchanged. This way each radar screen has its own box with its own position
	// while the content is computed centrally in the plugin.
	void CopyContentFrom(const CWeatherBox& other);

	// Sets the entries (one per airport, in the order of the rows). There should be at least one.
	void SetEntries(const std::vector<WeatherBoxEntry>& entries);
	const std::vector<WeatherBoxEntry>& GetEntries() const { return m_Entries; }

	// Selects the airport shown in detail (no effect if the box has no entry for it). Without a valid selection
	// the first entry counts.
	void SelectAirport(const std::string& icao);

	// The airport shown in detail (empty if there is none).
	std::string GetSelectedIcao() const;

	// Stops the flashing of an airport: only its QNH, or state and QNH.
	void AcknowledgeAirport(const std::string& icao, bool qnhOnly);

	// true if the QNH of the airport shown in detail flashes and can be clicked (valid after the last Draw()).
	bool IsQnhAlert() const;

	// Click area of the QNH (right part of the wind/QNH row), valid after the last Draw().
	const RECT& GetQnhRect() const { return m_rcQnh; }

	// Click areas of the airports (rows when collapsed with several airports, tabs when expanded with several
	// airports), valid after the last Draw(). Empty with only one airport.
	struct AirportHit
	{
		RECT rc;
		std::string icao;
	};
	const std::vector<AirportHit>& GetAirportHits() const { return m_Hits; }

	// Shows and hides the box alternately.
	void ToggleVisible() { m_bVisible = !m_bVisible; }
	bool IsVisible() const { return m_bVisible; }

	// Collapses the box (only header and wind/QNH row, or the airport rows, visible) or expands it again.
	void ToggleCollapsed() { m_bCollapsed = !m_bCollapsed; }
	void SetCollapsed(bool bCollapsed) { m_bCollapsed = bCollapsed; }
	bool IsCollapsed() const { return m_bCollapsed; }

	// Sets the position (top left corner), e.g. when loading from the settings. It is limited to
	// the allowed area on the next draw.
	void SetPosition(int left, int top);

	// true if the position comes from the user (moved or loaded), not the start position.
	bool HasUserPosition() const { return m_bUserPlaced; }

	// Top left corner of the box.
	POINT GetPosition() const { POINT pt = { m_rc.left, m_rc.top }; return pt; }

	// Draws the box into the device context of EuroScope. "area" is the area in which
	// the box may lie. Returns false if the box is hidden (nothing drawn).
	bool Draw(HDC hDC, const RECT& area);

	// Current position (pixels in the radar screen), valid after the last Draw().
	const RECT& GetRect() const { return m_rc; }

	// Click area of the collapse/expand arrow (top right), valid after the last Draw().
	const RECT& GetToggleRect() const { return m_rcToggle; }

	// Sets the top left corner to that of newPos (the size stays) and limits it to "area".
	void MoveTo(const RECT& newPos, const RECT& area);

private:
	void Clamp(const RECT& area);

	// Index of the airport shown in detail (0 if the selection is not valid)
	size_t SelectedIndex() const;

	bool        m_bVisible;
	bool        m_bCollapsed;	// true: only header and wind/QNH row (or the airport rows) are visible
	bool        m_bPlaced;		// false until the start position has been set
	bool        m_bUserPlaced;	// true as soon as the position has been moved or loaded
	RECT        m_rc;
	RECT        m_rcToggle;		// click area of the arrow
	RECT        m_rcQnh;		// click area of the QNH

	std::vector<WeatherBoxEntry> m_Entries;
	std::string m_sSelected;	// ICAO of the airport shown in detail (own for each radar screen)
	std::vector<AirportHit> m_Hits;
};
