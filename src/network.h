#pragma once

// WiFi connection management, Improv Wi-Fi provisioning, and every outbound
// HTTP fetch (weather, PSI, sunrise/sunset, LTA bus arrivals). Fetching
// itself runs on a dedicated FreeRTOS task (startNetTask(), pinned to
// core 0) per docs/MAKERPULSE_V2_SPEC.md's "Shared behaviour and
// architecture" -- the UI task never blocks on a slow HTTPS call. KP-index
// (aurora) fetching was dropped in the V2.1.0 spec migration: no page in
// the spec shows it, so there's nothing left to keep it fresh for.

#include "state.h"

void beginWiFiConnect();
void connectWiFi(bool waitForConnection = true);
void updateWiFiConnectionState();

void startNetTask();

bool fetchSunriseSunset();
void ensureSunTimesForToday();
bool fetchWeather();
void ensureWeather();
bool fetchPsi();
void ensurePsi();
bool fetchBusArrival(int idx);
bool fetchBusStopName(int idx);
void ensureBusArrival();
