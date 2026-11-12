// AtisFetcher.cpp: Fetches the current ATIS letters from the VATSIM data feed in the background

#include "pch.h"
#include "AtisFetcher.h"
#include "AtisParser.h"

#include <winhttp.h>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace
{
	const wchar_t* const FEED_HOST = L"data.vatsim.net";
	const wchar_t* const FEED_PATH = L"/v3/vatsim-data.json";
	const wchar_t* const USER_AGENT = L"WeatherReader/1.2.0 (EuroScope plugin)";

	// The feed is regenerated about every 15 seconds; we ask less often.
	const ULONGLONG POLL_INTERVAL_MS = 30 * 1000;
	const ULONGLONG RETRY_INTERVAL_MS = 60 * 1000;		// after an error
	const ULONGLONG MAX_AGE_MS = 3 * 60 * 1000;			// older data is no longer shown

	const size_t MAX_BODY_BYTES = 32 * 1024 * 1024;		// protection against endless responses

	// Timeouts in milliseconds: resolve, connect, send, receive
	const int TIMEOUT_RESOLVE_MS = 5000;
	const int TIMEOUT_CONNECT_MS = 5000;
	const int TIMEOUT_SEND_MS = 5000;
	const int TIMEOUT_RECEIVE_MS = 10000;
}

CAtisFetcher::CAtisFetcher()
	: m_hStopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr))
	, m_hRequest(nullptr)
	, m_bActive(false)
	, m_LastSuccess(0)
{
}

CAtisFetcher::~CAtisFetcher()
{
	Stop();
	if (m_hStopEvent != nullptr)
		CloseHandle((HANDLE)m_hStopEvent);
}

void CAtisFetcher::Start()
{
	if (m_Thread.joinable() || m_hStopEvent == nullptr)
		return;

	ResetEvent((HANDLE)m_hStopEvent);
	m_Thread = std::thread(&CAtisFetcher::ThreadMain, this);
}

void CAtisFetcher::Stop()
{
	if (!m_Thread.joinable())
		return;

	SetEvent((HANDLE)m_hStopEvent);

	// Abort a running download: closing the request makes the waiting WinHTTP call fail at once.
	// Whoever takes the handle out of m_hRequest (this function or the thread) closes it - never both.
	void* hRequest = m_hRequest.exchange(nullptr);
	if (hRequest != nullptr)
		WinHttpCloseHandle((HINTERNET)hRequest);

	m_Thread.join();
}

void CAtisFetcher::SetActive(bool active)
{
	m_bActive = active;
}

bool CAtisFetcher::GetLabel(const std::string& icao, std::string& label) const
{
	std::lock_guard<std::mutex> guard(m_Lock);

	if (m_LastSuccess == 0 || GetTickCount64() - m_LastSuccess > MAX_AGE_MS)
		return false;

	std::map<std::string, std::string>::const_iterator it = m_Labels.find(icao);
	if (it == m_Labels.end())
		return false;

	label = it->second;
	return true;
}

void CAtisFetcher::ThreadMain()
{
	ULONGLONG nextPoll = 0;

	// Once per second: look whether a download is due. The wait is also the end signal.
	while (WaitForSingleObject((HANDLE)m_hStopEvent, 1000) == WAIT_TIMEOUT)
	{
		if (!m_bActive)
		{
			// Not connected to VATSIM: forget everything, the next login downloads immediately
			nextPoll = 0;
			std::lock_guard<std::mutex> guard(m_Lock);
			m_Labels.clear();
			m_LastSuccess = 0;
			continue;
		}

		if (GetTickCount64() < nextPoll)
			continue;

		std::string body;
		std::map<std::string, std::string> labels;
		bool ok = DownloadFeed(body) && ParseAtisFeed(body, labels);

		// The connection may have been left while the download was running
		if (ok && m_bActive)
		{
			std::lock_guard<std::mutex> guard(m_Lock);
			m_Labels.swap(labels);
			m_LastSuccess = GetTickCount64();
		}

		nextPoll = GetTickCount64() + (ok ? POLL_INTERVAL_MS : RETRY_INTERVAL_MS);
	}
}

bool CAtisFetcher::DownloadFeed(std::string& body)
{
	body.clear();
	bool ok = false;

	HINTERNET hSession = WinHttpOpen(USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (hSession == nullptr)
		return false;

	WinHttpSetTimeouts(hSession, TIMEOUT_RESOLVE_MS, TIMEOUT_CONNECT_MS, TIMEOUT_SEND_MS, TIMEOUT_RECEIVE_MS);

#ifdef WINHTTP_OPTION_DECOMPRESSION
	// Let WinHTTP ask for a compressed response and unpack it (the feed is large, but compresses well).
	// Older systems without this option simply get the uncompressed feed.
	DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
	WinHttpSetOption(hSession, WINHTTP_OPTION_DECOMPRESSION, &decompression, sizeof(decompression));
#endif

	HINTERNET hConnect = WinHttpConnect(hSession, FEED_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (hConnect != nullptr)
	{
		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", FEED_PATH, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (hRequest != nullptr)
		{
			m_hRequest = hRequest;		// from now on Stop() can abort the request

			// (If Stop() was called just before, nothing is sent any more.)
			if (WaitForSingleObject((HANDLE)m_hStopEvent, 0) == WAIT_TIMEOUT &&
				WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
				WinHttpReceiveResponse(hRequest, nullptr))
			{
				DWORD status = 0;
				DWORD statusSize = sizeof(status);
				if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) && status == 200)
				{
					ok = true;
					std::vector<char> buffer(64 * 1024);
					for (;;)
					{
						DWORD read = 0;
						if (!WinHttpReadData(hRequest, &buffer[0], (DWORD)buffer.size(), &read))
						{
							ok = false;
							break;
						}
						if (read == 0)
							break;		// end of response

						body.append(&buffer[0], read);
						if (body.size() > MAX_BODY_BYTES)
						{
							ok = false;
							break;
						}
					}
				}
			}

			// Close the request unless Stop() has already done so
			void* pending = m_hRequest.exchange(nullptr);
			if (pending != nullptr)
				WinHttpCloseHandle((HINTERNET)pending);
		}
		WinHttpCloseHandle(hConnect);
	}
	WinHttpCloseHandle(hSession);

	return ok;
}
