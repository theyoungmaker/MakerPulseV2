#include "state.h"
#include "display.h"

// MakerPulse V.8
// Nav: Home / Weather / Notes / Status
// Full version
// - KP dots replaced with Low / Medium / High / Extreme text
// - KP level text uses same small font as wind direction and stays inside the box
// - Wind + direction added to Weather page
// - Wind direction uses Accent color
// - Weather sun event field automatically shows Sunrise or Sunset, whichever is next
// - Uptime added to Status page



// The Young Maker brand mark: a maker figure under three Wi-Fi arcs, reused
// as-is in the settings page header/footer. Colors sampled from their real
// logo asset and live site (theyoungmaker.com) rather than invented here.


// =========================================================
// WIFI
// =========================================================
// WiFi credentials are no longer compiled in. They are provisioned at
// runtime via the Improv Wi-Fi serial protocol (see setup()/loop() and
// onImprovWiFiConnectedCb below) and persisted through Preferences.
String storedWifiSsid = "";

String storedWifiPass = "";

bool deviceProvisioned = false;
 // true once a WiFi SSID has ever been saved

ImprovWiFi improvSerial(&Serial);


// =========================================================
// DISPLAY / TOUCH
// =========================================================
TFT_eSPI tft;


SPIClass touchSPI(VSPI);

XPT2046_Touchscreen ts(TOUCH_CS);


// =========================================================
// WEB / STORAGE
// =========================================================
WebServer server(80);

Preferences prefs;

SemaphoreHandle_t dataMutex = nullptr;

void dataLock() {
  if (dataMutex) xSemaphoreTake(dataMutex, portMAX_DELAY);
}

void dataUnlock() {
  if (dataMutex) xSemaphoreGive(dataMutex);
}


// =========================================================
// SPRITES
// =========================================================
TFT_eSprite sprClock = TFT_eSprite(&tft);

TFT_eSprite sprSmall = TFT_eSprite(&tft);


// =========================================================
// LOCATION
// =========================================================
float LAT = 1.3521f;

float LNG = 103.8198f;

String locationName = "Singapore";


// =========================================================
// THEME
// =========================================================
uint16_t COL_BG        = 0x08A3;

uint16_t COL_PANEL     = 0x1106;

uint16_t COL_PANEL_ALT = 0x18C7;

uint16_t COL_STROKE    = 0x31EC;

uint16_t COL_TEXT      = 0xEF7D;

uint16_t COL_DIM       = 0x94B2;

uint16_t COL_ACCENT    = 0x5EFA;


String shellColourKey = "muted-white";

// Screen orientation. "bottom" uses rotationValue 0, the exact 180-degree
// opposite of "top"'s rotationValue 2 on this ST7789 panel. Both TFT_eSPI
// and the vendored XPT2046_Touchscreen library define their rotation 0/2
// as precise mirror images of each other, so touchFlipX/touchFlipY stay
// false for both orientations -- ts.setRotation() alone keeps touch in
// sync with the display. (A prior version of this code additionally set
// touchFlipX/touchFlipY = true for "bottom", stacking a second, redundant
// 180-degree flip on top of what ts.setRotation(0) already does; that
// double-flip cancelled back toward "top"-like touch behavior while the
// display stayed correctly rotated -- symptom: tapping a control did
// nothing, but tapping the control's pre-rotation position worked. Fixed
// and verified on physical "USB on bottom" hardware 2026-11-18 (v1.2.1).)
String orientationKey = "top";

int rotationValue = 2;

bool touchFlipX = false;

bool touchFlipY = false;

String unitKey = "metric";
 // metric = C/mm, imperial = F/in
String regionFormatKey = "europe";
 // europe = 24h + dd.mm.yyyy, us = 12h + mm/dd/yyyy
String timezoneKey = "singapore";


// =========================================================
// PROFILE
// =========================================================
String deviceNickname = "";

String favBusService = "";

bool clockIs24h = true;

void saveClockIs24h(bool is24h) {
  clockIs24h = is24h;
  prefs.putBool("clock24h", is24h);
}


String ownerName = "";

int brightnessLevel = 2; // High -- matches the pre-V2.1.0 always-255 behaviour

