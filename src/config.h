#pragma once

// Compile-time configuration: pin assignments, display/touch calibration,
// layout geometry, and the small value types shared across modules.
// No mutable state and no function bodies live here.

#include <Arduino.h>
#include <TFT_eSPI.h>

#define FIRMWARE_VERSION "2.1.0"
#define SETUP_SITE_URL "theyoungmaker.github.io/MakerPulseV2"

// The Young Maker brand mark: a maker figure under three Wi-Fi arcs, reused
// as-is in the settings page header/footer. Colors sampled from their real
// logo asset and live site (theyoungmaker.com) rather than invented here.
#define YM_LOGO_SVG "<svg viewBox='0 0 100 130' xmlns='http://www.w3.org/2000/svg'><path d='M38 46a17 17 0 0 1 24 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><path d='M27 34a33 33 0 0 1 46 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><path d='M16 22a53 53 0 0 1 68 0' fill='none' stroke='#fe9804' stroke-width='6' stroke-linecap='round'/><circle cx='50' cy='62' r='9' fill='#262220'/><path d='M27 86q23 20 46 0' fill='none' stroke='#262220' stroke-width='7' stroke-linecap='round'/><path d='M50 86L30 124' stroke='#262220' stroke-width='7' stroke-linecap='round'/><path d='M50 86L70 124' stroke='#262220' stroke-width='7' stroke-linecap='round'/></svg>"

#define TOUCH_CS  33
#define TOUCH_IRQ 36

// ROT, TOUCH_FLIP_X and TOUCH_FLIP_Y used to be fixed here. They are now
// runtime values (rotationValue, touchFlipX, touchFlipY in state.h) derived
// from the persisted "orientation" setting -- see applyOrientationByKey()
// in state.cpp and CONTEXT.md's "Screen orientation" entry.

const bool INV = false;

static const int T_SCK  = 25;

static const int T_MISO = 39;

static const int T_MOSI = 32;


static const int TOUCH_X_MIN = 562;

static const int TOUCH_X_MAX = 3604;

static const int TOUCH_Y_MIN = 544;

static const int TOUCH_Y_MAX = 3720;


static const bool TOUCH_SWAP_XY = false;


const uint16_t COL_GREEN  = TFT_GREEN;

const uint16_t COL_YELLOW = 0xFFE0;

const uint16_t COL_RED    = TFT_RED;

const uint16_t COL_BLUE   = 0x041F;


// =========================================================
// LAYOUT
// =========================================================
const int SCREEN_W = 240;

const int SCREEN_H = 320;

const int TOPBAR_H = 34;


// =========================================================
// V2.1.0 SPEC GRID (docs/MAKERPULSE_V2_SPEC.md "Screen grid"/"Touch and
// gestures")
// =========================================================
const int V2_MARGIN_L = 12;

const int V2_MARGIN_R = 228; // SCREEN_W - 12

const int V2_CONTENT_W = V2_MARGIN_R - V2_MARGIN_L; // 216

const int V2_HEADER_Y = 12;

const int V2_CONTENT_Y0 = 26;

const int V2_CONTENT_Y1 = 272;

const int V2_TABBAR_Y0 = 280;

const int V2_TABBAR_Y1 = 320;

const int V2_TAB_TOUCH_Y0 = 276; // touch strip starts above the visual tab bar

const int V2_TAB_CELL_W = 54;

const int V2_TAP_MAX_MS = 400;

const int V2_TAP_MAX_MOVE_PX = 10;

const int V2_LONGPRESS_MS = 600;

const int V2_MIN_TOUCH_TARGET = 36;

const int V2_CALIBRATION_HOLD_MS = 5000;

// Spec "Touch and gestures" > "Calibration": two-point crosshair targets,
// inset from the edges so a mis-calibrated device can still hit them.
const int CAL_PT_A_X = 30, CAL_PT_A_Y = 30;
const int CAL_PT_B_X = 210, CAL_PT_B_Y = 290;


