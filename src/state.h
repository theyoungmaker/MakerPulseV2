#pragma once

// Device-wide mutable state: hardware object instances, persisted settings,
// and the small pure helpers (lookups, sanitizers, cache resets) that act
// directly on that state. Every other module reaches shared state through
// the externs declared here.

#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <ImprovWiFiLibrary.h>

extern String storedWifiSsid;
extern String storedWifiPass;
extern bool deviceProvisioned;
extern ImprovWiFi improvSerial;

extern TFT_eSPI tft;
extern SPIClass touchSPI;
extern XPT2046_Touchscreen ts;

extern WebServer server;
extern Preferences prefs;

extern TFT_eSprite sprClock;
extern TFT_eSprite sprSmall;

// Guards every shared-feed variable below (weather/PSI/bus/sun-time state)
// that the net task (network.cpp, core 0) writes and the UI (main loop,
// core 1) reads. Fetch functions hold it only around the brief
// assignment-of-parsed-results step, never across the HTTPS call itself --
// that's the whole point of the dedicated task. Initialized once in
// setup() before startNetTask() is called.
extern SemaphoreHandle_t dataMutex;
void dataLock();
void dataUnlock();

extern float LAT;
extern float LNG;
extern String locationName;

extern uint16_t COL_BG, COL_PANEL, COL_PANEL_ALT, COL_STROKE, COL_TEXT, COL_DIM, COL_ACCENT;
extern const uint16_t COL_GREEN, COL_YELLOW, COL_RED, COL_BLUE;

// One of the 8 fixed shell colours -- see CONTEXT.md's "Shell colour"
// entry. Replaces MakerPulse's freeform Accent/Text/Theme system; there is
// no custom picker in V2.
extern String shellColourKey;
extern String unitKey;
extern String regionFormatKey;
extern String timezoneKey;

// Screen orientation: "top" (default) or "bottom" -- see CONTEXT.md's
// "Screen orientation" entry. rotationValue/touchFlipX/touchFlipY are
// derived from orientationKey by applyOrientationByKey(); loadStoredSettings()
// only sets orientationKey itself (tft/ts aren't initialized yet at that
// point in setup()) -- main.cpp calls applyOrientationByKey() once the
// display and touch hardware are ready.
extern String orientationKey;
extern int rotationValue;
extern bool touchFlipX, touchFlipY;

extern String deviceNickname;

// Spec "Status" > "Layout": "Owner | Made by Aisha (OWNER_NAME, the name
// on the student's Name Cap)". Distinct from deviceNickname (which names
// the device itself, shown as the Status "Product name" via homeTitleText()
// -- actually just "MakerPulse" per the spec table, deviceNickname is a
// pre-V2.1.0 field kept for the settings page but no longer drawn on
// Status as the headline). Empty hides the "Made by" row entirely.
extern String ownerName;

// Spec "Status" > "Interactions": "Brightness option: Set backlight: Low
// 40, Mid 140, High 255 (8-bit PWM), save." 0=Low, 1=Mid, 2=High.
extern int brightnessLevel;
int brightnessPwm();
void setBrightnessLevel(int level);

// Spec "Status" > "Night mode Auto": "between today's sunset and the next
// sunrise, render with the Charcoal palette and Low brightness... The
// chosen colourway returns at sunrise. If sun times are unknown, use
// 19:00-07:00." nightModeKey is "off" or "auto"; applyNightOverride()
// checks the clock each second (main.cpp) and swaps the live palette in
// and out without touching the user's actual shellColourKey choice.
extern String nightModeKey;
void applyNightOverride();

// Spec "Status" > "Interactions": "Synced row: Force-refresh all feeds
// now; row reads 'Syncing...' until done." 0 = not syncing.
extern unsigned long forcedSyncStartedMs;
void forceSyncAllFeeds();

// Spec "Home" > "Interactions": favourite bus service, set by long-pressing
// its row on the Bus page (Phase 6); the Home Bus tile reads the same
// value. Empty until the user has favourited one.
extern String favBusService;