int brightnessPwm() {
  switch (brightnessLevel) {
    case 0: return BRIGHTNESS_LOW;
    case 1: return BRIGHTNESS_MID;
    default: return BRIGHTNESS_HIGH;
  }
}

void setBrightnessLevel(int level) {
  brightnessLevel = constrain(level, 0, 2);
  prefs.putInt("brightness", brightnessLevel);
  if (!sleepDimmed && !sleepOff) setBacklight(brightnessPwm());
}


String nightModeKey = "off";

unsigned long forcedSyncStartedMs = 0;

void forceSyncAllFeeds() {
  weatherBackoff.nextRetryAt = 0;
  psiBackoff.nextRetryAt = 0;
  sunBackoff.nextRetryAt = 0;
  for (int i = 0; i < BUS_MAX_STOPS; i++) busBackoff[i].nextRetryAt = 0;
  forcedSyncStartedMs = millis();
  pageDirty = true;
}


String cacheHomeTiles[4];


Page currentPage = PAGE_HOME;

Page lastDrawnPage = (Page)-1;


unsigned long lastClockTick = 0;

unsigned long lastDataTick  = 0;


bool pageDirty = true;

bool dataDirty = true;


// cache
String cacheClock = "";

String cacheTemp = "";

String cacheRain = "";

String cacheWeek = "";

String cacheHomeEmpty1 = "";

String cacheHomeEmpty2 = "";

String cacheFocusTimer = "";

String cacheTimerDone = "";

String cacheTimerPicker = "";

String cacheCustomStepper = "";


String lastUvText = "";

String lastPsiText = "";

String lastNowText = "";

String lastHourlyText = "";

String lastBusPageText = "";

String lastWeatherHeaderText = "";

String lastBusHeaderText = "";

String lastStatusHeaderText = "";
String lastStatusColourwayText = "";
String lastStatusSegmentsText = "";
String lastStatusInfoText = "";


const char* timezonePosixByKey(const String& key) {
  if (key == "utc") return "UTC0";
  if (key == "atlantic_azores") return "AZOT1AZOST,M3.5.0/0,M10.5.0/1";
  if (key == "europe_west") return "WET0WEST,M3.5.0/1,M10.5.0";
  if (key == "uk") return "GMT0BST,M3.5.0/1,M10.5.0";
  if (key == "europe_central") return "CET-1CEST,M3.5.0/2,M10.5.0/3";
  if (key == "europe_east") return "EET-2EEST,M3.5.0/3,M10.5.0/4";
  if (key == "africa_south") return "SAST-2";
  if (key == "middle_east_gulf") return "GST-4";
  if (key == "india") return "IST-5:30";
  if (key == "thailand") return "ICT-7";
  if (key == "singapore") return "SGT-8";
  if (key == "china") return "CST-8";
  if (key == "us_eastern") return "EST5EDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_central") return "CST6CDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_mountain") return "MST7MDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_arizona") return "MST7";
  if (key == "us_pacific") return "PST8PDT,M3.2.0/2,M11.1.0/2";
  if (key == "alaska") return "AKST9AKDT,M3.2.0/2,M11.1.0/2";
  if (key == "hawaii") return "HST10";
  if (key == "canada_atlantic") return "AST4ADT,M3.2.0/2,M11.1.0/2";
  if (key == "brazil_east") return "BRT3";
  if (key == "argentina") return "ART3";
  if (key == "asia_tokyo") return "JST-9";
  if (key == "korea") return "KST-9";
  if (key == "australia_perth") return "AWST-8";
  if (key == "australia_darwin") return "ACST-9:30";
  if (key == "australia_sydney") return "AEST-10AEDT,M10.1.0,M4.1.0/3";
  if (key == "new_zealand") return "NZST-12NZDT,M9.5.0/2,M4.1.0/3";
  return "SGT-8";
}


