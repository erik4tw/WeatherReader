// WeatherBox.cpp: Small info box for the WeatherReader plugin

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
	, m_Colour(RGB(255, 255, 255))
	, m_FlashStart(0)
	, m_FlashEnd(0)
	, m_QnhFlashStart(0)
	, m_bQnhAlert(false)
{
}

void CWeatherBox::SetContent(const char* sStation, const char* sText, COLORREF colour)
{
	m_sStation = sStation;
	m_sText = sText;
	m_Colour = colour;

	m_sNoteBefore.clear();
	m_sNoteBold.clear();
	m_sNoteAfter.clear();
	m_WindLines.clear();
	m_WindWarn.clear();
	m_sDetail.clear();
	m_sWind.clear();
	m_sQnh.clear();
	m_sAtis.clear();
	m_RvrLines.clear();
}

void CWeatherBox::SetWindLines(const std::vector<std::string>& lines, const std::vector<bool>& warn)
{
	m_WindLines = lines;
	m_WindWarn = warn;
}

void CWeatherBox::SetDetail(const char* sDetail)
{
	m_sDetail = sDetail;
}

void CWeatherBox::CopyContentFrom(const CWeatherBox& other)
{
	m_bVisible = other.m_bVisible;
	m_sStation = other.m_sStation;
	m_sText = other.m_sText;
	m_Colour = other.m_Colour;
	m_sNoteBefore = other.m_sNoteBefore;
	m_sNoteBold = other.m_sNoteBold;
	m_sNoteAfter = other.m_sNoteAfter;
	m_sDetail = other.m_sDetail;
	m_WindLines = other.m_WindLines;
	m_WindWarn = other.m_WindWarn;
	m_FlashStart = other.m_FlashStart;
	m_FlashEnd = other.m_FlashEnd;
	m_QnhFlashStart = other.m_QnhFlashStart;
	m_bQnhAlert = other.m_bQnhAlert;
	m_sWind = other.m_sWind;
	m_sQnh = other.m_sQnh;
	m_sAtis = other.m_sAtis;
	m_RvrLines = other.m_RvrLines;
}

void CWeatherBox::SetPosition(int left, int top)
{
	m_rc.left = left;
	m_rc.top = top;
	m_bPlaced = true;
	m_bUserPlaced = true;
}

void CWeatherBox::StartFlash(unsigned int durationMs)
{
	m_FlashStart = GetTickCount64();
	m_FlashEnd = m_FlashStart + durationMs;
}

void CWeatherBox::StartQnhAlert()
{
	if (m_bQnhAlert)
		return;		// already flashing: keep the rhythm

	m_QnhFlashStart = GetTickCount64();
	m_bQnhAlert = true;
}

void CWeatherBox::SetInfo(const char* sWind, const char* sQnh)
{
	m_sWind = sWind;
	m_sQnh = sQnh;
}

void CWeatherBox::SetRvr(const std::vector<std::string>& lines)
{
	m_RvrLines = lines;
}

void CWeatherBox::SetAtis(const char* sAtis)
{
	m_sAtis = sAtis;
}