// =========================================================
// STATUS (spec "Status" section)
// =========================================================
const int STATUS_SWATCH_Y = 118;
const int STATUS_SWATCH_R = 13;       // 26px diameter
const int STATUS_SWATCH_RING_R = 17;  // 34px diameter selection ring
const int STATUS_SWATCH_PITCH = 36;
const int STATUS_SWATCH_X0 = 30;      // first swatch centre
const int STATUS_SWATCH_HIT = 18;     // half of the 36x36 hit box

const int STATUS_BRIGHTNESS_Y = 148;
const int STATUS_NIGHT_Y = 180;
const int STATUS_SEGMENTED_GAP = 14;

const int STATUS_INFO_Y0 = 216;
const int STATUS_INFO_ROW_H = 15;


// =========================================================
// HOME (spec "Home" section) -- four fixed tiles, no widget
// customization. Tile anatomy (spec "Components"): 104x64, label at
// y+0, value at y+16, sub-line at y+52.
// =========================================================
const int HOME_TILE_W = 104;
const int HOME_TILE_H = 64;

const int HOME_TILE_BUS_X = 12,     HOME_TILE_BUS_Y = 112;
const int HOME_TILE_OUTDOOR_X = 124, HOME_TILE_OUTDOOR_Y = 112;
const int HOME_TILE_RAIN_X = 12,    HOME_TILE_RAIN_Y = 188;
const int HOME_TILE_TIMER_X = 124,  HOME_TILE_TIMER_Y = 188;

// Clock long-press (toggle 12h/24h) hit region -- covers the widest case
// ("23:58", spec's own example) plus slack, well clear of the day/date/
// month stack at the right margin.
const int HOME_CLOCK_HIT_X0 = 0,   HOME_CLOCK_HIT_X1 = 180;
const int HOME_CLOCK_HIT_Y0 = 0,   HOME_CLOCK_HIT_Y1 = 72;

// Focus timer: fixed 5/10/15/25/45 min cycle (spec "Interactions" --
// long-press while idle cycles this list; no custom entry, no per-user
// editing). Index 3 (25 min) is the default, matching the mockups.
static const int FOCUS_DURATIONS_MIN[] = {5, 10, 15, 25, 45};
static const int FOCUS_DURATIONS_COUNT = 5;
static const int FOCUS_DEFAULT_DURATION_IDX = 3;

enum FocusTimerState {
  TIMER_IDLE = 0,
  TIMER_RUNNING,
  TIMER_PAUSED,
  TIMER_DONE
};

// Spec "Timer done takeover": alternates accent/bg 3 times at 500 ms, then
// settles on the accent fill until dismissed or the 60 s auto-dismiss.
static const unsigned long TIMER_DONE_FLASH_MS = 500;
static const int TIMER_DONE_FLASH_COUNT = 3;


// =========================================================
// STATE
// =========================================================
enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_BUS = 2,
  PAGE_STATUS = 3
}
;


const unsigned long CLOCK_TICK_MS = 1000;

const unsigned long DATA_TICK_MS  = 30UL * 1000UL;

const unsigned long TIMER_DONE_DIALOG_MS = 60UL * 1000UL;

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000UL;

// Spec "Data sources and refresh cadence": weather refreshes every 15min,
// stale after 30min (Status's Synced row derives "stale" as 2x the
// refresh interval, so this one constant drives both).
static const uint32_t WEATHER_INTERVAL_SEC = 15 * 60;


static const int HOURLY_COUNT = 6; // spec "Hourly strip": now + next 5 hours

static const int DAILY_COUNT = 6; // today + next 5 days, for the 5-day toggle view

// Spec: PSI refreshes every 60min, stale after 2h (again 2x).
static const uint32_t PSI_INTERVAL_SEC = 60 * 60;
#define LTA_FALLBACK_API_KEY "2jQlivifRK227X26Uk+bdA=="