const char* timezoneLabelByKey(const String& key) {
  if (key == "utc") return "UTC";
  if (key == "atlantic_azores") return "Azores";
  if (key == "europe_west") return "Western Europe";
  if (key == "uk") return "United Kingdom";
  if (key == "europe_central") return "Central Europe";
  if (key == "europe_east") return "Eastern Europe";
  if (key == "africa_south") return "South Africa";
  if (key == "middle_east_gulf") return "Gulf / UAE";
  if (key == "india") return "India";
  if (key == "thailand") return "Thailand / ICT";
  if (key == "singapore") return "Singapore";
  if (key == "china") return "China";
  if (key == "us_eastern") return "US Eastern";
  if (key == "us_central") return "US Central";
  if (key == "us_mountain") return "US Mountain";
  if (key == "us_arizona") return "US Arizona";
  if (key == "us_pacific") return "US Pacific";
  if (key == "alaska") return "Alaska";
  if (key == "hawaii") return "Hawaii";
  if (key == "canada_atlantic") return "Canada Atlantic";
  if (key == "brazil_east") return "Brazil East";
  if (key == "argentina") return "Argentina";
  if (key == "asia_tokyo") return "Japan";
  if (key == "korea") return "South Korea";
  if (key == "australia_perth") return "Australia West";
  if (key == "australia_darwin") return "Australia Central";
  if (key == "australia_sydney") return "Australia East";
  if (key == "new_zealand") return "New Zealand";
  return "Singapore";
}


String sanitizeTimezoneKey(const String& key) {
  const char* supported[] = {
    "utc", "atlantic_azores", "europe_west", "uk", "europe_central", "europe_east",
    "africa_south", "middle_east_gulf", "india", "thailand", "singapore", "china",
    "us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific",
    "alaska", "hawaii", "canada_atlantic", "brazil_east", "argentina",
    "asia_tokyo", "korea", "australia_perth", "australia_darwin",
    "australia_sydney", "new_zealand"
  };
  for (const char* supportedKey : supported) {
    if (key == supportedKey) return key;
  }
  return "singapore";
}


void applyDeviceTimezoneByKey(const String& key) {
  timezoneKey = sanitizeTimezoneKey(key);
  setenv("TZ", timezonePosixByKey(timezoneKey), 1);
  tzset();
}


// Spec "Home" > "Layout": Bus/Outdoor/Rain/Timer, fixed order, fixed
// coordinates -- indices 0-3 respectively. No per-user placement.
void getHomeTileRect(int tileIdx, int& x, int& y, int& w, int& h) {
  const int xs[4] = {HOME_TILE_BUS_X, HOME_TILE_OUTDOOR_X, HOME_TILE_RAIN_X, HOME_TILE_TIMER_X};
  const int ys[4] = {HOME_TILE_BUS_Y, HOME_TILE_OUTDOOR_Y, HOME_TILE_RAIN_Y, HOME_TILE_TIMER_Y};
  x = xs[tileIdx];
  y = ys[tileIdx];
  w = HOME_TILE_W;
  h = HOME_TILE_H;
}


void clearHomeTileCaches() {
  for (int i = 0; i < 4; i++) {
    cacheHomeTiles[i] = "";
  }
}


// Focus timer -- spec "Timer states": idle / running / paused / done.
FocusTimerState focusTimerState = TIMER_IDLE;

int focusDurationMin = FOCUS_DEFAULT_DURATION_MIN;

unsigned long focusEndMs = 0;

unsigned long focusRemainingSec = (unsigned long)FOCUS_DEFAULT_DURATION_MIN * 60UL;

unsigned long focusDoneStartedMs = 0;

int timerPresetMin[FOCUS_PRESET_COUNT] = {
  FOCUS_PRESET_DEFAULT_MIN[0], FOCUS_PRESET_DEFAULT_MIN[1], FOCUS_PRESET_DEFAULT_MIN[2],
  FOCUS_PRESET_DEFAULT_MIN[3], FOCUS_PRESET_DEFAULT_MIN[4], FOCUS_PRESET_DEFAULT_MIN[5]
};

bool timerPickerOpen = false;

bool timerCustomEntryOpen = false;

int customTimerMinutes = FOCUS_DEFAULT_DURATION_MIN;

bool wifiEnabled = true;

bool wifiConnectInProgress = false;

unsigned long wifiConnectStartedMs = 0;


// Weather
float tempC = NAN;

float tempMinC = NAN;

float tempMaxC = NAN;

float precipMm = NAN;

float windSpeedMs = NAN;

float windDirectionDeg = NAN;

float uvIndex = NAN;

float uvIndexMax = NAN;

float apparentTempC = NAN;

