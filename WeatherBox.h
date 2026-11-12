// WeatherBox.h: Small info box for the WeatherReader plugin
//
// At the top the header with the station (e.g. "WX EDDF"; at its left the ATIS letter, if known), below it the wind group and QNH (bold), then the state in large letters
// (e.g. "VMC"), a small note (e.g. "RRS POSSIBLE"), the wind components per runway and a line
// with the values.
// The box is at least 140 pixels wide and grows if the header, note or value line need more room.
// If the METAR has RVR groups, one or more bold rows with them follow below the wind/QNH row (expanded only).
// An arrow at the top right of the header collapses and expands the box;
// when collapsed only the header (with ATIS letter) and the wind/QNH row are visible.
//
// The box is not a window of its own but is drawn by EuroScope into the radar screen
// (like e.g. the menu of vSID). This way it can never leave the EuroScope window. The class only
// knows position, content and drawing; moving it with the mouse is done by EuroScope via a
// "screen object" (see CWeatherRadarScreen).

#pragma once

#include <string>
#include <vector>

class CWeatherBox
{
public:
	CWeatherBox();

	// Takes over content (station, text, colour, note, flashing) and visibility from another box.
	// Position and collapsed state stay unchanged. This way each radar screen has its own
	// box with its own position while the content is computed centrally in the plugin.
	void CopyContentFrom(const CWeatherBox& other);

	// Sets the station, the large text (e.g. "VMC") and its colour. Clears note, wind lines and detail line.
	void SetContent(const char* sStation, const char* sText, COLORREF colour);

	// Sets the bold row above the large text: wind group on the left (e.g. "25024G35KT") and QNH on the
	// right (e.g. "Q1019"). Either may be empty; both empty = no row. Must be called after SetContent()
	// because SetContent() clears them.
	void SetInfo(const char* sWind, const char* sQnh);

	// Sets the RVR rows (e.g. "R25L/P2000N R25R/1200N", one string per row) shown in bold below the wind/QNH row, only
	// while the box is expanded. Empty = no rows. Must be called after SetContent() because SetContent() clears them.
	void SetRvr(const std::vector<std::string>& lines);

	// Sets the ATIS information letter(s) shown in bold at the left of the header (e.g. "K" or "D:K A:L").
	// Empty = none. Must be called after SetContent() because SetContent() clears it.
	void SetAtis(const char* sAtis);

	// Makes the QNH flash inverted (after a change) until it is acknowledged with AcknowledgeQnh().
	// If it is already flashing, nothing changes.
	void StartQnhAlert();

	// Stops the QNH flashing (the user has clicked on the QNH, or the airport has changed).
	void AcknowledgeQnh() { m_bQnhAlert = false; }
	bool IsQnhAlert() const { return m_bQnhAlert; }

	// Click area of the QNH (right part of the wind/QNH row), valid after the last Draw().
	const RECT& GetQnhRect() const { return m_rcQnh; }

	// Sets the wind lines (one per runway, e.g. "RWY 25 HW: 5 kts <dot> CW: 3 kts") in small type
	// below the note. warn[i] = true colours line i orange (e.g. too much tailwind for RRS);
	// if warn is shorter than lines, the remaining lines count as normal. Empty = no wind lines.
	// Must be called after SetContent() because SetContent() clears them.
	void SetWindLines(const std::vector<std::string>& lines, const std::vector<bool>& warn);

	// Sets a small grey line at the very bottom, e.g. "CEIL 1200 <dot> VIS 9999".
	// Must be called after SetContent() because SetContent() clears it.
	void SetDetail(const char* sDetail);

	// Sets a small grey note below the large text, e.g.
	// "RRS " + bold "NOT" + " POSSIBLE". All parts empty = no note. Must be called after
	// SetContent() because SetContent() clears the note.
	void SetNote(const char* sBefore, const char* sBold, const char* sAfter);

	// Makes the box flash for durationMs milliseconds (alternating filled area) to draw attention
	// to a change. If the box is collapsed, the header flashes.
	void StartFlash(unsigned int durationMs);

	// Shows and hides the box alternately.
	void ToggleVisible() { m_bVisible = !m_bVisible; }
	bool IsVisible() const { return m_bVisible; }

	// Collapses the box (only header and wind/QNH row visible) or expands it again.
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

	bool        m_bVisible;
	bool        m_bCollapsed;	// true: only header and wind/QNH row are visible
	bool        m_bPlaced;		// false until the start position has been set
	bool        m_bUserPlaced;	// true as soon as the position has been moved or loaded
	RECT        m_rc;
	RECT        m_rcToggle;		// click area of the arrow
	RECT        m_rcQnh;		// click area of the QNH

	std::string m_sStation;
	std::string m_sText;
	COLORREF    m_Colour;

	ULONGLONG   m_FlashStart;	// time (GetTickCount64) when flashing started
	ULONGLONG   m_FlashEnd;		// end of flashing; 0 = not flashing

	ULONGLONG   m_QnhFlashStart;	// the QNH flashes from this time (GetTickCount64) ...
	bool        m_bQnhAlert;		// ... until the user acknowledges it with a click

	std::string m_sAtis;		// ATIS letter(s) at the left of the header

	std::vector<std::string> m_RvrLines;	// RVR rows (expanded only)

	std::string m_sWind;		// wind group in the row above the large text
	std::string m_sQnh;			// QNH in the row above the large text

	std::string m_sNoteBefore;	// note text: normal, bold, normal
	std::string m_sNoteBold;
	std::string m_sNoteAfter;

	std::vector<std::string> m_WindLines;	// wind components, one line per runway
	std::vector<bool> m_WindWarn;			// true = line orange

	std::string m_sDetail;		// line with the evaluated values at the very bottom
};
