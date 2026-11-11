// AtisFetcher.h: Fetches the current ATIS letters from the VATSIM data feed in the background
//
// EuroScope itself does not offer the ATIS letter of an airport to plugins. The letters are therefore read
// from the public VATSIM data feed (https://data.vatsim.net/v3/vatsim-data.json, no login needed).
// A background thread downloads the feed about every 30 seconds, but only while EuroScope is connected to
// the real VATSIM network (see SetActive). The thread never touches EuroScope; the plugin only asks for the
// finished result with GetLabel().

#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class CAtisFetcher
{
public:
	CAtisFetcher();
	~CAtisFetcher();		// stops the thread

	// Starts and stops the background thread (Stop() waits until it has ended; safe to call twice).
	void Start();
	void Stop();

	// Allows or forbids downloads. While not active nothing is downloaded and all results are discarded.
	void SetActive(bool active);

	// Returns the text to show for the airport (e.g. "K" or "D:K A:L"). false if there is no current
	// information for this airport (no ATIS online, feed not yet loaded, not active, data too old).
	bool GetLabel(const std::string& icao, std::string& label) const;

private:
	void ThreadMain();
	bool DownloadFeed(std::string& body);	// returns false on any error

	std::thread m_Thread;
	void*       m_hStopEvent;		// HANDLE: signalled when the thread has to end
	std::atomic<void*> m_hRequest;	// HINTERNET of the running request, so that Stop() can abort it
	std::atomic<bool>  m_bActive;

	mutable std::mutex m_Lock;
	std::map<std::string, std::string> m_Labels;	// ICAO -> text (guarded by m_Lock)
	unsigned long long m_LastSuccess;				// GetTickCount64() of the last successful download, 0 = none
};