float humidity = NAN;

int weatherCode = -1;

bool weatherIsDay = true;

time_t lastWeatherFetch = 0;

FetchBackoff weatherBackoff;

float hourlyTempC[HOURLY_COUNT];

int hourlyCode[HOURLY_COUNT];

bool hourlyIsDay[HOURLY_COUNT];

int hourlyHour[HOURLY_COUNT];

int hourlyPrecipProb[HOURLY_COUNT];

int hourlyFilled = 0;

int dailyWeatherCode[DAILY_COUNT];

float dailyTempMaxC[DAILY_COUNT];

int dailyPrecipProbMax[DAILY_COUNT];

int dailyYmd[DAILY_COUNT];

int dailyFilled = 0;

bool weatherShowDaily = false;


// PSI (Singapore NEA haze index, national 24-hour reading via data.gov.sg v2)
String datagovApiKey = "";

float psiValue = NAN;

time_t lastPsiFetch = 0;

FetchBackoff psiBackoff;


// Bus arrival (LTA DataMall)
String ltaApiKey = "";


// Shared fallback AccountKey so bus arrivals work out of the box even before
// a user has requested their own key. Never stored/shown via Preferences or
// the settings page -- ltaApiKey itself always reflects only what the user
// has entered, and effectiveLtaApiKey() is the only thing that falls back.


String effectiveLtaApiKey() {
  return ltaApiKey.length() > 0 ? ltaApiKey : String(LTA_FALLBACK_API_KEY);
}

BusStopConfig busStops[BUS_MAX_STOPS];

int busActiveStopIdx = 0;

BusStopNameInfo busStopNames[BUS_MAX_STOPS];

BusServiceInfo busServicesByStop[BUS_MAX_STOPS][BUS_MAX_SERVICES];

int busServiceCountByStop[BUS_MAX_STOPS] = {0, 0, 0}
;

bool busDataValidByStop[BUS_MAX_STOPS] = {false, false, false}
;

time_t lastBusFetchByStop[BUS_MAX_STOPS] = {0, 0, 0}
;

FetchBackoff busBackoff[BUS_MAX_STOPS];

int busServicePage[BUS_MAX_STOPS] = {0, 0, 0}
;


String busStopDisplayName(int idx) {
  if (idx < 0 || idx >= BUS_MAX_STOPS) return "";
  if (busStops[idx].label.length() > 0) return busStops[idx].label;
  if (busStopNames[idx].valid) {
    if (busStopNames[idx].description.length() > 0) return busStopNames[idx].description;
    if (busStopNames[idx].roadName.length() > 0) return busStopNames[idx].roadName;
  }
  return "";
}


// Sunrise / Sunset
int sunriseMin = -1;

int sunsetMin  = -1;

int lastSunYmd = -1;

time_t lastSyncTime = 0;

FetchBackoff sunBackoff;


bool sleepDimmed = false;

bool sleepOff = false;

bool manualDimMode = false;


unsigned long lastInteractionMs = 0;


int sleepIntervalMin = 10;

int sleepOffDelaySec = 60;


int touchCalXMin = TOUCH_X_MIN;
int touchCalXMax = TOUCH_X_MAX;
int touchCalYMin = TOUCH_Y_MIN;
int touchCalYMax = TOUCH_Y_MAX;


void saveTouchCalibration(int xMin, int xMax, int yMin, int yMax) {
  touchCalXMin = xMin;
  touchCalXMax = xMax;
  touchCalYMin = yMin;
  touchCalYMax = yMax;
  prefs.putInt("touchXMin", xMin);
  prefs.putInt("touchXMax", xMax);
  prefs.putInt("touchYMin", yMin);
  prefs.putInt("touchYMax", yMax);
}

// =========================================================
// HELPERS
// =========================================================
bool timeIsSynced() {
  return time(nullptr) >= 1700000000;
}


int ymdFromLocal(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return (tmLocal.tm_year + 1900) * 10000 + (tmLocal.tm_mon + 1) * 100 + tmLocal.tm_mday;
}


int minutesFromLocalEpoch(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return tmLocal.tm_hour * 60 + tmLocal.tm_min;
}


int minutesNowLocal() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return tmNow.tm_hour * 60 + tmNow.tm_min;
}