// Spec "Home" > "Interactions": "Clock: long-press toggles 12h/24h
// (saved)". Independent of regionFormatKey (date format), which is a
// separate pre-existing setting.
extern bool clockIs24h;
void saveClockIs24h(bool is24h);

extern Page currentPage;
extern Page lastDrawnPage;
extern unsigned long lastClockTick;
extern unsigned long lastDataTick;
extern bool pageDirty;
extern bool dataDirty;

extern String cacheClock, cacheTemp, cacheRain, cacheWeek, cacheHomeEmpty1, cacheHomeEmpty2;
extern String cacheFocusTimer;
extern String cacheHomeTiles[4];
extern String cacheTimerDone;
extern String cacheTimerPicker;
extern String cacheCustomStepper;

extern String lastUvText, lastPsiText;
extern String lastNowText, lastHourlyText, lastBusPageText;
extern String lastWeatherHeaderText;
extern String lastBusHeaderText;
extern String lastStatusHeaderText, lastStatusColourwayText, lastStatusSegmentsText, lastStatusInfoText;

const char* timezonePosixByKey(const String& key);
const char* timezoneLabelByKey(const String& key);
String sanitizeTimezoneKey(const String& key);
void applyDeviceTimezoneByKey(const String& key);
void getHomeTileRect(int tileIdx, int& x, int& y, int& w, int& h);
void clearHomeTileCaches();

// Spec "Timer states": idle / running / paused / done. focusDurationMin is
// the chosen duration in minutes -- persisted in NVS as a raw value (not
// an index, since Custom entries don't fit a fixed-list index model),
// updated to whatever was last actually run, preset or custom alike.
// focusRemainingSec is the live countdown while running or paused;
// focusEndMs is only meaningful while running (millis() target for when
// it hits zero).
extern FocusTimerState focusTimerState;
extern int focusDurationMin;
extern unsigned long focusEndMs, focusRemainingSec;
extern unsigned long focusDoneStartedMs;

// The 6 preset durations shown in the timer picker -- editable via the
// settings page (CONTEXT.md's "timer picker" entry), defaulting to
// FOCUS_PRESET_DEFAULT_MIN. Index order is display order, not magnitude
// order, though in practice the defaults are already ascending.
extern int timerPresetMin[FOCUS_PRESET_COUNT];
int sanitizeTimerMinutes(int minutes);
void saveTimerPreset(int idx, int minutes);

// Timer picker / Custom-entry overlay UI state -- not persisted, reset to
// closed on every boot. See CONTEXT.md's "timer picker" entry.
extern bool timerPickerOpen;
extern bool timerCustomEntryOpen;
extern int customTimerMinutes; // working value while the Custom stepper is open
extern bool wifiEnabled, wifiConnectInProgress;
extern unsigned long wifiConnectStartedMs;

extern float tempC, tempMinC, tempMaxC, precipMm, windSpeedMs, windDirectionDeg, uvIndex, uvIndexMax, apparentTempC, humidity;
extern int weatherCode;
extern bool weatherIsDay;
extern time_t lastWeatherFetch;
extern FetchBackoff weatherBackoff;
extern float hourlyTempC[HOURLY_COUNT];
extern int hourlyCode[HOURLY_COUNT];
extern bool hourlyIsDay[HOURLY_COUNT];
extern int hourlyHour[HOURLY_COUNT];
extern int hourlyPrecipProb[HOURLY_COUNT];
extern int hourlyFilled;

// Spec "Weather" > "Tap the strip: 5-day view" -- index 0 is today (shares
// temperature_2m_max/min with the tempMaxC/tempMinC fields above), indices
// 1..5 are the five days the toggle view shows.
extern int dailyWeatherCode[DAILY_COUNT];
extern float dailyTempMaxC[DAILY_COUNT];
extern int dailyPrecipProbMax[DAILY_COUNT];
extern int dailyYmd[DAILY_COUNT];
extern int dailyFilled;