void CWeatherBox::SetNote(const char* sBefore, const char* sBold, const char* sAfter)
{
	m_sNoteBefore = sBefore;
	m_sNoteBold = sBold;
	m_sNoteAfter = sAfter;
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

	int dpi = GetDeviceCaps(hDC, LOGPIXELSY);
	int stationHeight = ScaleDpi(STATION_HEIGHT, dpi);
	int sidePadding = ScaleDpi(SIDE_PADDING, dpi);
	int toggleWidth = ScaleDpi(TOGGLE_WIDTH, dpi);
	int arrowSize = ScaleDpi(ARROW_SIZE, dpi);

	// Flash phase: while flashing, the coloured area alternates between "on" and "off"
	ULONGLONG now = GetTickCount64();
	bool flashOn = (now < m_FlashEnd) && (((now - m_FlashStart) / FLASH_HALF_PERIOD_MS) % 2 == 0);
	bool qnhFlashOn = m_bQnhAlert && (now >= m_QnhFlashStart) && (((now - m_QnhFlashStart) / FLASH_HALF_PERIOD_MS) % 2 == 0);
	SetRectEmpty(&m_rcQnh);		// set again below if there is a QNH
	bool flashHeader = flashOn && m_bCollapsed;				// when collapsed the header flashes
	bool flashText = flashOn && !m_bCollapsed;				// when expanded the large text flashes

	// Note and value line only exist if they have text
	bool hasNote = !m_sNoteBefore.empty() || !m_sNoteBold.empty() || !m_sNoteAfter.empty();
	int noteHeight = hasNote ? ScaleDpi(NOTE_HEIGHT, dpi) : 0;

	bool hasDetail = !m_sDetail.empty();
	int detailHeight = hasDetail ? ScaleDpi(DETAIL_HEIGHT, dpi) : 0;

	bool hasInfo = !m_sWind.empty() || !m_sQnh.empty();
	int infoHeight = hasInfo ? ScaleDpi(INFO_HEIGHT, dpi) : 0;

	// The RVR rows only exist when expanded; the width is reserved in any case (see below)
	int rvrLineHeight = ScaleDpi(RVR_LINE_HEIGHT, dpi);
	int rvrHeight = m_bCollapsed ? 0 : (int)m_RvrLines.size() * rvrLineHeight;

	int windLineHeight = ScaleDpi(WIND_LINE_HEIGHT, dpi);
	int windHeight = (int)m_WindLines.size() * windLineHeight;

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

	std::string header = "WX " + m_sStation;

	// Determine the size of the box: at least BOX_WIDTH, wider if needed. The header is centred
	// and needs room on both sides: the arrow on the right, the ATIS letter on the left (same
	// width on both sides, so that the header stays centred).
	int atisWidth = m_sAtis.empty() ? 0 : MeasureText(hDC, m_sAtis, hAtisFont) + sidePadding + sidePadding / 2;
	int headerSide = (atisWidth > toggleWidth) ? atisWidth : toggleWidth;
	int width  = ScaleDpi(BOX_WIDTH, dpi);
	int headerWidth = MeasureText(hDC, header, hStationFont) + 2 * headerSide;
	int noteWidth = MeasureText(hDC, m_sNoteBefore, hNoteFont) + MeasureText(hDC, m_sNoteBold, hNoteBoldFont) +
		MeasureText(hDC, m_sNoteAfter, hNoteFont) + 2 * sidePadding;
	int detailWidth = MeasureText(hDC, m_sDetail, hDetailFont) + 2 * sidePadding;
	if (headerWidth > width)  width = headerWidth;
	if (noteWidth > width)    width = noteWidth;
	if (detailWidth > width)  width = detailWidth;
	int infoWidth = MeasureText(hDC, m_sWind, hInfoFont) + MeasureText(hDC, m_sQnh, hInfoFont) + 2 * sidePadding;
	if (!m_sWind.empty() && !m_sQnh.empty())
		infoWidth += ScaleDpi(INFO_GAP, dpi);
	if (infoWidth > width)    width = infoWidth;
	for (size_t i = 0; i < m_RvrLines.size(); i++)
	{
		int lineWidth = MeasureText(hDC, m_RvrLines[i], hRvrFont) + 2 * sidePadding;
		if (lineWidth > width)
			width = lineWidth;
	}
	for (size_t i = 0; i < m_WindLines.size(); i++)
	{
		int lineWidth = MeasureText(hDC, m_WindLines[i], hWindFont) + 2 * sidePadding;
		if (lineWidth > width)
			width = lineWidth;
	}

	int height = ScaleDpi(BOX_HEIGHT, dpi) + infoHeight + rvrHeight + noteHeight + windHeight + detailHeight;

	// When collapsed only the header and the wind/QNH row are visible
	if (m_bCollapsed)
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

	// Top: header "WX <station>"; when the collapsed box flashes, on a coloured area
	// Only "WX <station>" is coloured like the weather state (so that the state can be seen when collapsed);
	// the rest of the header (time) and the arrow stay grey.
	COLORREF headerColour = RGB(170, 170, 170);
	COLORREF stationColour = m_Colour;
	if (flashHeader)
	{
		RECT rcHeaderFill = { m_rc.left + 1, m_rc.top + 1, m_rc.right - 1, m_rc.top + stationHeight };
		HBRUSH hHeaderBrush = CreateSolidBrush(m_Colour);
		FillRect(hDC, &rcHeaderFill, hHeaderBrush);
		DeleteObject(hHeaderBrush);
		headerColour = RGB(30, 30, 30);
		stationColour = headerColour;
	}

	// The header is split at the first space of the station label ("EDDF" | " <dot> 1420z") and centred as a whole.
	size_t splitAt = m_sStation.find(' ');
	std::string headerColoured = "WX " + (splitAt == std::string::npos ? m_sStation : m_sStation.substr(0, splitAt));
	std::string headerGrey = (splitAt == std::string::npos) ? std::string() : m_sStation.substr(splitAt);
	int colouredWidth = MeasureText(hDC, headerColoured, hStationFont);
	int headerLeft = (m_rc.left + m_rc.right - (colouredWidth + MeasureText(hDC, headerGrey, hStationFont))) / 2;

	RECT rcColoured = { headerLeft, m_rc.top + 2, m_rc.right, m_rc.top + stationHeight };
	DrawBoxText(hDC, headerColoured, rcColoured, hStationFont, stationColour, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	RECT rcGrey = { headerLeft + colouredWidth, m_rc.top + 2, m_rc.right, m_rc.top + stationHeight };
	DrawBoxText(hDC, headerGrey, rcGrey, hStationFont, headerColour, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	// ATIS letter(s) at the top left (bold, light blue; dark while the collapsed box flashes)
	if (!m_sAtis.empty())
	{
		RECT rcAtis = { m_rc.left + sidePadding, m_rc.top + 2, m_rc.left + sidePadding + atisWidth, m_rc.top + stationHeight };
		DrawBoxText(hDC, m_sAtis, rcAtis, hAtisFont, flashHeader ? headerColour : RGB(120, 190, 255),
			DT_LEFT | DT_VCENTER | DT_SINGLELINE);
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

	if (hasInfo)
	{
		// Wind group (left) and QNH (right) in bold below the header, also when collapsed. A changed QNH flashes inverted until it is clicked.
		int infoTop = m_rc.top + stationHeight;
		RECT rcInfo = { m_rc.left + sidePadding, infoTop, m_rc.right - sidePadding, infoTop + infoHeight };
		const COLORREF infoColour = RGB(235, 235, 235);

		DrawBoxText(hDC, m_sWind, rcInfo, hInfoFont, infoColour, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

		int pad = ScaleDpi(INFO_FLASH_PAD, dpi);
		int qnhWidth = MeasureText(hDC, m_sQnh, hInfoFont);
		if (!m_sQnh.empty())
		{
			// Click area to acknowledge a changed QNH: the QNH with some room around it, up to the edge of the box
			m_rcQnh.left = rcInfo.right - qnhWidth - 2 * pad;
			m_rcQnh.right = m_rc.right;
			m_rcQnh.top = infoTop;
			m_rcQnh.bottom = infoTop + infoHeight;
		}

		COLORREF qnhColour = infoColour;
		if (qnhFlashOn && !m_sQnh.empty())
		{
			RECT rcQnhFill = { rcInfo.right - qnhWidth - pad, infoTop + ScaleDpi(2, dpi),
				rcInfo.right + pad, infoTop + infoHeight - ScaleDpi(2, dpi) };
			HBRUSH hQnhBrush = CreateSolidBrush(infoColour);
			FillRect(hDC, &rcQnhFill, hQnhBrush);
			DeleteObject(hQnhBrush);
			qnhColour = RGB(30, 30, 30);
		}
		DrawBoxText(hDC, m_sQnh, rcInfo, hInfoFont, qnhColour, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
	}

	// Below: text (only when expanded)
	if (!m_bCollapsed)
	{
		// RVR rows directly below the wind/QNH row (bold, centred)
		for (size_t i = 0; i < m_RvrLines.size(); i++)
		{
			int top = m_rc.top + stationHeight + infoHeight + (int)i * rvrLineHeight;
			RECT rcRvr = { m_rc.left, top, m_rc.right, top + rvrLineHeight };
			DrawBoxText(hDC, m_RvrLines[i], rcRvr, hRvrFont, RGB(235, 235, 235), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}

		int textTop = m_rc.top + stationHeight + infoHeight + rvrHeight;
		RECT rcText = { m_rc.left, textTop, m_rc.right, m_rc.bottom - noteHeight - windHeight - detailHeight - 2 };
		COLORREF textColour = m_Colour;

		if (flashText)
		{
			// Flashing: coloured area (inside the frame) with dark text
			RECT rcFill = { m_rc.left + 1, textTop, m_rc.right - 1,
				(noteHeight + windHeight + detailHeight > 0) ? m_rc.bottom - noteHeight - windHeight - detailHeight : m_rc.bottom - 1 };
			HBRUSH hFillBrush = CreateSolidBrush(m_Colour);
			FillRect(hDC, &rcFill, hFillBrush);
			DeleteObject(hFillBrush);

			rcText = rcFill;
			textColour = RGB(30, 30, 30);
		}

		DrawBoxText(hDC, m_sText, rcText, hTextFont, textColour, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		if (hasNote)
		{
			// Note: three parts (normal, bold, normal) side by side, centred as a whole
			HGDIOBJ hPreviousFont = SelectObject(hDC, hNoteFont);

			const std::wstring parts[3] = { ToWide(m_sNoteBefore), ToWide(m_sNoteBold), ToWide(m_sNoteAfter) };
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
		for (size_t i = 0; i < m_WindLines.size(); i++)
		{
			int top = m_rc.bottom - detailHeight - windHeight - 2 + (int)i * windLineHeight;
			RECT rcWind = { m_rc.left, top, m_rc.right, top + windLineHeight };
			bool warn = (i < m_WindWarn.size()) && m_WindWarn[i];
			DrawBoxText(hDC, m_WindLines[i], rcWind, hWindFont, warn ? RGB(255, 140, 0) : RGB(170, 170, 170),
				DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		}

		if (hasDetail)
		{
			// At the very bottom: small line with the evaluated values
			RECT rcDetail = { m_rc.left, m_rc.bottom - detailHeight - 2, m_rc.right, m_rc.bottom - 2 };
			DrawBoxText(hDC, m_sDetail, rcDetail, hDetailFont, RGB(170, 170, 170),
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
