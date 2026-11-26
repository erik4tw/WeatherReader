// WeatherBox.cpp: Small info box for the WeatherReader plugin (see WeatherBox.h)

#include "pch.h"
#include "WeatherBox.h"

namespace
{
	// Layout in pixels at 96 dpi (adapted to the screen scaling)
	const int STATION_HEIGHT   = 28;	// top area with the station
	const int STATION_FONT     = 15;	// font height of the header
	const int SIDE_PADDING     = 8;		// minimum distance of note and value line to the edge
	const int TOGGLE_WIDTH     = 24;	// width of the click area for the collapse/expand arrow
	const int ARROW_SIZE       = 5;		// half the size of the arrow
	const int START_MARGIN_RIGHT = 24;	// start position: distance to the right edge
	const int START_MARGIN_TOP   = 24;	// start position: distance to the top edge

	// Flashing: how long the coloured area is on or off (ES redraws about once per second)
	const ULONGLONG FLASH_HALF_PERIOD_MS = 1000;

	// Note below the large text (e.g. "RRS possible")
	const int NOTE_HEIGHT      = 18;
	const int NOTE_FONT        = 12;

	// Wind lines (one per runway) below the note
	const int WIND_LINE_HEIGHT = 16;
	const int WIND_FONT        = 12;

	// Line with the evaluated values at the very bottom (e.g. "CEIL 1200 VIS 9999")
	const int DETAIL_HEIGHT    = 16;
	const int DETAIL_FONT      = 11;

	// Row with wind group and QNH above the large text (bold)
	const int INFO_HEIGHT      = 24;
	const int INFO_FONT        = 15;
	const int ATIS_FONT        = 19;	// ATIS letter(s) in the header

	// RVR rows below the wind/QNH row (bold, only when expanded)
	const int RVR_LINE_HEIGHT  = 20;
	const int RVR_FONT         = 14;
	const int INFO_GAP         = 14;	// minimum gap between wind group and QNH
	const int INFO_FLASH_PAD   = 4;		// padding of the inverted QNH area around the text

	// Several airports: rows (collapsed) and tabs (expanded)
	const int ROW_HEIGHT       = 24;	// one airport row when collapsed
	const int ROW_GAP          = 12;	// gap between the columns ICAO, wind and QNH of a row
	const int TAB_HEIGHT       = 28;	// row of tabs below the header when expanded
	const int TAB_PAD          = 10;	// space left and right of the ICAO code in a tab
	const COLORREF ROW_SELECTED_BACK = RGB(48, 48, 48);

	// The state flash of the airport shown in detail stops by itself after this time (the one of the other
	// airports lasts until it is clicked)
	const ULONGLONG FLASH_SELECTED_MS = 10000;

	// Large text (e.g. "VMC") and size of the box without note and value line.
	// BOX_WIDTH is the minimum width; if the header, note or value line is wider, the box grows.
	const int BOX_WIDTH        = 140;
	const int BOX_HEIGHT       = 92;
	const int LARGE_FONT       = 40;

	int ScaleDpi(int value, int dpi)
	{
		return MulDiv(value, dpi, 96);
	}

	HFONT CreateBoxFont(int height, int weight)
	{
		return CreateFontA(-height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_SWISS, "Segoe UI");
	}

