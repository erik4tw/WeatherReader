@echo off
rem Builds and runs the tests for the WeatherReader plugin.
rem Run this in a "Developer Command Prompt for VS" (the compiler "cl" is available there).
rem The program needs neither EuroScope nor MFC.

cd /d "%~dp0"
if not exist obj mkdir obj
cl /nologo /EHsc /std:c++17 /W3 /Fe:WeatherReaderTests.exe /Fo:obj\ WeatherReaderTests.cpp
if errorlevel 1 exit /b 1

WeatherReaderTests.exe
exit /b %errorlevel%