struct BusStopConfig {
  String code = "";
  String label = "";
  String svcFilter = "";
}
;

static const int BUS_MAX_STOPS = 3;


struct BusStopNameInfo {
  String roadName = "";
  String description = "";
  bool valid = false;
  time_t lastAttempt = 0;
}
;

static const uint32_t BUS_NAME_RETRY_SEC = 10 * 60;


// Spec "Staleness and offline": "On failure, retry with back-off: 30s,
// 60s, 120s, then the normal interval." One instance per feed (weather,
// PSI, sun, each bus stop); isDue()/recordResult() share the schedule so
// it's defined once instead of once per fetch function.
struct FetchBackoff {
  int failCount = 0;
  time_t nextRetryAt = 0;
};

inline bool fetchBackoffDue(const FetchBackoff& b, time_t now) {
  return now >= b.nextRetryAt;
}

inline void fetchBackoffRecord(FetchBackoff& b, time_t now, bool ok, uint32_t normalIntervalSec) {
  if (ok) {
    b.failCount = 0;
    b.nextRetryAt = now + (time_t)normalIntervalSec;
    return;
  }
  static const uint32_t steps[] = {30, 60, 120};
  uint32_t delaySec = (b.failCount < 3) ? steps[b.failCount] : normalIntervalSec;
  b.failCount++;
  b.nextRetryAt = now + (time_t)delaySec;
}


// Spec "Bus" > "Data rules": countdowns are recomputed every 15s from
// stored epochs, not re-fetched -- arrival1/2/3 are raw epoch seconds
// (0 = no NextBus at that slot), never pre-converted to "minutes from now"
// at fetch time (that would go stale between fetches).
struct BusServiceInfo {
  String serviceNo = "";
  time_t arrival1 = 0;
  time_t arrival2 = 0;
  time_t arrival3 = 0;
  bool monitored = true;   // LTA "Monitored"==0 means a scheduled (GPS-less) estimate
  int loadPips = 0;        // 1=Seats(SEA), 2=Standing(SDA), 3=Packed(LSD), 0=unknown
  String vehicleType = ""; // "SD"/"DD"/"BD" from the API, shown as-is
}
;

static const int BUS_MAX_SERVICES = 6;

// Spec "Data sources": "30s on Home or Bus, 120s on other pages or when
// dimmed." BUS_INTERVAL_SEC is the "elsewhere" rate; the 30s fast path is
// applied in ensureBusArrival() when Home or Bus is the visible page and
// the screen isn't dimmed. BUS_STALE_SEC is a flat 3 min regardless of
// which rate was actually active (unlike weather/PSI, bus's own refresh
// rate varies, so "2x the refresh interval" isn't a stable definition).
static const uint32_t BUS_INTERVAL_SEC = 120;
static const uint32_t BUS_FAST_INTERVAL_SEC = 30;
static const uint32_t BUS_STALE_SEC = 3 * 60;


// =========================================================
// SLEEP / BACKLIGHT
// =========================================================
const int BACKLIGHT_PIN = 21;


// Spec "Backlight and dimming": "Levels: Low 40, Mid 140, High 255" (the
// user-chosen Status-page brightness, see brightnessPwm() in state.h) and
// "After 5 min with no touch, fade to 12". BL_FULL is still used for the
// literal "Backlight goes to High" cases (boot flash, timer-done takeover)
// that are independent of the user's chosen level.
const int BL_FULL = 255;

const int BL_DIM  = 12;

const int BL_OFF  = 0;

const int BRIGHTNESS_LOW  = 40;

const int BRIGHTNESS_MID  = 140;

const int BRIGHTNESS_HIGH = 255;


// Spec "Bus" > "Layout": row top y = 56 + 52i, four rows per page.
const int BUS_ROW_Y0 = 56;
const int BUS_ROW_H = 52;
const int BUS_ROWS_PER_PAGE = 4;