	// Converts a UTF-8 text (all texts of the box) to UTF-16 for the Unicode GDI functions. The Unicode
	// functions are used on purpose: the ANSI ones depend on the code page of the system, which
	// would turn characters such as the middle dot into question marks on some systems.
	std::wstring ToWide(const std::string& utf8)
	{
		if (utf8.empty())
			return std::wstring();

		int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
		std::wstring wide(length > 0 ? length : 0, L'\0');
		if (length > 0)
			MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wide[0], length);
		return wide;
	}

	// Width of text in pixels when drawn with the font hFont.
	int MeasureText(HDC hDC, const std::string& text, HFONT hFont)
	{
		if (text.empty())
			return 0;

		std::wstring wide = ToWide(text);
		HGDIOBJ hOldFont = SelectObject(hDC, hFont);
		SIZE size = { 0, 0 };
		GetTextExtentPoint32W(hDC, wide.c_str(), (int)wide.size(), &size);
		SelectObject(hDC, hOldFont);
		return size.cx;
	}

	// Is the state flash of an airport in its "on" phase? capSelected: the airport is shown in detail, so the flash
	// ends after FLASH_SELECTED_MS at the latest.
	bool StateFlashOn(const WeatherBoxAlert& alert, ULONGLONG now, bool capSelected)
	{
		ULONGLONG end = alert.flashEnd;
		if (capSelected && end != 0 && alert.flashStart + FLASH_SELECTED_MS < end)
			end = alert.flashStart + FLASH_SELECTED_MS;
		return now < end && now >= alert.flashStart && (((now - alert.flashStart) / FLASH_HALF_PERIOD_MS) % 2 == 0);
	}

	// Is the QNH flash of an airport in its "on" phase?
	bool QnhFlashOn(const WeatherBoxAlert& alert, ULONGLONG now)
	{
		return alert.qnhAlert && now >= alert.qnhFlashStart && (((now - alert.qnhFlashStart) / FLASH_HALF_PERIOD_MS) % 2 == 0);
	}

	// Draws text with the font hFont in rc (the previously selected font is restored).
	void DrawBoxText(HDC hDC, const std::string& text, RECT rc, HFONT hFont, COLORREF colour, UINT format)
	{
		std::wstring wide = ToWide(text);
		HGDIOBJ hOldFont = SelectObject(hDC, hFont);

		SetTextColor(hDC, colour);
		DrawTextW(hDC, wide.c_str(), -1, &rc, format | DT_NOPREFIX);

		SelectObject(hDC, hOldFont);
	}
}

CWeatherBox::CWeatherBox()
	: m_bVisible(true)
	, m_bCollapsed(false)
	, m_bPlaced(false)
	, m_bUserPlaced(false)
	, m_rc()
	, m_rcToggle()
	, m_rcQnh()
{
}

void CWeatherBox::SetEntries(const std::vector<WeatherBoxEntry>& entries)
{
	m_Entries = entries;
}

void CWeatherBox::CopyContentFrom(const CWeatherBox& other)
{
	m_bVisible = other.m_bVisible;
	m_Entries = other.m_Entries;
}

size_t CWeatherBox::SelectedIndex() const
{
	for (size_t i = 0; i < m_Entries.size(); i++)
		if (m_Entries[i].icao == m_sSelected)
			return i;
	return 0;
}

void CWeatherBox::SelectAirport(const std::string& icao)
{
	for (size_t i = 0; i < m_Entries.size(); i++)
	{
		if (m_Entries[i].icao == icao)
		{
			m_sSelected = icao;
			return;
		}
	}
}

std::string CWeatherBox::GetSelectedIcao() const
{
	if (m_Entries.empty())
		return std::string();
	return m_Entries[SelectedIndex()].icao;
}

void CWeatherBox::AcknowledgeAirport(const std::string& icao, bool qnhOnly)
{
	for (size_t i = 0; i < m_Entries.size(); i++)
		if (m_Entries[i].icao == icao)
			m_Entries[i].alert.Acknowledge(qnhOnly);
}

bool CWeatherBox::IsQnhAlert() const
{
	if (m_Entries.empty() || IsRectEmpty(&m_rcQnh))
		return false;
	if (m_Entries.size() > 1 && m_bCollapsed)
		return false;		// the rows have their own click areas
	return m_Entries[SelectedIndex()].alert.qnhAlert;
}

void CWeatherBox::SetPosition(int left, int top)
{
	m_rc.left = left;
	m_rc.top = top;
	m_bPlaced = true;
	m_bUserPlaced = true;
}

void CWeatherBox::Clamp(const RECT& area)
{
	int width  = m_rc.right - m_rc.left;
	int height = m_rc.bottom - m_rc.top;

	// If the allowed area is smaller than the box, the top left corner takes precedence.
	if (m_rc.right > area.right)   { m_rc.right = area.right;   m_rc.left = m_rc.right - width; }
	if (m_rc.bottom > area.bottom) { m_rc.bottom = area.bottom; m_rc.top = m_rc.bottom - height; }
	if (m_rc.left < area.left)     { m_rc.left = area.left;     m_rc.right = m_rc.left + width; }
	if (m_rc.top < area.top)       { m_rc.top = area.top;       m_rc.bottom = m_rc.top + height; }
}

