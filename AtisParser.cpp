// AtisParser.cpp: Reads the ATIS letters of the VATSIM data feed (see AtisParser.h)

#include "pch.h"
#include "AtisParser.h"

namespace
{
	// ---- Minimal JSON reader ------------------------------------------------------------
	// Only what is needed for the data feed: the top-level object is walked key by key, every value
	// except "atis" is skipped without being evaluated (the feed is large, mostly pilots).

	void SkipWhitespace(const std::string& s, size_t& p)
	{
		while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\r' || s[p] == '\n'))
			p++;
	}

	// Reads the JSON string at position p (which must be a '"'). Escape sequences are resolved; for
	// \uXXXX a '?' is stored (the fields we need only contain plain ASCII). p ends up behind the closing quote.
	bool ReadString(const std::string& s, size_t& p, std::string& out)
	{
		out.clear();
		if (p >= s.size() || s[p] != '"')
			return false;
		p++;

		while (p < s.size())
		{
			char c = s[p++];
			if (c == '"')
				return true;
			if (c != '\\')
			{
				out += c;
				continue;
			}

			if (p >= s.size())
				return false;
			char e = s[p++];
			switch (e)
			{
			case 'n': out += '\n'; break;
			case 't': out += '\t'; break;
			case 'r': out += '\r'; break;
			case 'b': out += '\b'; break;
			case 'f': out += '\f'; break;
			case 'u':
				if (p + 4 > s.size())
					return false;
				p += 4;
				out += '?';
				break;
			default: out += e; break;	// \" \\ \/
			}
		}
		return false;	// string not terminated
	}

	// Skips one JSON value of any kind (string, number, true/false/null, object, array).
	bool SkipValue(const std::string& s, size_t& p)
	{
		SkipWhitespace(s, p);
		if (p >= s.size())
			return false;

		if (s[p] == '"')
		{
			std::string dummy;
			return ReadString(s, p, dummy);
		}

		if (s[p] == '{' || s[p] == '[')
		{
			int depth = 0;
			while (p < s.size())
			{
				char c = s[p];
				if (c == '"')
				{
					std::string dummy;
					if (!ReadString(s, p, dummy))
						return false;
					continue;
				}
				p++;
				if (c == '{' || c == '[')
					depth++;
				else if (c == '}' || c == ']')
				{
					depth--;
					if (depth == 0)
						return true;
				}
			}
			return false;	// not closed
		}

		// Number, true, false, null: up to the next separator
		size_t start = p;
		while (p < s.size() && s[p] != ',' && s[p] != '}' && s[p] != ']' &&
			s[p] != ' ' && s[p] != '\t' && s[p] != '\r' && s[p] != '\n')
			p++;
		return p > start;
	}

	struct AtisEntry
	{
		std::string callsign;
		std::string code;
	};

	// Reads one entry of the "atis" array (an object at position p).
	bool ReadAtisEntry(const std::string& s, size_t& p, AtisEntry& entry)
	{
		entry = AtisEntry();
		if (p >= s.size() || s[p] != '{')
			return false;
		p++;

		for (;;)
		{
			SkipWhitespace(s, p);
			if (p >= s.size())
				return false;
			if (s[p] == '}')
			{
				p++;
				return true;
			}
			if (s[p] == ',')
			{
				p++;
				continue;
			}

			std::string key;
			if (!ReadString(s, p, key))
				return false;
			SkipWhitespace(s, p);
			if (p >= s.size() || s[p] != ':')
				return false;
			p++;
			SkipWhitespace(s, p);

			if ((key == "callsign" || key == "atis_code") && p < s.size() && s[p] == '"')
			{
				std::string value;
				if (!ReadString(s, p, value))
					return false;
				if (key == "callsign")
					entry.callsign = value;
				else
					entry.code = value;
			}
			else if (!SkipValue(s, p))		// other fields and null values
			{
				return false;
			}
		}
	}

