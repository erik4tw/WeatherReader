// AtisParser.h: Reads the ATIS letters of the VATSIM data feed
//
// The VATSIM data feed (https://data.vatsim.net/v3/vatsim-data.json) contains the array "atis" with one
// entry per online ATIS station. Each entry has (among others) the fields "callsign" (e.g.
// "EDDF_ATIS", "EDDF_A_ATIS" or "EDDF_D_ATIS") and "atis_code" (the current information letter, may be
// empty or null). This file only evaluates the JSON text; fetching it from the internet is done by
// AtisFetcher.cpp. It contains no Windows-specific code so that it can be tested easily.

#pragma once

#include <map>
#include <string>

// Reads the data feed (JSON text) and returns for each airport (ICAO code, upper case) the text to show,
// e.g. "K" for a combined ATIS (or equal departure and arrival letters), "D:K A:L" if the departure and
// arrival ATIS have different letters and "A:L" / "D:K" if only one of them is online. Stations without
// an information letter are left out.
// Returns false if the text is not a usable data feed (no "atis" array found); "result" is empty then.
bool ParseAtisFeed(const std::string& json, std::map<std::string, std::string>& result);