void CWeatherBox::MoveTo(const RECT& newPos, const RECT& area)
{
	int width  = m_rc.right - m_rc.left;
	int height = m_rc.bottom - m_rc.top;

	m_rc.left = newPos.left;
	m_rc.top = newPos.top;
	m_rc.right = m_rc.left + width;
	m_rc.bottom = m_rc.top + height;

	Clamp(area);
	m_bUserPlaced = true;
}

bool CWeatherBox::Draw(HDC hDC, const RECT& area)
{
	if (!m_bVisible)
		return false;

	// The entries to show; without any, a single empty entry (dash)
	std::vector<WeatherBoxEntry> placeholder;
	const std::vector<WeatherBoxEntry>* pList = &m_Entries;
	if (m_Entries.empty())
	{
		WeatherBoxEntry empty;
		empty.station = "-";
		empty.text = "-";
		empty.colour = RGB(170, 170, 170);
		placeholder.push_back(empty);
		pList = &placeholder;
	}
	const std::vector<WeatherBoxEntry>& list = *pList;

	const bool multi = list.size() > 1;
	const size_t sel = SelectedIndex();
	const WeatherBoxEntry& e = list[sel < list.size() ? sel : 0];
	const bool listMode = multi && m_bCollapsed;	// airport rows instead of the detail view

	int dpi = GetDeviceCaps(hDC, LOGPIXELSY);
	int stationHeight = ScaleDpi(STATION_HEIGHT, dpi);
	int sidePadding = ScaleDpi(SIDE_PADDING, dpi);
	int toggleWidth = ScaleDpi(TOGGLE_WIDTH, dpi);
	int arrowSize = ScaleDpi(ARROW_SIZE, dpi);
	int rowHeight = ScaleDpi(ROW_HEIGHT, dpi);
	int rowGap = ScaleDpi(ROW_GAP, dpi);
	int tabPad = ScaleDpi(TAB_PAD, dpi);

	// Tabs below the header (several airports, expanded); everything below starts at bodyTop
	int tabHeight = (multi && !m_bCollapsed) ? ScaleDpi(TAB_HEIGHT, dpi) : 0;
	int bodyTop = stationHeight + tabHeight;

	// Flash phase: while flashing, the coloured area alternates between "on" and "off"
	ULONGLONG now = GetTickCount64();
	bool flashOn = StateFlashOn(e.alert, now, multi);
	bool qnhFlashOn = QnhFlashOn(e.alert, now);
	SetRectEmpty(&m_rcQnh);		// set again below if there is a QNH
	m_Hits.clear();
	bool flashHeader = flashOn && m_bCollapsed && !multi;	// when collapsed (one airport) the header flashes
	bool flashText = flashOn && !m_bCollapsed;				// when expanded the large text flashes

	// Note and value line only exist if they have text
	bool hasNote = !e.noteBefore.empty() || !e.noteBold.empty() || !e.noteAfter.empty();
	int noteHeight = hasNote ? ScaleDpi(NOTE_HEIGHT, dpi) : 0;

	bool hasDetail = !e.detail.empty();
	int detailHeight = hasDetail ? ScaleDpi(DETAIL_HEIGHT, dpi) : 0;

	bool hasInfo = !e.wind.empty() || !e.qnh.empty();
	int infoHeight = hasInfo ? ScaleDpi(INFO_HEIGHT, dpi) : 0;

	// The RVR rows only exist when expanded; the width is reserved in any case (see below)
	int rvrLineHeight = ScaleDpi(RVR_LINE_HEIGHT, dpi);
	int rvrHeight = m_bCollapsed ? 0 : (int)e.rvrLines.size() * rvrLineHeight;

	int windLineHeight = ScaleDpi(WIND_LINE_HEIGHT, dpi);
	int windHeight = (int)e.windLines.size() * windLineHeight;

	// Save the state of the EuroScope device context and restore it at the end
	int savedDC = SaveDC(hDC);

	HFONT hStationFont = CreateBoxFont(ScaleDpi(STATION_FONT, dpi), FW_NORMAL);
	HFONT hTextFont = CreateBoxFont(ScaleDpi(LARGE_FONT, dpi), FW_BOLD);
	HFONT hNoteFont = CreateBoxFont(ScaleDpi(NOTE_FONT, dpi), FW_NORMAL);
	HFONT hNoteBoldFont = CreateBoxFont(ScaleDpi(NOTE_FONT, dpi), FW_BOLD);
	HFONT hDetailFont = CreateBoxFont(ScaleDpi(DETAIL_FONT, dpi), FW_NORMAL);
	HFONT hWindFont = CreateBoxFont(ScaleDpi(WIND_FONT, dpi), FW_NORMAL);
	HFONT hInfoFont = CreateBoxFont(ScaleDpi(INFO_FONT, dpi), FW_BOLD);
	HFONT hAtisFont = CreateBoxFont(ScaleDpi(ATIS_FONT, dpi), FW_BOLD);
	HFONT hRvrFont = CreateBoxFont(ScaleDpi(RVR_FONT, dpi), FW_BOLD);

	// Determine the size of the box: at least BOX_WIDTH, wider if needed. The width is the widest of ALL airports,
	// so that the box keeps its width when another airport is selected or the box is collapsed. The header is
	// centred and needs room on both sides: the arrow on the right, the ATIS letter on the left (same
	// width on both sides, so that the header stays centred).
	int width = ScaleDpi(BOX_WIDTH, dpi);
	int icaoColumn = 0;		// width of the ICAO column of the rows
	int tabsWidth = 0;		// width of all tabs together
	for (size_t i = 0; i < list.size(); i++)
	{
		const WeatherBoxEntry& x = list[i];

		int atisWidth = x.atis.empty() ? 0 : MeasureText(hDC, x.atis, hAtisFont) + sidePadding + sidePadding / 2;
		int headerSide = (atisWidth > toggleWidth) ? atisWidth : toggleWidth;
		int headerWidth = MeasureText(hDC, "WX " + x.station, hStationFont) + 2 * headerSide;
		int noteWidth = MeasureText(hDC, x.noteBefore, hNoteFont) + MeasureText(hDC, x.noteBold, hNoteBoldFont) +
			MeasureText(hDC, x.noteAfter, hNoteFont) + 2 * sidePadding;
		int detailWidth = MeasureText(hDC, x.detail, hDetailFont) + 2 * sidePadding;
		int infoWidth = MeasureText(hDC, x.wind, hInfoFont) + MeasureText(hDC, x.qnh, hInfoFont) + 2 * sidePadding;
		if (!x.wind.empty() && !x.qnh.empty())
			infoWidth += ScaleDpi(INFO_GAP, dpi);

		if (headerWidth > width)  width = headerWidth;
		if (noteWidth > width)    width = noteWidth;
		if (detailWidth > width)  width = detailWidth;
		if (infoWidth > width)    width = infoWidth;
		for (size_t j = 0; j < x.rvrLines.size(); j++)
		{
			int lineWidth = MeasureText(hDC, x.rvrLines[j], hRvrFont) + 2 * sidePadding;
			if (lineWidth > width)
				width = lineWidth;
		}
		for (size_t j = 0; j < x.windLines.size(); j++)
		{
			int lineWidth = MeasureText(hDC, x.windLines[j], hWindFont) + 2 * sidePadding;
			if (lineWidth > width)
				width = lineWidth;
		}

		if (multi)
		{
			int icaoWidth = MeasureText(hDC, x.icao, hInfoFont);
			if (icaoWidth > icaoColumn)
				icaoColumn = icaoWidth;
			tabsWidth += icaoWidth + 2 * tabPad;
		}
	}
	if (multi)
	{
		// Rows: ICAO column, wind group and QNH side by side; tabs side by side
		for (size_t i = 0; i < list.size(); i++)
		{
			const WeatherBoxEntry& x = list[i];
			int rowWidth = icaoColumn + rowGap + MeasureText(hDC, x.wind, hInfoFont) + rowGap +
				MeasureText(hDC, x.qnh, hInfoFont) + 2 * sidePadding;
			if (rowWidth > width)
				width = rowWidth;
		}
		if (tabsWidth + 2 * sidePadding > width)
			width = tabsWidth + 2 * sidePadding;
	}

	int height = ScaleDpi(BOX_HEIGHT, dpi) + tabHeight + infoHeight + rvrHeight + noteHeight + windHeight + detailHeight;

	// When collapsed only the header and the wind/QNH row (one airport) or the airport rows (several) are visible
	if (listMode)
		height = stationHeight + (int)list.size() * rowHeight + ScaleDpi(2, dpi);
	else if (m_bCollapsed)
		height = stationHeight + infoHeight;

	// On the first draw: start position at the top right of the allowed area
	if (!m_bPlaced)
	{
		m_rc.left = area.right - width - ScaleDpi(START_MARGIN_RIGHT, dpi);
		m_rc.top = area.top + ScaleDpi(START_MARGIN_TOP, dpi);
		m_bPlaced = true;
	}

	m_rc.right = m_rc.left + width;
	m_rc.bottom = m_rc.top + height;
	Clamp(area);		// also makes the box visible again after the EuroScope window was made smaller

	// Background and frame
	HBRUSH hBackBrush = CreateSolidBrush(RGB(30, 30, 30));
	FillRect(hDC, &m_rc, hBackBrush);
	DeleteObject(hBackBrush);

	HBRUSH hBorderBrush = CreateSolidBrush(RGB(100, 100, 100));
	FrameRect(hDC, &m_rc, hBorderBrush);
	DeleteObject(hBorderBrush);

	SetBkMode(hDC, TRANSPARENT);

	const COLORREF darkColour = RGB(30, 30, 30);
	const COLORREF infoColour = RGB(235, 235, 235);

	// Top: header "WX <station>"; when the collapsed box flashes, on a coloured area
	// Only "WX <station>" is coloured like the weather state (so that the state can be seen when collapsed);
	// the rest of the header (time) and the arrow stay grey.
	COLORREF headerColour = RGB(170, 170, 170);
	COLORREF stationColour = e.colour;
	if (flashHeader)
	{
		RECT rcHeaderFill = { m_rc.left + 1, m_rc.top + 1, m_rc.right - 1, m_rc.top + stationHeight };
		HBRUSH hHeaderBrush = CreateSolidBrush(e.colour);
		FillRect(hDC, &rcHeaderFill, hHeaderBrush);
		DeleteObject(hHeaderBrush);
		headerColour = darkColour;
		stationColour = headerColour;
	}

	if (listMode)
	{
		// Several airports, collapsed: only "WX" in the header (the airports are in the rows below)
		RECT rcHeader = { m_rc.left, m_rc.top + 2, m_rc.right, m_rc.top + stationHeight };
		DrawBoxText(hDC, "WX", rcHeader, hStationFont, headerColour, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}
	else
	{
		// The header is split at the first space of the station label ("EDDF" | " <dot> 1420z") and centred as a whole.
		size_t splitAt = e.station.find(' ');
		std::string headerColoured = "WX " + (splitAt == std::string::npos ? e.station : e.station.substr(0, splitAt));
		std::string headerGrey = (splitAt == std::string::npos) ? std::string() : e.station.substr(splitAt);
		int colouredWidth = MeasureText(hDC, headerColoured, hStationFont);
		int headerLeft = (m_rc.left + m_rc.right - (colouredWidth + MeasureText(hDC, headerGrey, hStationFont))) / 2;

		RECT rcColoured = { headerLeft, m_rc.top + 2, m_rc.right, m_rc.top + stationHeight };
		DrawBoxText(hDC, headerColoured, rcColoured, hStationFont, stationColour, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
		RECT rcGrey = { headerLeft + colouredWidth, m_rc.top + 2, m_rc.right, m_rc.top + stationHeight };
		DrawBoxText(hDC, headerGrey, rcGrey, hStationFont, headerColour, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

		// ATIS letter(s) at the top left (bold, light blue; dark while the collapsed box flashes)
		if (!e.atis.empty())
		{
			int atisWidth = MeasureText(hDC, e.atis, hAtisFont) + sidePadding + sidePadding / 2;
			RECT rcAtis = { m_rc.left + sidePadding, m_rc.top + 2, m_rc.left + sidePadding + atisWidth, m_rc.top + stationHeight };
			DrawBoxText(hDC, e.atis, rcAtis, hAtisFont, flashHeader ? headerColour : RGB(120, 190, 255),
				DT_LEFT | DT_VCENTER | DT_SINGLELINE);
		}
	}

	// Arrow at the top right for collapsing/expanding: down = expanded, right = collapsed
	m_rcToggle.left = m_rc.right - toggleWidth;
	m_rcToggle.right = m_rc.right;
	m_rcToggle.top = m_rc.top;
	m_rcToggle.bottom = m_rc.top + stationHeight;

	int cx = (m_rcToggle.left + m_rcToggle.right) / 2;
	int cy = (m_rcToggle.top + m_rcToggle.bottom) / 2;
	POINT arrow[3];
	if (m_bCollapsed)
	{
		arrow[0].x = cx - arrowSize / 2;	arrow[0].y = cy - arrowSize;
		arrow[1].x = cx - arrowSize / 2;	arrow[1].y = cy + arrowSize;
		arrow[2].x = cx + arrowSize;		arrow[2].y = cy;
	}
	else
	{
		arrow[0].x = cx - arrowSize;		arrow[0].y = cy - arrowSize / 2;
		arrow[1].x = cx + arrowSize;		arrow[1].y = cy - arrowSize / 2;
		arrow[2].x = cx;					arrow[2].y = cy + arrowSize;
	}
	HBRUSH hArrowBrush = CreateSolidBrush(headerColour);
	SelectObject(hDC, hArrowBrush);
	SelectObject(hDC, GetStockObject(NULL_PEN));
	Polygon(hDC, arrow, 3);
	SelectObject(hDC, GetStockObject(WHITE_BRUSH));		// release the brush so that it can be deleted
	DeleteObject(hArrowBrush);

	int pad = ScaleDpi(INFO_FLASH_PAD, dpi);

	if (listMode)
	{
		// Several airports, collapsed: one row per airport with ICAO (state colour), wind group and QNH.
		// A row whose state has changed is filled with the state colour until it is clicked; a changed QNH
		// flashes inverted as in the single view.
		for (size_t i = 0; i < list.size(); i++)
		{
			const WeatherBoxEntry& x = list[i];
			int top = m_rc.top + stationHeight + (int)i * rowHeight;
			RECT rcRow = { m_rc.left + 1, top, m_rc.right - 1, top + rowHeight };

			bool rowFlash = StateFlashOn(x.alert, now, false);
			bool rowQnhFlash = QnhFlashOn(x.alert, now);

			if (rowFlash || i == sel)
			{
				HBRUSH hRowBrush = CreateSolidBrush(rowFlash ? x.colour : ROW_SELECTED_BACK);
				FillRect(hDC, &rcRow, hRowBrush);
				DeleteObject(hRowBrush);
			}

			RECT rcInfo = { m_rc.left + sidePadding, top, m_rc.right - sidePadding, top + rowHeight };
			DrawBoxText(hDC, x.icao, rcInfo, hInfoFont, rowFlash ? darkColour : x.colour,
				DT_LEFT | DT_VCENTER | DT_SINGLELINE);

			RECT rcWind = rcInfo;
			rcWind.left += icaoColumn + rowGap;
			DrawBoxText(hDC, x.wind, rcWind, hInfoFont, rowFlash ? darkColour : infoColour,
				DT_LEFT | DT_VCENTER | DT_SINGLELINE);

			COLORREF qnhColour = rowFlash ? darkColour : infoColour;
			if (rowQnhFlash && !x.qnh.empty())
			{
				int qnhWidth = MeasureText(hDC, x.qnh, hInfoFont);
				RECT rcQnhFill = { rcInfo.right - qnhWidth - pad, top + ScaleDpi(2, dpi),
					rcInfo.right + pad, top + rowHeight - ScaleDpi(2, dpi) };
				HBRUSH hQnhBrush = CreateSolidBrush(rowFlash ? darkColour : infoColour);
				FillRect(hDC, &rcQnhFill, hQnhBrush);
				DeleteObject(hQnhBrush);
				qnhColour = rowFlash ? x.colour : darkColour;
			}
			DrawBoxText(hDC, x.qnh, rcInfo, hInfoFont, qnhColour, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

			AirportHit hit;
			hit.rc.left = m_rc.left;
			hit.rc.right = m_rc.right;
			hit.rc.top = top;
			hit.rc.bottom = top + rowHeight;
			hit.icao = x.icao;
			m_Hits.push_back(hit);
		}
	}
	else
	{
		if (tabHeight > 0)
		{
			// Several airports, expanded: tabs with the ICAO codes below the header. The selected one has a
			// lighter background and a bar in its state colour; a changed airport (not the selected one) is filled
			// with its state colour (state changed) or inverted (QNH changed) until it is clicked.
			int tabTop = m_rc.top + stationHeight;
			int tabX = (m_rc.left + m_rc.right - tabsWidth) / 2;
			for (size_t i = 0; i < list.size(); i++)
			{
				const WeatherBoxEntry& x = list[i];
				int tabWidth = MeasureText(hDC, x.icao, hInfoFont) + 2 * tabPad;
				RECT rcTab = { tabX, tabTop + ScaleDpi(2, dpi), tabX + tabWidth, tabTop + tabHeight };
				tabX += tabWidth;

				bool isSelected = (i == sel);
				bool tabFlash = !isSelected && StateFlashOn(x.alert, now, false);
				bool tabQnhFlash = !isSelected && QnhFlashOn(x.alert, now);

				COLORREF textColour = x.colour;
				if (tabFlash || tabQnhFlash || isSelected)
				{
					COLORREF back = tabFlash ? x.colour : (tabQnhFlash ? infoColour : ROW_SELECTED_BACK);
					HBRUSH hTabBrush = CreateSolidBrush(back);
					FillRect(hDC, &rcTab, hTabBrush);
					DeleteObject(hTabBrush);
					if (tabFlash || tabQnhFlash)
						textColour = darkColour;
				}
				if (isSelected)
				{
					RECT rcBar = { rcTab.left, rcTab.bottom - ScaleDpi(2, dpi), rcTab.right, rcTab.bottom };
					HBRUSH hBarBrush = CreateSolidBrush(x.colour);
					FillRect(hDC, &rcBar, hBarBrush);
					DeleteObject(hBarBrush);
				}
				DrawBoxText(hDC, x.icao, rcTab, hInfoFont, textColour, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

				AirportHit hit;
				hit.rc = rcTab;
				hit.icao = x.icao;
				m_Hits.push_back(hit);
			}
		}

		if (hasInfo)
		{
			// Wind group (left) and QNH (right) in bold below the header, also when collapsed. A changed QNH flashes inverted until it is clicked.
			int infoTop = m_rc.top + bodyTop;
			RECT rcInfo = { m_rc.left + sidePadding, infoTop, m_rc.right - sidePadding, infoTop + infoHeight };

			DrawBoxText(hDC, e.wind, rcInfo, hInfoFont, infoColour, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

			int qnhWidth = MeasureText(hDC, e.qnh, hInfoFont);
			if (!e.qnh.empty())
			{
				// Click area to acknowledge a changed QNH: the QNH with some room around it, up to the edge of the box
				m_rcQnh.left = rcInfo.right - qnhWidth - 2 * pad;
				m_rcQnh.right = m_rc.right;
				m_rcQnh.top = infoTop;
				m_rcQnh.bottom = infoTop + infoHeight;
			}

			COLORREF qnhColour = infoColour;
			if (qnhFlashOn && !e.qnh.empty())
			{
				RECT rcQnhFill = { rcInfo.right - qnhWidth - pad, infoTop + ScaleDpi(2, dpi),
					rcInfo.right + pad, infoTop + infoHeight - ScaleDpi(2, dpi) };
				HBRUSH hQnhBrush = CreateSolidBrush(infoColour);
				FillRect(hDC, &rcQnhFill, hQnhBrush);
				DeleteObject(hQnhBrush);
				qnhColour = darkColour;
			}
			DrawBoxText(hDC, e.qnh, rcInfo, hInfoFont, qnhColour, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
		}
	}

	// Below: text (only when expanded)
	if (!m_bCollapsed)
	{
		// RVR rows directly below the wind/QNH row (bold, centred)
		for (size_t i = 0; i < e.rvrLines.size(); i++)
		{
			int top = m_rc.top + bodyTop + infoHeight + (int)i * rvrLineHeight;
			RECT rcRvr = { m_rc.left, top, m_rc.right, top + rvrLineHeight };
			DrawBoxText(hDC, e.rvrLines[i], rcRvr, hRvrFont, infoColour, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}

		int textTop = m_rc.top + bodyTop + infoHeight + rvrHeight;
		RECT rcText = { m_rc.left, textTop, m_rc.right, m_rc.bottom - noteHeight - windHeight - detailHeight - 2 };
		COLORREF textColour = e.colour;

		if (flashText)
		{
			// Flashing: coloured area (inside the frame) with dark text
			RECT rcFill = { m_rc.left + 1, textTop, m_rc.right - 1,
				(noteHeight + windHeight + detailHeight > 0) ? m_rc.bottom - noteHeight - windHeight - detailHeight : m_rc.bottom - 1 };
			HBRUSH hFillBrush = CreateSolidBrush(e.colour);
			FillRect(hDC, &rcFill, hFillBrush);
			DeleteObject(hFillBrush);

			rcText = rcFill;
			textColour = darkColour;
		}

		DrawBoxText(hDC, e.text, rcText, hTextFont, textColour, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		if (hasNote)
		{
			// Note: three parts (normal, bold, normal) side by side, centred as a whole
			HGDIOBJ hPreviousFont = SelectObject(hDC, hNoteFont);

			const std::wstring parts[3] = { ToWide(e.noteBefore), ToWide(e.noteBold), ToWide(e.noteAfter) };
			HFONT fonts[3] = { hNoteFont, hNoteBoldFont, hNoteFont };
			SIZE sizes[3] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
			int totalWidth = 0;
			int textHeight = 0;

			for (int i = 0; i < 3; i++)
			{
				if (parts[i].empty())
					continue;
				SelectObject(hDC, fonts[i]);
				GetTextExtentPoint32W(hDC, parts[i].c_str(), (int)parts[i].size(), &sizes[i]);
				totalWidth += sizes[i].cx;
				if (sizes[i].cy > textHeight)
					textHeight = sizes[i].cy;
			}

			int x = (m_rc.left + m_rc.right - totalWidth) / 2;
			int y = m_rc.bottom - detailHeight - windHeight - noteHeight - 2 + (noteHeight - textHeight) / 2;

			SetTextColor(hDC, RGB(170, 170, 170));
			for (int i = 0; i < 3; i++)
			{
				if (parts[i].empty())
					continue;
				SelectObject(hDC, fonts[i]);
				TextOutW(hDC, x, y, parts[i].c_str(), (int)parts[i].size());
				x += sizes[i].cx;
			}

			SelectObject(hDC, hPreviousFont);
		}

		// Wind lines: one per runway, between note and value line
		for (size_t i = 0; i < e.windLines.size(); i++)
		{
			int top = m_rc.bottom - detailHeight - windHeight - 2 + (int)i * windLineHeight;
			RECT rcWind = { m_rc.left, top, m_rc.right, top + windLineHeight };
			bool warn = (i < e.windWarn.size()) && e.windWarn[i];
			DrawBoxText(hDC, e.windLines[i], rcWind, hWindFont, warn ? RGB(255, 140, 0) : RGB(170, 170, 170),
				DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}

		if (hasDetail)
		{
			// At the very bottom: small line with the evaluated values
			RECT rcDetail = { m_rc.left, m_rc.bottom - detailHeight - 2, m_rc.right, m_rc.bottom - 2 };
			DrawBoxText(hDC, e.detail, rcDetail, hDetailFont, RGB(170, 170, 170),
				DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}
	}

	RestoreDC(hDC, savedDC);	// afterwards the fonts are no longer selected and can be deleted
	DeleteObject(hStationFont);
	DeleteObject(hTextFont);
	DeleteObject(hNoteFont);
	DeleteObject(hNoteBoldFont);
	DeleteObject(hDetailFont);
	DeleteObject(hWindFont);
	DeleteObject(hInfoFont);
	DeleteObject(hAtisFont);
	DeleteObject(hRvrFont);
	return true;
}