int sanitizeTimerMinutes(int minutes) {
  return constrain(minutes, FOCUS_CUSTOM_MIN_MINUTES, FOCUS_CUSTOM_MAX_MINUTES);
}


void saveTimerPreset(int idx, int minutes) {
  if (idx < 0 || idx >= FOCUS_PRESET_COUNT) return;
  timerPresetMin[idx] = sanitizeTimerMinutes(minutes);
  prefs.putInt((String("timerPreset") + String(idx)).c_str(), timerPresetMin[idx]);
}


// Spec "Home" > "Interactions"/"Timer states": tap is state-aware -- idle
// and paused both open the timer picker (display.cpp) rather than
// starting/resuming directly, since picking a preset or Custom is itself
// "go" (matches the old pre-V2.1.0 MakerPulse menu: selecting a duration
// starts the timer, it's not a separate select-then-press-start step).
// Running still just pauses on tap, unchanged. Done has its own dismiss
// gesture (any tap, anywhere) rather than reusing this tap handler.
void handleTimerTileTap() {
  switch (focusTimerState) {
    case TIMER_IDLE:
    case TIMER_PAUSED:
      openTimerPicker();
      break;
    case TIMER_RUNNING:
      focusTimerState = TIMER_PAUSED; // freezes focusRemainingSec as-is
      cacheFocusTimer = "";
      clearHomeTileCaches();
      pageDirty = true;
      break;
    case TIMER_DONE:
      break;
  }
}


void openTimerPicker() {
  timerPickerOpen = true;
  timerCustomEntryOpen = false;
  cacheTimerPicker = "";
  pageDirty = true;
}


void closeTimerPicker() {
  timerPickerOpen = false;
  timerCustomEntryOpen = false;
  pageDirty = true;
}


void resumeFocusTimer() {
  if (focusTimerState != TIMER_PAUSED) return;
  focusEndMs = millis() + focusRemainingSec * 1000UL;
  focusTimerState = TIMER_RUNNING;
  closeTimerPicker();
  cacheFocusTimer = "";
  clearHomeTileCaches();
}


void stopFocusTimerToIdle() {
  focusTimerState = TIMER_IDLE;
  focusRemainingSec = (unsigned long)focusDurationMin * 60UL;
  focusEndMs = 0;
  closeTimerPicker();
  cacheFocusTimer = "";
  clearHomeTileCaches();
}


void startFocusTimerWithMinutes(int minutes) {
  minutes = sanitizeTimerMinutes(minutes);
  focusDurationMin = minutes;
  prefs.putInt("focusDurMin", minutes);
  focusRemainingSec = (unsigned long)minutes * 60UL;
  focusEndMs = millis() + focusRemainingSec * 1000UL;
  focusTimerState = TIMER_RUNNING;
  closeTimerPicker();
  cacheFocusTimer = "";
  clearHomeTileCaches();
}


// Pre-fills with focusDurationMin -- "whatever duration was last actually
// run", not a fixed default like the old pre-V2.1.0 "20 min".
void openCustomTimerStepper() {
  timerCustomEntryOpen = true;
  customTimerMinutes = sanitizeTimerMinutes(focusDurationMin);
  cacheCustomStepper = "";
  pageDirty = true;
}


void adjustCustomTimerMinutes(int delta) {
  if (!timerCustomEntryOpen) return;
  customTimerMinutes = sanitizeTimerMinutes(customTimerMinutes + delta);
  cacheCustomStepper = "";
  pageDirty = true;
}


// Explicit Cancel button: back to the picker (one level up), not all the
// way to idle -- see CONTEXT.md's "timer picker" entry for the distinction
// from tapping outside the stepper, which closes everything instead.
void cancelCustomTimerStepper() {
  timerCustomEntryOpen = false;
  cacheTimerPicker = "";
  pageDirty = true;
}


void dismissTimerDone() {
  if (focusTimerState != TIMER_DONE) return;
  focusTimerState = TIMER_IDLE;
  focusRemainingSec = (unsigned long)focusDurationMin * 60UL;
  focusDoneStartedMs = 0;
  cacheTimerDone = "";
  cacheFocusTimer = "";
  clearHomeTileCaches();
  if (!sleepDimmed && !sleepOff) setBacklight(brightnessPwm());
  pageDirty = true;
}


