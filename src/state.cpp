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

// Default order matches the "Colourway theme" mockup: Bus, Outdoor, Rain,
// Timer. Still fully user-configurable via the settings page's Widget
// Customization panel, same as MakerPulse.
HomeWidgetType homeWidgetSlots[HOME_SLOT_COUNT] = {
  HOME_WIDGET_BUS,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_TIMER
}
;


String cacheHomeSlots[HOME_SLOT_COUNT];


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

String cacheTimerMenu = "";

String cacheCustomTimer = "";

String cacheBusStopEntry = "";

String cacheTimerDone = "";

String cacheTimerDoneCountdown = "";

String cacheTimerDoneFlash = "";


String lastWifiText = "";

String lastSignalText = "";

String lastIpText = "";

String lastUptimeText = "";

String lastUvText = "";

String lastUvLevelText = "";

String lastPsiText = "";

String lastPsiLevelText = "";

String lastNowText = "";

String lastHourlyText = "";

String lastBusPageText = "";

String lastNetworkToggleText = "";


const char* homeWidgetKey(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:    return "week";
    case HOME_WIDGET_TIMER:   return "timer";
    case HOME_WIDGET_RAIN:    return "rain";
    case HOME_WIDGET_OUTDOOR: return "outdoor";
    case HOME_WIDGET_KP:      return "kp";
    case HOME_WIDGET_UV:      return "uv";
    case HOME_WIDGET_WIND:    return "wind";
    case HOME_WIDGET_SUN:     return "sun";
    case HOME_WIDGET_PSI:     return "psi";
    case HOME_WIDGET_BUS:     return "bus";
    default:                  return "week";
  }
}


const char* homeWidgetLabel(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:    return "Week";
    case HOME_WIDGET_TIMER:   return "Timer";
    case HOME_WIDGET_RAIN:    return "Rain";
    case HOME_WIDGET_OUTDOOR: return "Outdoor";
    case HOME_WIDGET_KP:      return "KP index";
    case HOME_WIDGET_UV:      return "UV index";
    case HOME_WIDGET_WIND:    return "Wind";
    case HOME_WIDGET_SUN:     return "Sun event";
    case HOME_WIDGET_PSI:     return "PSI (haze)";
    case HOME_WIDGET_BUS:     return "Bus arrival";
    default:                  return "Week";
  }
}


HomeWidgetType homeWidgetFromKey(const String& key) {
  if (key == "week") return HOME_WIDGET_WEEK;
  if (key == "timer") return HOME_WIDGET_TIMER;
  if (key == "rain") return HOME_WIDGET_RAIN;
  if (key == "outdoor") return HOME_WIDGET_OUTDOOR;
  if (key == "kp") return HOME_WIDGET_KP;
  if (key == "uv") return HOME_WIDGET_UV;
  if (key == "wind") return HOME_WIDGET_WIND;
  if (key == "sun") return HOME_WIDGET_SUN;
  if (key == "psi") return HOME_WIDGET_PSI;
  if (key == "bus") return HOME_WIDGET_BUS;
  return HOME_WIDGET_WEEK;
}


const char* homeSlotLabel(int slot) {
  switch (slot) {
    case 0: return "Top left";
    case 1: return "Top right";
    case 2: return "Bottom left";
    case 3: return "Bottom right";
    default: return "Slot";
  }
}


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


// V2's Home page uses its own flat "Colourway theme" grid (see config.h's
// HOME2_* constants) -- only Home reads this function, so updating it here
// doesn't affect Weather/Bus/Status's still-unchanged layout.
void getHomeSlotRect(int slot, int& x, int& y, int& w, int& h) {
  const int xs[HOME_SLOT_COUNT] = {HOME2_MARGIN, HOME2_COL2_X, HOME2_MARGIN, HOME2_COL2_X};
  const int ys[HOME_SLOT_COUNT] = {HOME2_GRID_Y1, HOME2_GRID_Y1, HOME2_GRID_Y2, HOME2_GRID_Y2};
  x = xs[slot];
  y = ys[slot];
  w = HOME2_COL_W;
  h = HOME2_GRID_ROW_H;
}


void clearHomeSlotCaches() {
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }
}


// Focus timer
bool focusMenuOpen = false;

bool focusTimerRunning = false;

bool focusTimerFinished = false;

unsigned long focusEndMs = 0;

unsigned long focusDurationSec = 0;

unsigned long focusRemainingSec = 0;

bool timerDoneDialogOpen = false;

unsigned long timerDoneDialogStartedMs = 0;

bool flashModeEnabled = false;

int timerPresetMin[6] = {1, 5, 10, 15, 25, 30}
;

bool customTimerEntryOpen = false;

int customTimerMinutes = 20;

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

float apparentTempC = NAN;

int weatherCode = -1;

bool weatherIsDay = true;