// Page-local UI toggle, not persisted -- "resets to hourly when you leave
// the page" (main.cpp's handleNavTouch() clears it on any page change).
extern bool weatherShowDaily;

// National 24-hour PSI via data.gov.sg v2 (spec "Data sources" table) --
// no region selection anymore, the v2 real-time API returns one national
// reading. datagovApiKey is the required x-api-key header value.
extern String datagovApiKey;
extern float psiValue;
extern time_t lastPsiFetch;
extern FetchBackoff psiBackoff;

extern String ltaApiKey;
String effectiveLtaApiKey();

extern BusStopConfig busStops[BUS_MAX_STOPS];
extern int busActiveStopIdx;
extern BusStopNameInfo busStopNames[BUS_MAX_STOPS];
extern BusServiceInfo busServicesByStop[BUS_MAX_STOPS][BUS_MAX_SERVICES];
extern int busServiceCountByStop[BUS_MAX_STOPS];
extern bool busDataValidByStop[BUS_MAX_STOPS];
extern time_t lastBusFetchByStop[BUS_MAX_STOPS];
extern FetchBackoff busBackoff[BUS_MAX_STOPS];
extern int busServicePage[BUS_MAX_STOPS];
String busStopDisplayName(int idx);

extern int sunriseMin, sunsetMin, lastSunYmd;
extern time_t lastSyncTime;
extern FetchBackoff sunBackoff;

extern bool sleepDimmed, sleepOff, manualDimMode;
extern unsigned long lastInteractionMs;
extern int sleepIntervalMin, sleepOffDelaySec;

// Runtime touch calibration (spec: "long-press the STATUS header for 5s to
// run touch calibration; values are stored in NVS" -- Phase 7 builds the
// actual calibration routine; this storage exists from Phase 2 onward so
// readTouchXY() always reads through it instead of config.h's fixed
// defaults). Defaults to config.h's TOUCH_X_MIN/MAX/Y_MIN/MAX until a
// calibration has actually been run and saved.
extern int touchCalXMin, touchCalXMax, touchCalYMin, touchCalYMax;
void saveTouchCalibration(int xMin, int xMax, int yMin, int yMax);

// Spec "Hardening" > "Edge cases": "NTP not yet synced (show '--:--')" --
// any epoch before this is clearly pre-sync, not a real clock reading.
// Same threshold main.cpp's waitForNtpTime() uses at boot.
bool timeIsSynced();

int ymdFromLocal(time_t t);
int minutesFromLocalEpoch(time_t t);
int minutesNowLocal();

// Spec "Home" > "Interactions"/"Timer states": tap handling is state-aware
// (handleTimerTileTap()) rather than one toggle function, now that idle
// and paused taps both open the picker instead of starting/resuming
// directly.
void handleTimerTileTap();      // tap: idle/paused->opens picker, running->pause
void openTimerPicker();
void closeTimerPicker();        // closes picker and/or Custom stepper, no state change
void resumeFocusTimer();        // picker's "Resume" row, only valid while paused
void stopFocusTimerToIdle();    // picker's "Stop" row, only valid while paused
void startFocusTimerWithMinutes(int minutes); // picker preset row or Custom's "Start"
void openCustomTimerStepper();  // picker's "Custom" row
void adjustCustomTimerMinutes(int delta);
void cancelCustomTimerStepper(); // Custom's "Cancel" button -- back to the picker
void dismissTimerDone();         // tap anywhere while done, or 60s auto-dismiss
void updateFocusTimerState();    // ticks the running countdown; running->done at zero
void updateTimerDoneAutoDismiss();

void applyShellColourByKey(const String& key);
const char* shellColourLabel(const String& key);
bool isValidShellColourKey(const String& key);
int shellColourCount();
void shellColourAt(int idx, const char** key, const char** label, uint16_t* swatch);
void colourwayRolesAt(int idx, uint16_t* bg, uint16_t* ink, uint16_t* muted, uint16_t* rule, uint16_t* accent);
void applyOrientationByKey(const String& key);

void loadStoredSettings();
void resetDataCaches();