void updateFocusTimerState() {
  if (focusTimerState != TIMER_RUNNING) return;

  unsigned long now = millis();
  if ((long)(focusEndMs - now) <= 0) {
    focusRemainingSec = 0;
    focusTimerState = TIMER_DONE;
    focusDoneStartedMs = now;
    cacheFocusTimer = "";
    clearHomeTileCaches();
    cacheTimerDone = "";
    wakeDisplay();
    setBacklight(BL_FULL); // spec: "Backlight goes to High"
    pageDirty = true;
    return;
  }

  unsigned long remainingMs = focusEndMs - now;
  unsigned long nextRemainingSec = (remainingMs + 999UL) / 1000UL;
  if (nextRemainingSec != focusRemainingSec) {
    focusRemainingSec = nextRemainingSec;
    cacheFocusTimer = "";
    clearHomeTileCaches();
  }
}


void updateTimerDoneAutoDismiss() {
  if (focusTimerState != TIMER_DONE) return;
  if (millis() - focusDoneStartedMs >= TIMER_DONE_DIALOG_MS) {
    dismissTimerDone();
  }
}


// =========================================================
// COLOURWAY (see docs/MAKERPULSE_V2_SPEC.md "Colour roles"/"Colourways",
// and CONTEXT.md's "Colourway" entry)
// =========================================================
// Five roles per palette -- bg, ink, muted, rule, accent -- values are the
// spec's own pre-computed RGB565 hex, used verbatim (no runtime derivation,
// no WCAG math here: the spec states all pairs already clear AA 4.5:1).
// This supersedes the 8 MakerPulse-filament "shell colour" presets (see
// docs/adr/0002, now superseded by docs/adr/0003) -- same mechanism (one
// Preferences key, one firmware image), different, spec-authored palette.
// Charcoal doubles as the night-mode palette (applyNightOverride() in
// display.cpp, Phase 7). COL_PANEL/COL_PANEL_ALT are legacy fields kept
// equal to bg/rule only so Weather/Bus/Status (not yet rewritten to the
// "no boxes" spec) still compile and look reasonable -- they go away once
// those pages are redone.
struct Colourway {
  const char* key;
  const char* label;
  uint16_t caseColor; // physical case swatch shown on Status, not drawn elsewhere
  uint16_t bg, ink, muted, rule, accent;
};

static const Colourway COLOURWAYS[] = {
  { "sage",     "Sage",     0xA591, 0xDF3A, 0x1943, 0x5329, 0xBE35, 0x3B46 },
  { "ivory",    "Ivory",    0xEF1A, 0xF79D, 0x18E3, 0x62EA, 0xDE97, 0xBA01 },
  { "sky",      "Sky",      0x95FC, 0xE77E, 0x1127, 0x4B0E, 0xBE9C, 0x1B34 },
  { "peach",    "Peach",    0xF531, 0xFF5C, 0x38A2, 0x7A68, 0xEE36, 0xB9E1 },
  { "lilac",    "Lilac",    0xB53B, 0xEF5E, 0x20C7, 0x5AAF, 0xD65D, 0x59B9 },
  { "charcoal", "Charcoal", 0x2965, 0x18E3, 0xF79D, 0xAD54, 0x39C7, 0xF5A0 },
};
static const int COLOURWAY_COUNT = sizeof(COLOURWAYS) / sizeof(COLOURWAYS[0]);
static const int CHARCOAL_INDEX = 5; // COLOURWAYS[5] -- also the night-mode palette


bool isValidShellColourKey(const String& key) {
  for (int i = 0; i < COLOURWAY_COUNT; i++) {
    if (key == COLOURWAYS[i].key) return true;
  }
  return false;
}


const char* shellColourLabel(const String& key) {
  for (int i = 0; i < COLOURWAY_COUNT; i++) {
    if (key == COLOURWAYS[i].key) return COLOURWAYS[i].label;
  }
  return COLOURWAYS[0].label;
}


int shellColourCount() { return COLOURWAY_COUNT; }