time_t lastWeatherFetch = 0;

float hourlyTempC[HOURLY_COUNT];

int hourlyCode[HOURLY_COUNT];

bool hourlyIsDay[HOURLY_COUNT];

int hourlyHour[HOURLY_COUNT];

int hourlyFilled = 0;


// KP-index
float kpIndex = NAN;

time_t lastKpFetch = 0;


// PSI (Singapore NEA haze index)
String psiRegion = "central";

float psiValue = NAN;

time_t lastPsiFetch = 0;


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


bool busStopEntryOpen = false;

int busStopEntryTargetIdx = -1;

String busStopEntryDigits = "";


// Sunrise / Sunset
int sunriseMin = -1;

int sunsetMin  = -1;

int lastSunYmd = -1;

time_t lastSyncTime = 0;


bool sleepDimmed = false;

bool sleepOff = false;

bool manualDimMode = false;


unsigned long lastInteractionMs = 0;


int sleepIntervalMin = 10;

int sleepOffDelaySec = 60;

// =========================================================
// HELPERS
// =========================================================
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


String sanitizePsiRegion(const String& region) {
  if (region == "north" || region == "south" || region == "east" || region == "west" || region == "central") {
    return region;
  }
  return "central";
}


int sanitizeTimerMinutes(int value) {
  return constrain(value, 1, 180);
}


void resetFocusTimer() {
  focusTimerRunning = false;
  focusTimerFinished = false;
  focusMenuOpen = false;
  customTimerEntryOpen = false;
  timerDoneDialogOpen = false;
  focusEndMs = 0;
  focusDurationSec = 0;
  focusRemainingSec = 0;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheCustomTimer = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}


void startFocusTimer(unsigned long minutes) {
  focusDurationSec = minutes * 60UL;
  focusRemainingSec = focusDurationSec;
  focusEndMs = millis() + (focusDurationSec * 1000UL);
  focusTimerRunning = true;
  focusTimerFinished = false;
  focusMenuOpen = false;
  customTimerEntryOpen = false;
  timerDoneDialogOpen = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheCustomTimer = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
}


void dismissTimerDoneDialog() {
  if (!timerDoneDialogOpen) return;
  timerDoneDialogOpen = false;
  timerDoneDialogStartedMs = 0;
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);
  pageDirty = true;
}


void openTimerDoneDialog() {
  timerDoneDialogOpen = true;
  timerDoneDialogStartedMs = millis();
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  wakeDisplay();
}


void updateFocusTimerState() {
  if (!focusTimerRunning) return;

  unsigned long now = millis();
  if ((long)(focusEndMs - now) <= 0) {
    focusRemainingSec = 0;
    focusTimerRunning = false;
    focusTimerFinished = true;
    focusMenuOpen = false;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
    cacheTimerMenu = "";
    openTimerDoneDialog();
    return;
  }

  unsigned long remainingMs = focusEndMs - now;
  unsigned long nextRemainingSec = (remainingMs + 999UL) / 1000UL;
  if (nextRemainingSec != focusRemainingSec) {
    focusRemainingSec = nextRemainingSec;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
  }
}


void updateTimerDoneDialogState() {
  if (!timerDoneDialogOpen) return;
  if (millis() - timerDoneDialogStartedMs >= TIMER_DONE_DIALOG_MS) {
    dismissTimerDoneDialog();
  }
}


// =========================================================
// SHELL COLOUR (see CONTEXT.md's "Shell colour" entry)
// =========================================================
// The 8 fixed preset colours, one per physical 3D-printed case -- see
// "MakerPulse Colours.pdf" and docs/adr/0002-fixed-shell-colour-palette.md
// for where these came from and how BG/PANEL/STROKE/ACCENT were derived
// from each case's sampled body color (same hue family, pale tint for the
// screen background, explicitly checked for (a) WCAG AA against the shared
// ink/dim and (b) pairwise RGB565 distinctness -- the first version of
// this table passed (a) but not (b): several presets' tints rounded to the
// *same* 16-bit pixel value once quantized, so switching shells visibly
// changed nothing. Two shells (Muted White, Muted Green) have such low
// source saturation that preserving their "hue" produced an unstable,
// near-arbitrary tint that collided with unrelated shells; those two are
// derived as neutral (zero-saturation) tints instead. Text (ink) and dim
// are intentionally hue-NEUTRAL constants shared by all 8 -- not derived
// per shell -- so every preset gets the same contrast margin instead of
// needing 8 separate manual checks.
struct ShellColourPalette {
  const char* key;
  const char* label;
  uint16_t swatch; // raw case color, shown as the settings-page swatch
  uint16_t bg, panel, stroke, accent;
};