	std::string ToUpper(std::string text)
	{
		for (size_t i = 0; i < text.size(); i++)
			if (text[i] >= 'a' && text[i] <= 'z')
				text[i] = (char)(text[i] - 'a' + 'A');
		return text;
	}

	// Information letter of an entry: exactly one letter A-Z (upper case), otherwise empty.
	std::string NormaliseCode(const std::string& code)
	{
		std::string upper = ToUpper(code);
		if (upper.size() == 1 && upper[0] >= 'A' && upper[0] <= 'Z')
			return upper;
		return std::string();
	}

	// Letters of one airport
	struct AirportCodes
	{
		std::string combined;	// ICAO_ATIS
		std::string arrival;	// ICAO_A_ATIS
		std::string departure;	// ICAO_D_ATIS
	};

	// Text to display for an airport (see AtisParser.h); empty = nothing to show.
	std::string BuildLabel(const AirportCodes& codes)
	{
		if (!codes.arrival.empty() && !codes.departure.empty())
		{
			if (codes.arrival == codes.departure)
				return codes.arrival;
			return "D:" + codes.departure + " A:" + codes.arrival;
		}
		if (!codes.departure.empty())
			return "D:" + codes.departure;
		if (!codes.arrival.empty())
			return "A:" + codes.arrival;
		return codes.combined;
	}

	// Sorts an entry into the table of its airport. Callsigns: "EDDF_ATIS", "EDDF_A_ATIS", "EDDF_D_ATIS".
	void AddEntry(const AtisEntry& entry, std::map<std::string, AirportCodes>& airports)
	{
		std::string callsign = ToUpper(entry.callsign);
		std::string code = NormaliseCode(entry.code);
		if (code.empty() || callsign.size() < 4 + 5 || callsign.compare(callsign.size() - 5, 5, "_ATIS") != 0)
			return;

		std::string icao = callsign.substr(0, 4);
		std::string middle = callsign.substr(4, callsign.size() - 4 - 5);	// "", "_A" or "_D"

		if (middle.empty())
			airports[icao].combined = code;
		else if (middle == "_A")
			airports[icao].arrival = code;
		else if (middle == "_D")
			airports[icao].departure = code;
		// other callsigns (e.g. "EDDF_1_ATIS") are ignored
	}
}

bool ParseAtisFeed(const std::string& json, std::map<std::string, std::string>& result)
{
	result.clear();

	size_t p = 0;
	SkipWhitespace(json, p);
	if (p >= json.size() || json[p] != '{')
		return false;
	p++;

	std::map<std::string, AirportCodes> airports;
	bool foundAtis = false;

	for (;;)
	{
		SkipWhitespace(json, p);
		if (p >= json.size())
			break;
		if (json[p] == '}')
			break;
		if (json[p] == ',')
		{
			p++;
			continue;
		}

		std::string key;
		if (!ReadString(json, p, key))
			break;
		SkipWhitespace(json, p);
		if (p >= json.size() || json[p] != ':')
			break;
		p++;
		SkipWhitespace(json, p);

		if (key != "atis")
		{
			if (!SkipValue(json, p))
				break;
			continue;
		}

		// The "atis" array
		if (p >= json.size() || json[p] != '[')
			break;			// "atis": null or something unexpected
		p++;
		foundAtis = true;

		for (;;)
		{
			SkipWhitespace(json, p);
			if (p >= json.size() || json[p] == ']')
			{
				if (p < json.size())
					p++;
				break;
			}
			if (json[p] == ',')
			{
				p++;
				continue;
			}

			if (json[p] == '{')
			{
				AtisEntry entry;
				if (!ReadAtisEntry(json, p, entry))
					break;
				AddEntry(entry, airports);
			}
			else if (!SkipValue(json, p))
			{
				break;
			}
		}

		break;		// everything needed has been read
	}

	if (!foundAtis)
		return false;

	for (std::map<std::string, AirportCodes>::const_iterator it = airports.begin(); it != airports.end(); ++it)
	{
		std::string label = BuildLabel(it->second);
		if (!label.empty())
			result[it->first] = label;
	}
	return true;
}