// For the settings page's swatch row -- iterate idx 0..shellColourCount()-1.
void shellColourAt(int idx, const char** key, const char** label, uint16_t* swatch) {
  if (idx < 0 || idx >= COLOURWAY_COUNT) idx = 0;
  *key = COLOURWAYS[idx].key;
  *label = COLOURWAYS[idx].label;
  *swatch = COLOURWAYS[idx].caseColor;
}


// For #define COLOUR_TEST -- all 5 roles, not just the case swatch.
void colourwayRolesAt(int idx, uint16_t* bg, uint16_t* ink, uint16_t* muted, uint16_t* rule, uint16_t* accent) {
  if (idx < 0 || idx >= COLOURWAY_COUNT) idx = 0;
  *bg = COLOURWAYS[idx].bg;
  *ink = COLOURWAYS[idx].ink;
  *muted = COLOURWAYS[idx].muted;
  *rule = COLOURWAYS[idx].rule;
  *accent = COLOURWAYS[idx].accent;
}


static void applyColourway(const Colourway& p) {
  COL_BG = p.bg;
  COL_TEXT = p.ink;
  COL_DIM = p.muted;
  COL_STROKE = p.rule;
  COL_ACCENT = p.accent;
  COL_PANEL = p.bg;     // legacy -- see comment above the struct
  COL_PANEL_ALT = p.rule;
}


void applyShellColourByKey(const String& key) {
  const Colourway* p = &COLOURWAYS[0];
  for (int i = 0; i < COLOURWAY_COUNT; i++) {
    if (key == COLOURWAYS[i].key) { p = &COLOURWAYS[i]; break; }
  }
  shellColourKey = p->key;
  applyColourway(*p);
}


// Spec "Status" > "Night mode Auto": swaps the live palette to Charcoal +
// Low brightness between sunset and sunrise (19:00-07:00 fallback if sun
// times aren't known yet) without touching shellColourKey itself -- the
// chosen colourway reapplies automatically at sunrise, and the Status
// page's own swatch selection still reflects what the user actually
// picked, not whatever's visually active overnight. Called once a second
// from main.cpp; the static guards against redundant reapplication (and
// the backlight/pageDirty churn that would cause) every tick.
void applyNightOverride() {
  static bool nightCurrentlyActive = false;

  bool nightActive = false;
  if (nightModeKey == "auto") {
    int nowMin = minutesNowLocal();
    int startMin = (sunriseMin >= 0 && sunsetMin >= 0) ? sunsetMin : (19 * 60);
    int endMin   = (sunriseMin >= 0 && sunsetMin >= 0) ? sunriseMin : (7 * 60);
    nightActive = (startMin > endMin)
                    ? (nowMin >= startMin || nowMin < endMin)  // wraps midnight
                    : (nowMin >= startMin && nowMin < endMin);
  }

  if (nightActive == nightCurrentlyActive) return;
  nightCurrentlyActive = nightActive;

  if (nightActive) {
    applyColourway(COLOURWAYS[CHARCOAL_INDEX]);
    if (!sleepDimmed && !sleepOff) setBacklight(BRIGHTNESS_LOW);
  } else {
    applyShellColourByKey(shellColourKey);
    if (!sleepDimmed && !sleepOff) setBacklight(brightnessPwm());
  }
  pageDirty = true;
}


// Derives rotationValue/touchFlipX/touchFlipY from orientationKey and
// applies them to the already-initialized tft/ts hardware. Must only be
// called after tft.init() and ts.begin() have run -- loadStoredSettings()
// runs earlier in setup(), before that hardware is ready, so it only sets
// orientationKey; main.cpp calls this separately once tft/ts are ready, and
// settings_server.cpp calls it again (hot-apply, no reboot) when the
// setting is changed.
void applyOrientationByKey(const String& key) {
  orientationKey = (key == "bottom") ? "bottom" : "top";

  if (orientationKey == "bottom") {
    rotationValue = 0;
    touchFlipX = false;
    touchFlipY = false;
  } else {
    rotationValue = 2;
    touchFlipX = false;
    touchFlipY = false;
  }

  tft.setRotation(rotationValue);
  ts.setRotation(rotationValue);
}