static const ShellColourPalette SHELL_COLOURS[] = {
  { "muted-white",      "Muted White",      0xD679, 0xE71C, 0xCE59, 0xA514, 0x51C5 },
  { "pastel-ice",       "Pastel Ice",       0x9DD7, 0xD71C, 0xBE7A, 0x7D97, 0x1AAC },
  { "muted-blue",       "Muted Blue",       0x6C54, 0xD6DC, 0xB61A, 0x74B7, 0x19CC },
  { "army-blue",        "Army Blue",        0x324D, 0xC67C, 0xA5BA, 0x6436, 0x19AC },
  { "muted-green",      "Muted Green",      0x5B2B, 0xD69A, 0xBDF7, 0x9492, 0x3A85 },
  { "army-red",         "Army Red",         0x8903, 0xE617, 0xDCF3, 0xCAAA, 0x60C2 },
  { "pastel-peach",     "Pastel Peach",     0xF674, 0xEF1A, 0xE656, 0xD50D, 0x69E1 },
  { "mandarin-orange",  "Mandarin Orange",  0xE3C7, 0xEEB9, 0xDDF4, 0xD44B, 0x6982 },
};
static const int SHELL_COLOUR_COUNT = sizeof(SHELL_COLOURS) / sizeof(SHELL_COLOURS[0]);

// Hue-neutral constant ink/dim, shared by every shell colour.
static const uint16_t SHELL_INK = 0x10A2;
static const uint16_t SHELL_DIM = 0x4A69;


bool isValidShellColourKey(const String& key) {
  for (int i = 0; i < SHELL_COLOUR_COUNT; i++) {
    if (key == SHELL_COLOURS[i].key) return true;
  }
  return false;
}


const char* shellColourLabel(const String& key) {
  for (int i = 0; i < SHELL_COLOUR_COUNT; i++) {
    if (key == SHELL_COLOURS[i].key) return SHELL_COLOURS[i].label;
  }
  return SHELL_COLOURS[0].label;
}


int shellColourCount() { return SHELL_COLOUR_COUNT; }


// For the settings page's swatch row -- iterate idx 0..shellColourCount()-1.
void shellColourAt(int idx, const char** key, const char** label, uint16_t* swatch) {
  if (idx < 0 || idx >= SHELL_COLOUR_COUNT) idx = 0;
  *key = SHELL_COLOURS[idx].key;
  *label = SHELL_COLOURS[idx].label;
  *swatch = SHELL_COLOURS[idx].swatch;
}


void applyShellColourByKey(const String& key) {
  const ShellColourPalette* p = &SHELL_COLOURS[0];
  for (int i = 0; i < SHELL_COLOUR_COUNT; i++) {
    if (key == SHELL_COLOURS[i].key) { p = &SHELL_COLOURS[i]; break; }
  }
  shellColourKey = p->key;
  COL_BG = p->bg;
  COL_PANEL = p->panel;
  COL_PANEL_ALT = p->panel;
  COL_STROKE = p->stroke;
  COL_ACCENT = p->accent;
  COL_TEXT = SHELL_INK;
  COL_DIM = SHELL_DIM;
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

  // One of the 8 fixed shell colours -- see CONTEXT.md's "Shell colour"
  // entry. No custom picker in V2; an invalid/missing key falls back to
  // the first preset.
  String shellColour = prefs.getString("shellColour", "muted-white");
  if (!isValidShellColourKey(shellColour)) shellColour = "muted-white";

  deviceNickname    = prefs.getString("nickname", "");
  locationName     = prefs.getString("locname", "Singapore");
  LAT              = prefs.getFloat("lat", 1.3521f);
  LNG              = prefs.getFloat("lng", 103.8198f);
  sleepIntervalMin = prefs.getInt("sleepMin", 10);
  unitKey          = prefs.getString("units", "metric");
  regionFormatKey  = prefs.getString("region", "europe");
  timezoneKey      = sanitizeTimezoneKey(prefs.getString("tz", "singapore"));
  flashModeEnabled = prefs.getBool("flashMode", false);
  wifiEnabled      = prefs.getBool("wifiEnabled", true);
  psiRegion        = sanitizePsiRegion(prefs.getString("psiRegion", "central"));
  ltaApiKey        = prefs.getString("ltaKey", "");
  orientationKey   = prefs.getString("orientation", "top");
  if (orientationKey != "bottom") orientationKey = "top";

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

  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    homeWidgetSlots[i] = homeWidgetFromKey(prefs.getString(key.c_str(), homeWidgetKey(homeWidgetSlots[i])));
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    timerPresetMin[i] = sanitizeTimerMinutes(prefs.getInt(key.c_str(), timerPresetMin[i]));
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
  kpIndex = NAN;
  sunriseMin = -1;
  sunsetMin = -1;
  lastSunYmd = -1;
  lastWeatherFetch = 0;
  lastKpFetch = 0;
  dataDirty = true;
  pageDirty = true;
}
