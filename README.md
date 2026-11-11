# WeatherReader

A small [EuroScope](https://www.euroscope.hu/) plugin for VATSIM controllers. It shows the weather
state of the **active airport** (as selected in the *Active airport/runway selector*) in a small,
draggable box on the radar screen, so you do not have to read and interpret the METAR yourself.

> **For simulation use on the VATSIM network only. Never use it for real-world aviation.**
> The plugin is a convenience aid, not an authoritative source. Always check the METAR and
> the applicable procedures yourself.

## What it shows

```
K WX EDDF · 1420z      <- ATIS letter (bold, blue), station (coloured like the state) and METAR time (UTC); the arrow collapses the box
25008KT          Q1019 <- wind group and QNH as in the METAR, bold
   R25L/P2000N         <- RVR of the landing runways, as in the METAR (only if the METAR has an RVR, expanded only)
        IMC            <- state, coloured
   RRS POSSIBLE        <- note on Reduced Runway Separation (RRS)
RWY 25 HW: 5 kts · CW: 3 kts
RWY 18 TW: 13 (23) · CW: 5 (8) kts   <- per active runway; gusts in brackets
  CEIL 1200 · VIS 9999 <- the values the state is based on (or NO CEIL)
```

| State                  | Colour        | Meaning                                                                  |
|------------------------|---------------|--------------------------------------------------------------------------|
| **VMC**                | green         | Ceiling >= 1500 ft (or none) and visibility >= 5000 m                    |
| **IMC**                | yellow        | IMC, weather allows RRS (visibility >= 5000 m, ceiling >= 1000 ft, no SN/SG/PL/GR/GS) |
| **IMC**                | orange        | IMC, RRS not possible                                                    |
| **LVO**                | red           | Ceiling < 200 ft (incl. VV///) or RVR <= 600 m; takes precedence         |
| **-**                  | grey          | No airport active, no METAR yet, or METAR cannot be evaluated            |

* **RRS and tailwind:** RRS additionally requires a mean tailwind of at most 5 kt on the runway. Yellow
  IMC turns orange if RRS is possible on none of the active runways; with VMC (or yellow IMC) and
  only some runways restricted the note reads e.g. `RRS ONLY RWY 07, 18`, and the wind line of a
  restricted runway turns orange. Wintry weather (SN, SG, PL, GR, GS) rules out RRS even in VMC.
* **Change of state:** if the state changes at the same airport, the box flashes for 10 seconds and a short
  "meep" is played. (Not on the first display, on an airport change or for "unknown".)
* **Wind and QNH:** the main wind group (without the varying-direction group) and the QNH are shown in bold above the
  state, so the box can be used to read out the wind. If the QNH changes at the same airport, the QNH flashes inverted for
  10 seconds and the same "meep" is played. The row stays visible when the box is collapsed (then the box consists of the header and this row). "WX <ICAO>" in the header is coloured like the state, so the weather state is also visible when collapsed.
* **ATIS letter:** EuroScope does not offer the ATIS letter to plugins, so the plugin reads it from the public
  [VATSIM data feed](https://data.vatsim.net/v3/vatsim-data.json) and shows it at the left of the header, also when the
  box is collapsed. It looks for the stations `<ICAO>_ATIS`, `<ICAO>_A_ATIS` and `<ICAO>_D_ATIS`: a combined ATIS (or equal
  arrival and departure letters) shows e.g. `K`, different letters show `D:K A:L`, a single arrival or departure ATIS
  shows `A:L` or `D:K`. Nothing is shown if the airport has no ATIS online.
  **Network access:** a background thread downloads the feed (HTTPS GET, no login, about every 30 seconds, via the
  Windows proxy settings) and only while EuroScope is connected to the live VATSIM network. On a Sweatbox, as a simulator
  or in playback nothing is downloaded and no letter is shown. No data is sent anywhere.
* **RVR:** if the METAR contains RVR groups, they are shown as written in the METAR (e.g. `R25L/P2000N`) in bold below the
  wind/QNH row, as the controller has to read them out to every landing aircraft. Only the groups of the runway ends that
  are ticked as **arrival** runways in the airport/runway selector are shown, at most two per row. If no arrival runway is
  selected, or none of them has an RVR group, all RVR groups of the METAR are shown. The rows are only visible when the
  box is expanded. For the LVO decision the lowest RVR of *all* groups still counts (<= 600 m).
* **Runways** are taken from the airport/runway selector (departure or arrival, all parallel runways
  of a number are merged to one line). The runway heading is derived from the runway number (x 10).
* Only the current weather before `BECMG`, `TEMPO`, `NOSIG` and `RMK` is evaluated.
  `CB`/`TCU` clouds without a height are assumed to be at least 1500 ft. Wind must be given in knots (`KT`).

The exact rules are documented in [`MetarParser.h`](MetarParser.h) and [`DisplayLogic.h`](DisplayLogic.h).

## Installation

1. Download `WeatherReader.dll` (Release build, 32-bit) from the
   [Releases](../../releases) page.
2. In EuroScope open **Other SET > Plug-ins**, click **Load** and select the DLL.
3. The box appears in the standard radar screen and in the ground radar. Drag it to where you want it; the
   position and the collapsed state are saved per screen type in your EuroScope settings.

> **If EuroScope cannot load the DLL:** Windows may have marked the downloaded file as blocked. Right-click
> `WeatherReader.dll` in the Explorer, choose **Properties**, tick **Unblock** at the bottom of the *General* tab
> (if it is shown), click **OK** and load the plugin again.

## Commands

| Command                    | Effect                                                             |
|----------------------------|--------------------------------------------------------------------|
| `.wbox`                    | Show/hide the box                                                  |
| `.wbox metar <METAR>`      | Use the given METAR instead of the live one (an `*` follows the station); handy for testing, also on a Sweatbox |
| `.wbox metar off`          | Use the live METAR again (also: `.wbox metar` without text)        |

## Building

Requirements: Visual Studio with the *Desktop development with C++* workload including *MFC* (platform toolset
v145 as configured; adjust if needed). EuroScope is a 32-bit application, so build the **Win32 / x86** configuration.

1. Open `WeatherReader.slnx` (or `WeatherReader.vcxproj`).
2. Select **Release | x86** and build. The Release configuration links MFC statically, so no Visual C++
   redistributable is needed on the target machine. The result is `Release\WeatherReader.dll`.

The EuroScope plugin SDK (`es/EuroScopePlugIn.h`, `es/EuroScopePlugInDll.lib`) is included in the repository and was
created by Gergely Csernak, the author of EuroScope.

## Tests

The METAR evaluation, wind components, RRS logic, texts and the data feed parser are covered by a small test program that needs neither
EuroScope nor MFC. Open a *Developer Command Prompt for VS* and run:

```
tests\run_tests.bat
```

## Project layout

| File                          | Content                                                        |
|-------------------------------|----------------------------------------------------------------|
| `MetarParser.*`               | METAR evaluation (flight rules, visibility, ceiling, RVR, wind) |
| `DisplayLogic.*`              | Detail line, wind lines, RRS decision and note (pure logic)    |
| `AtisParser.*`                | Reads the ATIS letters from the VATSIM data feed (JSON text)   |
| `AtisFetcher.*`               | Background download of the data feed (WinHTTP)                 |
| `MeepSound.*`                 | The change-of-state sound, generated in memory                 |
| `WeatherBox.*`                | Drawing and layout of the box (GDI)                            |
| `WeatherReader.*`             | EuroScope plugin class and radar screen                        |
| `tests/`                      | Console test program                                           |

## License

See the `LICENSE` file in the repository.