void loadStoredSettings() {
  prefs.begin("makerpulse", false);

  storedWifiSsid    = prefs.getString("wifiSsid", "");
  storedWifiPass    = prefs.getString("wifiPass", "");
  deviceProvisioned = storedWifiSsid.length() > 0;

  // One of the spec's 6 fixed colourways -- see CONTEXT.md's "Colourway"
  // entry. No custom picker in V2; an invalid/missing key falls back to
  // the first preset (Sage, the spec's own default).
  String shellColour = prefs.getString("shellColour", "sage");
  if (!isValidShellColourKey(shellColour)) shellColour = "sage";

  deviceNickname    = prefs.getString("nickname", "");
  locationName     = prefs.getString("locname", "Singapore");
  LAT              = prefs.getFloat("lat", 1.3521f);
  LNG              = prefs.getFloat("lng", 103.8198f);
  sleepIntervalMin = prefs.getInt("sleepMin", 10);
  unitKey          = prefs.getString("units", "metric");
  regionFormatKey  = prefs.getString("region", "europe");
  timezoneKey      = sanitizeTimezoneKey(prefs.getString("tz", "singapore"));
  clockIs24h       = prefs.getBool("clock24h", true);
  ownerName        = prefs.getString("ownerName", "");
  brightnessLevel  = constrain(prefs.getInt("brightness", 2), 0, 2);
  nightModeKey     = prefs.getString("nightMode", "off");
  if (nightModeKey != "auto") nightModeKey = "off";
  favBusService    = prefs.getString("favBusSvc", "");
  focusDurationMin = sanitizeTimerMinutes(prefs.getInt("focusDurMin", FOCUS_DEFAULT_DURATION_MIN));
  focusRemainingSec = (unsigned long)focusDurationMin * 60UL;
  for (int i = 0; i < FOCUS_PRESET_COUNT; i++) {
    timerPresetMin[i] = sanitizeTimerMinutes(prefs.getInt((String("timerPreset") + String(i)).c_str(), FOCUS_PRESET_DEFAULT_MIN[i]));
  }
  wifiEnabled      = prefs.getBool("wifiEnabled", true);
  datagovApiKey    = prefs.getString("datagovKey", "");
  ltaApiKey        = prefs.getString("ltaKey", "");
  orientationKey   = prefs.getString("orientation", "top");
  if (orientationKey != "bottom") orientationKey = "top";

  touchCalXMin = prefs.getInt("touchXMin", TOUCH_X_MIN);
  touchCalXMax = prefs.getInt("touchXMax", TOUCH_X_MAX);
  touchCalYMin = prefs.getInt("touchYMin", TOUCH_Y_MIN);
  touchCalYMax = prefs.getInt("touchYMax", TOUCH_Y_MAX);

  String legacyBusStop = prefs.getString("busStop", "");
  String legacyBusSvc  = prefs.getString("busSvc", "");
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    String codeKey  = String("busCode") + String(i);
    String labelKey = String("busLabel") + String(i);
    String svcKey   = String("busSvc") + String(i);
    String nameKey  = String("busName") + String(i);
    String roadKey  = String("busRoad") + String(i);

    busStops[i].code      = prefs.getString(codeKey.c_str(), i == 0 ? legacyBusStop : "");
    busStops[i].label     = prefs.getString(labelKey.c_str(), "");
    busStops[i].svcFilter = prefs.getString(svcKey.c_str(), i == 0 ? legacyBusSvc : "");

    busStopNames[i].roadName    = prefs.getString(roadKey.c_str(), "");
    busStopNames[i].description = prefs.getString(nameKey.c_str(), "");
    busStopNames[i].valid = busStopNames[i].description.length() > 0 || busStopNames[i].roadName.length() > 0;
  }

  if (unitKey != "metric" && unitKey != "imperial") unitKey = "metric";
  if (regionFormatKey != "europe" && regionFormatKey != "us") regionFormatKey = "europe";
  deviceNickname.trim();
  applyShellColourByKey(shellColour);
  applyDeviceTimezoneByKey(timezoneKey);
}


void resetDataCaches() {
  tempC = NAN;
  precipMm = NAN;
  windSpeedMs = NAN;
  windDirectionDeg = NAN;
  sunriseMin = -1;
  sunsetMin = -1;
  lastSunYmd = -1;
  lastWeatherFetch = 0;
  dataDirty = true;
  pageDirty = true;
}
