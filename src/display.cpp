#include "display.h"
#include "network.h"
#include <math.h>

// V2.1.0 spec font set (docs/MAKERPULSE_V2_SPEC.md "Type scale") -- see
// MakerPulseV2/CONTEXT.md's "Colourway" entry. Being wired in page by page
// across Phases 1-7; until a page's phase lands it still draws with
// whatever it had before.
#include "fonts/FDisplay.h"
#include "fonts/FNum.h"
#include "fonts/FTitle.h"
#include "fonts/FHead.h"
#include "fonts/FBodyReg.h"
#include "fonts/FBodyBold.h"
#include "fonts/FSmall.h"



static String ipText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-";
  return WiFi.localIP().toString();
}


static bool useUsRegionFormat() {
  return regionFormatKey == "us";
}


static String formatClockParts(const struct tm& tmValue, bool withSeconds) {
  char buf[20];
  const char* pattern = useUsRegionFormat()
    ? (withSeconds ? "%I:%M:%S %p" : "%I:%M %p")
    : (withSeconds ? "%H:%M:%S" : "%H:%M");
  strftime(buf, sizeof(buf), pattern, &tmValue);
  return String(buf);
}


static String formatDateParts(const struct tm& tmValue) {
  char buf[32];
  strftime(buf, sizeof(buf), useUsRegionFormat() ? "%a %m/%d/%Y" : "%a %d.%m.%Y", &tmValue);
  return String(buf);
}


static String tempText() {
  if (isnan(tempC)) return unitKey == "imperial" ? "--.-F" : "--.-C";

  if (unitKey == "imperial") {
    float f = tempC * 9.0f / 5.0f + 32.0f;
    return String(f, 1) + "F";
  }

  return String(tempC, 1) + "C";
}


static String formatDisplayTemp(float value) {
  if (isnan(value)) return "--";

  if (unitKey == "imperial") {
    float f = value * 9.0f / 5.0f + 32.0f;
    return String((int)roundf(f)) + "F";
  }

  return String((int)roundf(value)) + "C";
}


static String tempRangeText() {
  return "H:" + formatDisplayTemp(tempMaxC) + "  L:" + formatDisplayTemp(tempMinC);
}


// Home presentational helper -- degree-sign display instead of a C/F
// letter suffix (the spec's Home tiles show "29°", not "29C").
static String formatDegreeOnly(float value) {
  String t = formatDisplayTemp(value); // reuses the same rounding/unit conversion
  if (t == "--") return t;
  return t.substring(0, t.length() - 1) + "°";
}


static String tempRangeDegreeText() {
  return formatDegreeOnly(tempMinC) + "-" + formatDegreeOnly(tempMaxC);
}


// Spec "Home" > "Layout"/"Data rules": Rain tile shows the highest
// precipitation_probability over the next 3 hours (hourly indices 1..3 --
// index 0 is the current hour) and an intensity word from the strongest
// WMO weather code in that same window.
static int rainChancePercent() {
  if (hourlyFilled <= 1) return -1;
  int end = (hourlyFilled < HOURLY_COUNT) ? hourlyFilled : HOURLY_COUNT;
  int maxP = -1;
  for (int i = 1; i < end; i++) {
    if (hourlyPrecipProb[i] > maxP) maxP = hourlyPrecipProb[i];
  }
  return maxP;
}


static String rainChanceText() {
  int p = rainChancePercent();
  return p < 0 ? "--%" : String(p) + "%";
}


static int rainCodeRank(int code) {
  if (code == 95 || (code >= 96 && code <= 99)) return 4; // storm
  if (code == 65 || code == 82) return 3;                  // heavy
  if (code == 63 || code == 81) return 2;                  // showers
  if ((code >= 51 && code <= 55) || code == 61 || code == 80) return 1; // light
  return 0;
}


static String rainIntensityWord() {
  if (hourlyFilled <= 1) return "dry now";
  int end = (hourlyFilled < HOURLY_COUNT) ? hourlyFilled : HOURLY_COUNT;
  int bestRank = 0;
  for (int i = 1; i < end; i++) {
    int r = rainCodeRank(hourlyCode[i]);
    if (r > bestRank) bestRank = r;
  }
  switch (bestRank) {
    case 4: return "storm";
    case 3: return "heavy";
    case 2: return "showers";
    case 1: return "light";
    default: return "dry now";
  }
}


// Splits a leading numeric run (optional '-', digits, at most one '.') from
// the rest of a string -- e.g. "3 min" -> ("3", " min"), "29C" -> ("29","C"),
// "Due" -> ("", "Due"). Lets Home V2 draw the number in the big bold font
// and any letter suffix in the small font, for any widget's value string.
static void splitNumericPrefix(const String& s, String& numPart, String& restPart) {
  int i = 0;
  int n = s.length();
  if (i < n && s[i] == '-') i++;
  bool sawDigit = false, sawDot = false;
  while (i < n) {
    char c = s[i];
    if (c >= '0' && c <= '9') { sawDigit = true; i++; continue; }
    if (c == '.' && !sawDot) { sawDot = true; i++; continue; }
    break;
  }
  if (!sawDigit) { numPart = ""; restPart = s; return; }
  numPart = s.substring(0, i);
  restPart = s.substring(i);
}


static void drawDegreeRing(int cx, int cy, int r, uint16_t color) {
  tft.drawCircle(cx, cy, r, color);
}


// Draws `text` in the currently-set font, substituting small drawn glyphs
// for two UTF-8 sequences neither custom font contains: the degree sign
// (0xC2 0xB0, a ring) and the middle dot (0xC2 0xB7, a filled dot) -- the
// spec's own copy uses both throughout ("29°", "Sunrise 6:59 · Sunset
// 19:05", "paused · hold to reset"). Both glyphs are sized proportionally
// to the currently-set font's height, not a fixed pixel radius -- a radius
// tuned for an 11px label looks like a disconnected speck next to a 56px
// clock digit otherwise.
static int drawTextWithDegree(int x, int y, const String& text, uint16_t fg, uint16_t bg) {
  tft.setTextColor(fg, bg);
  tft.setTextDatum(TL_DATUM);
  int fh = tft.fontHeight();
  int n = text.length();
  int cx = x;
  int i = 0;
  String buf = "";

  while (i < n) {
    bool isDegree = (uint8_t)text[i] == 0xC2 && i + 1 < n && (uint8_t)text[i + 1] == 0xB0;
    bool isMiddot = (uint8_t)text[i] == 0xC2 && i + 1 < n && (uint8_t)text[i + 1] == 0xB7;
    if (isDegree || isMiddot) {
      if (buf.length() > 0) {
        tft.drawString(buf, cx, y);
        cx += tft.textWidth(buf);
        buf = "";
      }
      if (isDegree) {
        // A real ° sits near cap-height, at the upper third of the glyph box.
        int r = max(2, fh / 7);
        drawDegreeRing(cx + r + 1, y + r + 1, r, fg);
        cx += 2 * r + 4;
      } else {
        int r = max(1, fh / 10);
        tft.fillCircle(cx + r + 2, y + fh / 2, r, fg);
        cx += 2 * r + 6;
      }
      i += 2;
      continue;
    }
    buf += text[i];
    i++;
  }
  if (buf.length() > 0) {
    tft.drawString(buf, cx, y);
    cx += tft.textWidth(buf);
  }
  return cx;
}


// Width `text` would occupy if drawn via drawTextWithDegree() in the
// currently-set font -- same substitution widths (ring: 2*3+5, dot:
// 2*2+8), so a caller can centre text containing either glyph.
static int measureTextWithGlyphs(const String& text) {
  int fh = tft.fontHeight();
  int degreeW = 2 * max(2, fh / 7) + 4;
  int middotW = 2 * max(1, fh / 10) + 6;

  int n = text.length();
  int w = 0;
  int i = 0;
  String buf = "";
  while (i < n) {
    bool isDegree = (uint8_t)text[i] == 0xC2 && i + 1 < n && (uint8_t)text[i + 1] == 0xB0;
    bool isMiddot = (uint8_t)text[i] == 0xC2 && i + 1 < n && (uint8_t)text[i + 1] == 0xB7;
    if (isDegree || isMiddot) {
      if (buf.length() > 0) { w += tft.textWidth(buf); buf = ""; }
      w += isDegree ? degreeW : middotW;
      i += 2;
      continue;
    }
    buf += text[i];
    i++;
  }
  if (buf.length() > 0) w += tft.textWidth(buf);
  return w;
}


static void drawTextWithGlyphsCentered(int cx, int y, const String& text, uint16_t fg, uint16_t bg) {
  int w = measureTextWithGlyphs(text);
  drawTextWithDegree(cx - w / 2, y, text, fg, bg);
}


// Spec "Hourly column": "rain-bar track 12x20... rule... with an accent
// bar filled from the bottom, height = round(chance*20), minimum 2px."
static void drawRainBar(int cx, int y, int chancePercent, uint16_t trackColor, uint16_t fillColor, uint16_t bg) {
  int trackX = cx - 6, trackW = 12, trackH = 20;
  tft.fillRoundRect(trackX, y, trackW, trackH, 2, bg);
  tft.drawRoundRect(trackX, y, trackW, trackH, 2, trackColor);
  int fillH = (int)roundf(chancePercent * trackH / 100.0f);
  if (chancePercent > 0 && fillH < 2) fillH = 2;
  if (fillH > trackH - 2) fillH = trackH - 2; // keep the track's rounded top border visible
  if (fillH > 0) {
    tft.fillRoundRect(trackX, y + trackH - fillH, trackW, fillH, 2, fillColor);
  }
}


// Draws a value's leading numeric run in `bigFont` (bold) and any trailing
// letter suffix in font 1 (small) -- e.g. "3 min" -> bold "3" + small " min".
static void drawValueWithSuffix(int x, int y, const String& value, const GFXfont* bigFont, uint16_t fg, uint16_t bg) {
  String numPart, restPart;
  splitNumericPrefix(value, numPart, restPart);

  int cx = x;
  tft.setTextDatum(TL_DATUM);
  if (numPart.length() > 0) {
    tft.setFreeFont(bigFont);
    tft.setTextColor(fg, bg);
    tft.drawString(numPart, cx, y);
    cx += tft.textWidth(numPart);
    tft.setFreeFont(NULL);
  }
  if (restPart.length() > 0) {
    tft.setTextFont(1);
    int baselineShim = 10; // font1 sits visually high next to the big font's baseline
    drawTextWithDegree(cx + 2, y + baselineShim, restPart, fg, bg);
  }
}


// Spec "Type scale": "Small-caps labels are simply uppercase strings in
// F_SMALL; letter-spacing is applied by drawing with +1 px per character."
// Supports the three horizontal datums the spec's Home layout actually
// uses (left, centre, right); y is always the TL-style top of the glyphs.
// Returns the tracked width, in case a caller needs it (e.g. an underline).
// TFT_eSPI's textWidth() special-cases the *last* character of whatever
// string you pass it -- it uses (xOffset+width) instead of xAdvance for
// that one character, on the theory that a right-aligned string shouldn't
// count trailing bearing. That's fine for measuring a whole string once,
// but calling textWidth() on a series of 1-character strings (as
// drawSmallCapsLabel() used to) makes *every* character "the last one",
// so blank glyphs like space measure as 0 width instead of their real
// advance -- this is exactly why "BUS STOP" rendered as "BUSSTOP". Reads
// xAdvance straight out of the GFXglyph table instead, bypassing that
// per-call special case entirely.
static int gfxGlyphAdvance(const GFXfont* font, char c) {
  uint16_t first = pgm_read_word(&font->first);
  uint16_t last = pgm_read_word(&font->last);
  uint8_t uc = (uint8_t)c;
  if (uc < first || uc > last) return 0;
  GFXglyph* glyphs = (GFXglyph*)pgm_read_dword(&font->glyph);
  return pgm_read_byte(&glyphs[uc - first].xAdvance);
}


static int drawSmallCapsLabel(const String& text, int x, int y, uint8_t datum, uint16_t fg, uint16_t bg) {
  String upper = text;
  upper.toUpperCase();
  int n = upper.length();

  int totalW = 0;
  for (int i = 0; i < n; i++) totalW += gfxGlyphAdvance(&F_SMALL, upper[i]) + 1;
  if (n > 0) totalW -= 1; // no trailing gap after the last glyph

  int startX = x;
  if (datum == TC_DATUM) startX = x - totalW / 2;
  else if (datum == TR_DATUM) startX = x - totalW;

  tft.setFreeFont(&F_SMALL);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(TL_DATUM);

  int cx = startX;
  for (int i = 0; i < n; i++) {
    char ch = upper[i];
    tft.drawString(String(ch), cx, y);
    cx += gfxGlyphAdvance(&F_SMALL, ch) + 1;
  }

  tft.setFreeFont(NULL);
  return totalW;
}


static String rainText() {
  if (isnan(precipMm)) return unitKey == "imperial" ? "--.--in" : "--.-mm";

  if (unitKey == "imperial") {
    float inches = precipMm / 25.4f;
    return String(inches, 2) + "in";
  }

  return String(precipMm, 1) + "mm";
}


static String windText() {
  if (isnan(windSpeedMs)) return unitKey == "imperial" ? "--.-mph" : "--.-m/s";

  if (unitKey == "imperial") {
    float mph = windSpeedMs * 2.236936f;
    return String(mph, 1) + "mph";
  }

  return String(windSpeedMs, 1) + "m/s";
}


static String windDirectionText() {
  if (isnan(windDirectionDeg)) return "--";

  const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = (int)roundf(windDirectionDeg / 45.0f) % 8;
  return String(dirs[idx]) + " " + String((int)roundf(windDirectionDeg)) + "deg";
}


// Spec "Weather" > "Data mapping" PSI/UV tables, verbatim. PSI turns
// accent once it clears "Moderate" (101+); UV never does (no accent row).
static String psiLevelWord(float value, bool& isAccent) {
  isAccent = false;
  if (isnan(value)) return "--";
  if (value <= 50.0f) return "Good";
  if (value <= 100.0f) return "Moderate";
  isAccent = true;
  if (value <= 200.0f) return "Unhealthy";
  if (value <= 300.0f) return "Very unhealthy";
  return "Hazardous";
}


// Spec "Bus" > "Data rules": "floor((EstimatedArrival-now)/60)" -- always
// computed fresh from the stored epoch at draw time (never cached as a
// stale pre-converted minute count). Returns -1 for "no bus" (epoch 0).
static int busMinutesUntil(time_t arrivalEpoch) {
  if (arrivalEpoch <= 0) return -1;
  long sec = (long)(arrivalEpoch - time(nullptr));
  if (sec < 0) sec = 0;
  return (int)(sec / 60);
}


// Favourite service (favBusService) if set and present in the active
// stop's fetched list; otherwise the soonest service, so the tile never
// just goes blank before the user has picked a favourite.
static int homeBusServiceIdx() {
  int count = busServiceCountByStop[busActiveStopIdx];
  if (count == 0) return -1;
  if (favBusService.length() > 0) {
    for (int i = 0; i < count; i++) {
      if (busServicesByStop[busActiveStopIdx][i].serviceNo == favBusService) return i;
    }
  }
  return 0; // services are stored sorted by soonest ETA (fetchBusArrival())
}


static String homeBusLabelText() {
  if (busStops[busActiveStopIdx].code.length() == 0) return "BUS";
  int idx = homeBusServiceIdx();
  if (idx < 0) return "BUS";
  return "BUS " + busServicesByStop[busActiveStopIdx][idx].serviceNo;
}


// value/valueColor pair: "Arr" in accent at <=0 min, "~12" in muted when
// not monitored, "-" when there's no bus at all.
static void homeBusValueParts(String& value, uint16_t& valueColor, uint16_t accentColor, uint16_t inkColor, uint16_t mutedColor) {
  if (busStops[busActiveStopIdx].code.length() == 0 || !busDataValidByStop[busActiveStopIdx]) {
    value = "--"; valueColor = mutedColor; return;
  }
  int idx = homeBusServiceIdx();
  if (idx < 0) { value = "-"; valueColor = mutedColor; return; }

  const BusServiceInfo& svc = busServicesByStop[busActiveStopIdx][idx];
  int eta1 = busMinutesUntil(svc.arrival1);
  if (!svc.monitored) {
    value = "~" + String(eta1 < 0 ? 0 : eta1);
    valueColor = mutedColor;
    return;
  }
  if (eta1 <= 0) { value = "Arr"; valueColor = accentColor; return; }
  value = String(eta1) + " min";
  valueColor = inkColor;
}


static String homeBusSubText() {
  if (busStops[busActiveStopIdx].code.length() == 0) return "set on bus page";
  int idx = homeBusServiceIdx();
  if (idx < 0) return "no bus";
  int eta2 = busMinutesUntil(busServicesByStop[busActiveStopIdx][idx].arrival2);
  if (eta2 < 0) return "no bus";
  return "next " + String(eta2) + " min";
}


static String uvLevelWord(float value) {
  if (isnan(value)) return "--";
  if (value < 3.0f) return "Low";
  if (value < 6.0f) return "Moderate";
  if (value < 8.0f) return "High";
  if (value < 11.0f) return "Very high";
  return "Extreme";
}


// Spec "Weather" > "Data mapping" WMO table, verbatim.
static String weatherConditionText(int code) {
  switch (code) {
    case 0: return "Clear";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55:
    case 56:
    case 57: return "Drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65:
    case 66:
    case 67: return "Heavy rain";
    case 80: return "Showers";
    case 81: return "Heavy showers";
    case 82: return "Violent showers";
    case 95:
    case 96:
    case 99: return "Thunderstorm";
    default: return "Cloudy";
  }
}


static String uptimeText() {
  unsigned long seconds = millis() / 1000UL;
  unsigned long days = seconds / 86400UL;
  seconds %= 86400UL;
  unsigned long hours = seconds / 3600UL;
  seconds %= 3600UL;
  unsigned long minutes = seconds / 60UL;

  if (days > 0) return String(days) + "d " + String(hours) + "h";
  if (hours > 0) return String(hours) + "h " + String(minutes) + "m";
  return String(minutes) + "m";
}


static String formatTimerClock(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
  return String(buf);
}


// Spec "Timer states": idle "tap to start", running "tap to pause", paused
// "paused · tap for options" (reopens the picker with Resume/Stop above
// the presets -- see CONTEXT.md's "timer picker" entry; this replaces the
// old long-press-to-reset wording, since long-press no longer does
// anything timer-specific). Done has its own takeover screen, not a tile.
static String focusSubText() {
  switch (focusTimerState) {
    case TIMER_RUNNING: return "tap to pause";
    case TIMER_PAUSED:  return String("paused ") + (char)0xC2 + (char)0xB7 + " tap for options";
    default:            return "tap to start";
  }
}




static String weekNumberText() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &tmNow);
  return String(buf);
}


static String homeTitleText() {
  return deviceNickname.length() > 0 ? deviceNickname : "MakerPulse";
}


void setBacklight(int value) {
  value = constrain(value, 0, 255);
  analogWrite(BACKLIGHT_PIN, value);
}


void wakeDisplay(bool clearManualMode) {
  sleepDimmed = false;
  sleepOff = false;
  if (clearManualMode) manualDimMode = false;
  lastInteractionMs = millis();
  setBacklight(brightnessPwm());
  pageDirty = true;
}


void enterSleepDim() {
  if (sleepDimmed || sleepOff || manualDimMode) return;
  sleepDimmed = true;
  setBacklight(BL_DIM);
}


void enterSleepOff() {
  if (sleepOff) return;
  sleepOff = true;
  sleepDimmed = true;
  setBacklight(BL_OFF);
  pageDirty = true;
}


void toggleSleepMode() {
  if (manualDimMode) {
    wakeDisplay();
    return;
  }

  manualDimMode = true;
  sleepDimmed = true;
  sleepOff = false;
  setBacklight(BL_DIM);
  pageDirty = true;
}


void handleAutoSleep() {
  if (focusTimerState == TIMER_DONE) return;
  if (sleepIntervalMin <= 0) return;

  unsigned long now = millis();
  unsigned long dimAfterMs = (unsigned long)sleepIntervalMin * 60UL * 1000UL;
  unsigned long offAfterMs = dimAfterMs + ((unsigned long)sleepOffDelaySec * 1000UL);

  if (!sleepDimmed && !sleepOff && now - lastInteractionMs > dimAfterMs) {
    enterSleepDim();
  }

  if (sleepDimmed && !sleepOff && now - lastInteractionMs > offAfterMs) {
    enterSleepOff();
  }
}


// =========================================================
// TOUCH
// =========================================================
bool readTouchXY(int& sx, int& sy) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  if (p.z < 80 || p.z > 4000) return false;

  int x = map(p.x, touchCalXMin, touchCalXMax, 0, SCREEN_W);
  int y = map(p.y, touchCalYMin, touchCalYMax, 0, SCREEN_H);

  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);

  if (TOUCH_SWAP_XY) { int tmp = x; x = y; y = tmp; }
  if (touchFlipX)    x = (SCREEN_W - 1) - x;
  if (touchFlipY)    y = (SCREEN_H - 1) - y;

  sx = x;
  sy = y;
  return true;
}


bool touchNewPress(int& tx, int& ty) {
  static bool wasDown = false;
  static unsigned long lastPressMs = 0;

  bool down = false;
  int x = 0, y = 0;

  if (readTouchXY(x, y)) down = true;

  bool fire = false;
  unsigned long now = millis();

  if (down && !wasDown && (now - lastPressMs > 220)) {
    fire = true;
    lastPressMs = now;
    tx = x;
    ty = y;
  }

  wasDown = down;
  return fire;
}


// Spec "Touch and gestures": a tap is <=400ms and moves <10px; a long-press
// fires once, exactly at the 600ms threshold, without waiting for release
// (so the UI can give instant feedback). Anything else -- held past 400ms
// without reaching 600ms, or moved too far -- resolves to no event at all,
// same as a drag/swipe being ignored.
bool pollTouchEvent(TouchEvt& evt, bool& justPressed) {
  static bool wasDown = false;
  static unsigned long pressStartMs = 0;
  static int startX = 0, startY = 0;
  static int maxMoveSq = 0;
  static bool longPressFired = false;

  evt.type = TOUCH_NONE;
  justPressed = false;

  int x = 0, y = 0;
  bool down = readTouchXY(x, y);
  unsigned long now = millis();

  if (down && !wasDown) {
    pressStartMs = now;
    startX = x;
    startY = y;
    maxMoveSq = 0;
    longPressFired = false;
    justPressed = true;
  } else if (down && wasDown) {
    int dx = x - startX, dy = y - startY;
    int distSq = dx * dx + dy * dy;
    if (distSq > maxMoveSq) maxMoveSq = distSq;

    // Long-press is checked against the finger's CURRENT distance from
    // the anchor, not maxMoveSq -- a resistive touchscreen's
    // initial-contact noise typically settles within the first ~100ms,
    // long before the 600ms threshold, so one noisy sample early in the
    // hold no longer permanently disqualifies an otherwise-steady press
    // the way tracking the worst-case drift over the whole hold did.
    // (Root-caused from a user report that long-press "does not work" --
    // see PROJECT_LOG.md.) Tap detection below still uses maxMoveSq: it
    // has no "current position" to check at release time, so it needs
    // the accumulated drift.
    bool currentlyNearAnchor = distSq < (V2_LONGPRESS_MAX_MOVE_PX * V2_LONGPRESS_MAX_MOVE_PX);
    if (!longPressFired && currentlyNearAnchor && (now - pressStartMs) >= (unsigned long)V2_LONGPRESS_MS) {
      longPressFired = true;
      evt.type = TOUCH_LONG_PRESS;
      evt.x = startX;
      evt.y = startY;
    }
  } else if (!down && wasDown) {
    unsigned long heldMs = now - pressStartMs;
    bool withinTapMove = maxMoveSq < (V2_TAP_MAX_MOVE_PX * V2_TAP_MAX_MOVE_PX);
    if (!longPressFired && withinTapMove && heldMs <= (unsigned long)V2_TAP_MAX_MS) {
      evt.type = TOUCH_TAP;
      evt.x = startX;
      evt.y = startY;
    }
  }

  wasDown = down;
  return evt.type != TOUCH_NONE;
}


unsigned long touchHoldMsInRect(int rx, int ry, int rw, int rh) {
  static bool holding = false;
  static int anchorX = 0, anchorY = 0;
  static unsigned long holdStartMs = 0;

  int x = 0, y = 0;
  bool inRect = readTouchXY(x, y) && x >= rx && x < rx + rw && y >= ry && y < ry + rh;
  if (!inRect) {
    holding = false; // released, or moved out of the rect -- next entry starts fresh
    return 0;
  }

  unsigned long now = millis();
  if (!holding) {
    holding = true;
    anchorX = x;
    anchorY = y;
    holdStartMs = now;
    return 0;
  }
  int dx = x - anchorX, dy = y - anchorY;
  if (dx * dx + dy * dy >= (V2_LONGPRESS_MAX_MOVE_PX * V2_LONGPRESS_MAX_MOVE_PX)) {
    // Moved too far to still be "the same hold" -- restart the anchor
    // rather than cancel outright, so a slightly wobbly finger still works.
    anchorX = x;
    anchorY = y;
    holdStartMs = now;
    return 0;
  }
  return now - holdStartMs;
}


// Spec "Touch and gestures" > "Calibration": a two-point crosshair
// routine. Reads raw ts.getPoint() values directly -- readTouchXY()
// already applies the calibration this is in the middle of replacing.
// Solves the linear map(raw, calMin, calMax, 0, screenSize) equation
// from two (raw, target) samples; touchFlipX/Y are mirrors applied
// *after* that map in readTouchXY(), so they're undone here (mirrors are
// their own inverse) to get the pre-flip target each raw sample must
// resolve to, independent of whatever orientation is currently set.
static int touchCalStep = 0; // 0 = idle, 1 = waiting for point A, 2 = waiting for point B
static int touchCalRawAx = 0, touchCalRawAy = 0;
static bool touchCalWasDown = false;

static void solveLinearCal(int rawA, int targetA, int rawB, int targetB, int screenSize, int& calMin, int& calMax) {
  if (rawB == rawA) { calMin = rawA - screenSize / 2; calMax = rawA + screenSize / 2; return; }
  float k = (float)(targetB - targetA) / (float)(rawB - rawA);
  float calMinF = rawA - (targetA / k);
  calMin = (int)roundf(calMinF);
  calMax = (int)roundf(calMinF + (screenSize / k));
}


static void drawCalibrationCrosshair(int x, int y, const char* label) {
  tft.fillScreen(COL_BG);
  tft.drawFastHLine(x - 10, y, 20, COL_ACCENT);
  tft.drawFastVLine(x, y - 10, 20, COL_ACCENT);
  tft.drawCircle(x, y, 6, COL_ACCENT);
  tft.setFreeFont(&F_BODY_REG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(label, SCREEN_W / 2, 150);
  tft.setFreeFont(NULL);
  tft.setTextDatum(TL_DATUM);
}


bool touchCalibrationActive() { return touchCalStep != 0; }


void startTouchCalibration() {
  touchCalStep = 1;
  touchCalWasDown = false;
  drawCalibrationCrosshair(CAL_PT_A_X, CAL_PT_A_Y, "Tap the crosshair");
}


void updateTouchCalibration() {
  TS_Point p;
  bool down = ts.touched();
  if (down) p = ts.getPoint();
  bool validTouch = down && p.z >= 80 && p.z <= 4000;

  if (validTouch && !touchCalWasDown) {
    if (touchCalStep == 1) {
      touchCalRawAx = p.x;
      touchCalRawAy = p.y;
      touchCalStep = 2;
      drawCalibrationCrosshair(CAL_PT_B_X, CAL_PT_B_Y, "Tap the crosshair");
    } else if (touchCalStep == 2) {
      int targetAx = touchFlipX ? (SCREEN_W - 1 - CAL_PT_A_X) : CAL_PT_A_X;
      int targetBx = touchFlipX ? (SCREEN_W - 1 - CAL_PT_B_X) : CAL_PT_B_X;
      int targetAy = touchFlipY ? (SCREEN_H - 1 - CAL_PT_A_Y) : CAL_PT_A_Y;
      int targetBy = touchFlipY ? (SCREEN_H - 1 - CAL_PT_B_Y) : CAL_PT_B_Y;

      int newXMin, newXMax, newYMin, newYMax;
      solveLinearCal(touchCalRawAx, targetAx, p.x, targetBx, SCREEN_W, newXMin, newXMax);
      solveLinearCal(touchCalRawAy, targetAy, p.y, targetBy, SCREEN_H, newYMin, newYMax);

      saveTouchCalibration(newXMin, newXMax, newYMin, newYMax);

      touchCalStep = 0;
      pageDirty = true;
    }
  }
  touchCalWasDown = validTouch;
}


// =========================================================
// DRAW HELPERS
// =========================================================
// Spec "Components" > "Icon": "drawn from circles, lines and triangles on
// a 16-unit grid, scaled to 16 or 40 px." scale=1.0 is the original
// ~16-18px drawing; the Weather page's 40px hero icon passes scale~2.2.
void drawCleanSunIcon(TFT_eSPI& spr, int cx, int cy, uint16_t c, float scale = 1.0f) {
  int r = (int)(4 * scale + 0.5f);
  int near = (int)(7 * scale + 0.5f);
  int far = (int)(9 * scale + 0.5f);
  int diagNear = (int)(5 * scale + 0.5f);
  int diagFar = (int)(6 * scale + 0.5f);
  spr.fillCircle(cx, cy, r, c);
  spr.drawLine(cx, cy - far, cx, cy - near, c);
  spr.drawLine(cx, cy + near, cx, cy + far, c);
  spr.drawLine(cx - far, cy, cx - near, cy, c);
  spr.drawLine(cx + near, cy, cx + far, cy, c);
  spr.drawLine(cx - diagFar, cy - diagFar, cx - diagNear, cy - diagNear, c);
  spr.drawLine(cx + diagNear, cy - diagNear, cx + diagFar, cy - diagFar, c);
  spr.drawLine(cx - diagFar, cy + diagFar, cx - diagNear, cy + diagNear, c);
  spr.drawLine(cx + diagNear, cy + diagNear, cx + diagFar, cy + diagFar, c);
}


void drawMoonIcon(TFT_eSPI& spr, int cx, int cy, uint16_t c, uint16_t bg = COL_PANEL, float scale = 1.0f) {
  int r = (int)(6 * scale + 0.5f);
  int bite = (int)(4 * scale + 0.5f);
  int biteY = (int)(2 * scale + 0.5f);
  spr.fillCircle(cx, cy, r, c);
  spr.fillCircle(cx + bite, cy - biteY, r, bg);
}


void drawCloudGlyph(TFT_eSPI& spr, int cx, int cy, uint16_t c, float scale = 1.0f) {
  spr.fillCircle(cx - (int)(7 * scale), cy + (int)(2 * scale), (int)(7 * scale + 0.5f), c);
  spr.fillCircle(cx + (int)(2 * scale), cy - (int)(4 * scale), (int)(9 * scale + 0.5f), c);
  spr.fillCircle(cx + (int)(10 * scale), cy + (int)(2 * scale), (int)(6 * scale + 0.5f), c);
  spr.fillRoundRect(cx - (int)(12 * scale), cy, (int)(30 * scale), (int)(10 * scale), (int)(5 * scale + 0.5f), c);
}


// Spec icon kinds: clear (sun/moon), partly, cloud, rain, storm -- the WMO
// mapping table's own "anything else -> cloud" fallback already collapses
// fog/snow into this same set, so there's no separate fog/snow glyph.
void drawConditionIcon(TFT_eSPI& spr, int cx, int cy, int code, bool isDay, uint16_t accent, uint16_t dim, uint16_t bg = COL_PANEL, float scale = 1.0f) {
  bool clearOrMainlyClear = (code == 0 || code == 1);
  bool thunder = (code == 95 || code == 96 || code == 99);
  bool rain = (code >= 61 && code <= 67) || (code >= 51 && code <= 57) || (code >= 80 && code <= 82);

  if (clearOrMainlyClear) {
    if (isDay) drawCleanSunIcon(spr, cx, cy, accent, scale);
    else drawMoonIcon(spr, cx, cy, accent, bg, scale);
    if (code == 1) spr.fillCircle(cx + (int)(9 * scale), cy + (int)(9 * scale), (int)(5 * scale + 0.5f), dim);
    return;
  }

  if (thunder) {
    drawCloudGlyph(spr, cx, cy - (int)(4 * scale), dim, scale);
    int x0 = cx - (int)(2 * scale), x1 = cx + (int)(3 * scale), x2 = cx - (int)(1 * scale), x3 = cx + (int)(4 * scale);
    int y0 = cy + (int)(8 * scale), y1 = cy + (int)(14 * scale);
    spr.drawLine(x0, y0, x1, y0, accent);
    spr.drawLine(x1, y0, x2, y1, accent);
    spr.drawLine(x2, y1, x3, y1, accent);
    return;
  }

  if (rain) {
    drawCloudGlyph(spr, cx, cy - (int)(4 * scale), dim, scale);
    for (int i = -1; i <= 1; i++) {
      int lx = cx + (int)(i * 7 * scale);
      spr.drawLine(lx, cy + (int)(9 * scale), lx - (int)(2 * scale), cy + (int)(15 * scale), accent);
    }
    return;
  }

  drawCloudGlyph(spr, cx, cy, dim, scale);
}


// Spec "Timer states" > "Timer done takeover": accent fill, "TIME'S UP" /
// duration / dismiss hint in bg colour, alternating accent/bg 3 times at
// 500 ms before settling on the accent fill. Shows on top of any page
// (drawCurrentPageFull()/updateCurrentPageDynamic() check focusTimerState
// before touching the per-page drawer) and the timer keeps running
// elsewhere since this is pure presentation over state.cpp's own machine.
void drawTimerDoneOverlay(bool force) {
  if (focusTimerState != TIMER_DONE) return;

  unsigned long elapsed = millis() - focusDoneStartedMs;
  unsigned long flashWindowMs = (unsigned long)TIMER_DONE_FLASH_COUNT * 2UL * TIMER_DONE_FLASH_MS;
  bool flashing = elapsed < flashWindowMs;
  bool onAccent = !flashing || ((elapsed / TIMER_DONE_FLASH_MS) % 2UL == 0);

  uint16_t backdrop = onAccent ? COL_ACCENT : COL_BG;
  uint16_t ink = onAccent ? COL_BG : COL_ACCENT;

  String durationText = formatTimerClock((unsigned long)focusDurationMin * 60UL);
  String combined = durationText + "|" + String(backdrop);
  if (!force && combined == cacheTimerDone) return;
  cacheTimerDone = combined;

  tft.fillScreen(backdrop);

  drawSmallCapsLabel("TIME'S UP", SCREEN_W / 2, 112, TC_DATUM, ink, backdrop);

  tft.setFreeFont(&F_DISPLAY);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ink, backdrop);
  tft.drawString(durationText, SCREEN_W / 2, 130);
  tft.setFreeFont(NULL);

  tft.setFreeFont(&F_BODY_REG);
  tft.drawString("Tap anywhere to dismiss", SCREEN_W / 2, 200);
  tft.setFreeFont(NULL);
  tft.setTextDatum(TL_DATUM);
}


// Timer picker -- CONTEXT.md's "timer picker" entry. A flat, full-screen
// overlay (no boxes/borders, hairlines between rows) matching the rest of
// V2.1.0's "no boxes" visual language. Idle/paused taps on the Home Timer
// tile open this (handleTimerTileTap() in state.cpp); while paused, two
// extra rows ("Resume"/"Stop") are prepended above the 6 presets +
// Custom. Row order here must exactly match handleTimerPickerTouch()'s
// hit-testing below -- timerPickerPresetStartIdx() is the single source
// of truth both share.
static const int TIMER_PICKER_ROWS_Y0 = 40;
static const int TIMER_PICKER_ROW_H = 34;

static int timerPickerPresetStartIdx() {
  return (focusTimerState == TIMER_PAUSED) ? 2 : 0;
}

static int timerPickerRowCount() {
  return timerPickerPresetStartIdx() + FOCUS_PRESET_COUNT + 1; // presets + Custom
}

static String timerPickerRowLabel(int idx, uint16_t& color) {
  int presetStart = timerPickerPresetStartIdx();
  color = COL_TEXT;
  if (focusTimerState == TIMER_PAUSED && idx == 0) { color = COL_ACCENT; return "Resume"; }
  if (focusTimerState == TIMER_PAUSED && idx == 1) { return "Stop"; }
  if (idx < presetStart + FOCUS_PRESET_COUNT) {
    return String(timerPresetMin[idx - presetStart]) + " min";
  }
  return "Custom";
}

void drawTimerPickerOverlay(bool force) {
  if (!timerPickerOpen) return;

  int rowCount = timerPickerRowCount();
  String combined = String(rowCount) + "|" + String(focusTimerState) + "|" + String(COL_BG);
  if (!force && combined == cacheTimerPicker) return;
  cacheTimerPicker = combined;

  tft.fillScreen(COL_BG);
  drawSmallCapsLabel("TIMER", SCREEN_W / 2, 16, TC_DATUM, COL_DIM, COL_BG);
  tft.drawFastHLine(V2_MARGIN_L, TIMER_PICKER_ROWS_Y0 - 6, V2_CONTENT_W, COL_STROKE);

  tft.setFreeFont(&F_BODY_REG);
  tft.setTextDatum(TL_DATUM);
  int fh = tft.fontHeight();
  for (int i = 0; i < rowCount; i++) {
    int y = TIMER_PICKER_ROWS_Y0 + i * TIMER_PICKER_ROW_H;
    uint16_t color;
    String label = timerPickerRowLabel(i, color);
    tft.setTextColor(color, COL_BG);
    tft.drawString(label, V2_MARGIN_L, y + (TIMER_PICKER_ROW_H - fh) / 2);
    if (i < rowCount - 1) {
      tft.drawFastHLine(V2_MARGIN_L, y + TIMER_PICKER_ROW_H - 1, V2_CONTENT_W, COL_STROKE);
    }
  }
  tft.setFreeFont(NULL);
}


// Custom-entry stepper -- CONTEXT.md's "timer picker" entry. Replicates
// the old pre-V2.1.0 MakerPulse handleCustomTimerEntryTouch()'s
// Cancel-vs-tap-outside distinction: the explicit Cancel button returns
// to the picker (one level up), while tapping outside this panel's band
// (above TIMER_CUSTOM_PANEL_Y0 or below TIMER_CUSTOM_PANEL_Y1) closes
// everything, landing back on whatever state was already live underneath
// (idle, or still paused if reopened from a paused session) rather than
// forcing idle -- cancelling a paused session via a stray outside-tap on
// a sub-screen would be more destructive than anyone asked for.
static const int TIMER_CUSTOM_PANEL_Y0 = 70;
static const int TIMER_CUSTOM_PANEL_Y1 = 250;
static const int TIMER_CUSTOM_STEP_Y0 = 160;
static const int TIMER_CUSTOM_STEP_Y1 = 200;
static const int TIMER_CUSTOM_ACTIONS_Y0 = 224;
static const int TIMER_CUSTOM_ACTIONS_Y1 = 250;
static const char* TIMER_CUSTOM_STEP_LABELS[4] = {"-5", "-1", "+1", "+5"};
static const int TIMER_CUSTOM_STEP_DELTAS[4] = {-5, -1, 1, 5};

void drawCustomTimerStepperOverlay(bool force) {
  if (!timerCustomEntryOpen) return;

  String combined = String(customTimerMinutes);
  if (!force && combined == cacheCustomStepper) return;
  cacheCustomStepper = combined;

  tft.fillScreen(COL_BG);
  drawSmallCapsLabel("CUSTOM TIMER", SCREEN_W / 2, 78, TC_DATUM, COL_DIM, COL_BG);
  tft.drawFastHLine(V2_MARGIN_L, 96, V2_CONTENT_W, COL_STROKE);

  tft.setFreeFont(&F_DISPLAY);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(String(customTimerMinutes) + " min", SCREEN_W / 2, 104);
  tft.setFreeFont(NULL);

  int stepW = V2_CONTENT_W / 4;
  tft.setFreeFont(&F_TITLE);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_ACCENT, COL_BG);
  for (int i = 0; i < 4; i++) {
    int cx = V2_MARGIN_L + stepW * i + stepW / 2;
    tft.drawString(TIMER_CUSTOM_STEP_LABELS[i], cx, TIMER_CUSTOM_STEP_Y0 + 14);
  }
  tft.setFreeFont(NULL);

  tft.drawFastHLine(V2_MARGIN_L, 214, V2_CONTENT_W, COL_STROKE);

  tft.setFreeFont(&F_BODY_BOLD);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString("Cancel", V2_MARGIN_L, TIMER_CUSTOM_ACTIONS_Y0);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.drawString("Start", V2_MARGIN_R, TIMER_CUSTOM_ACTIONS_Y0);
  tft.setFreeFont(NULL);
  tft.setTextDatum(TL_DATUM);
}


// Spec "Components"/Tab bar: labels F_BODY centred in 54px cells at y290
// (from x V2_MARGIN_L, so 4*54=216=V2_CONTENT_W -- the label row is inset
// to the content margins); active = Bold ink + 2px underline at y309 exactly
// the label's width; inactive = Regular muted. The *touch* strip is wider
// (full SCREEN_W/4 columns, y>=V2_TAB_TOUCH_Y0) per "Touch and gestures"'s
// "tab cells are 60x44 (full width...)" -- see handleNavTouch(), unchanged.
// "Weather" at the spec's own 13px F_BODY size still measures right at
// ~54px -- a fixed 54px-pitch grid (one label centred per cell) has no
// margin for that and visibly collides with its neighbours. Laid out with
// each label's own real width plus an explicit gap instead (same
// technique as the Status page's segmented options), then the whole group
// centred in the content column, so it can never overlap regardless of
// which label is bold/active.
static void drawV2TabBar() {
  const char* names[4] = {"Home", "Weather", "Bus", "Status"};
  const int gap = 10;

  tft.fillRect(0, V2_TABBAR_Y0, SCREEN_W, SCREEN_H - V2_TABBAR_Y0, COL_BG);

  int widths[4];
  int totalW = 0;
  for (int i = 0; i < 4; i++) {
    bool active = ((int)currentPage == i);
    tft.setFreeFont(active ? &F_BODY_BOLD : &F_BODY_REG);
    widths[i] = tft.textWidth(names[i]);
    tft.setFreeFont(NULL);
    totalW += widths[i];
    if (i > 0) totalW += gap;
  }

  tft.setTextDatum(TL_DATUM);
  int cx = V2_MARGIN_L + (V2_CONTENT_W - totalW) / 2;
  for (int i = 0; i < 4; i++) {
    bool active = ((int)currentPage == i);
    tft.setFreeFont(active ? &F_BODY_BOLD : &F_BODY_REG);
    tft.setTextColor(active ? COL_TEXT : COL_DIM, COL_BG);
    tft.drawString(names[i], cx, 290);
    tft.setFreeFont(NULL);

    if (active) {
      tft.drawFastHLine(cx, 309, widths[i], COL_TEXT);
      tft.drawFastHLine(cx, 310, widths[i], COL_TEXT);
    }
    cx += widths[i] + gap;
  }
}


// Weather/Bus/Status's simple page header (title + hairline divider) --
// Home has no equivalent since its clock already fills that role.
// 24h, no leading zero -- spec's own clock example ("7:42", "19:05") and
// the toggle target of the clock long-press. Independent of regionFormatKey
// (date format), a separate pre-existing setting.
static String homeClockText(const struct tm& tmValue) {
  int hour24 = tmValue.tm_hour;
  int minute = tmValue.tm_min;
  char buf[12];
  if (clockIs24h) {
    snprintf(buf, sizeof(buf), "%d:%02d", hour24, minute);
  } else {
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
  }
  return String(buf);
}


// Home's own sun-time format -- always 24h, no leading zero, matching the
// spec's literal "Sunrise 6:59 · Sunset 19:05" example regardless of the
// clock's 12h/24h toggle (that toggle is scoped to the clock element only,
// per "Interactions").
static String homeMinuteOfDayText(int minOfDay) {
  if (minOfDay < 0) return "--:--";
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", minOfDay / 60, minOfDay % 60);
  return String(buf);
}


// Spec "Home" > "Layout": clock, TUE/17/NOV calendar stack, sun line + W47,
// hairline at y100. No bold F_SMALL asset exists (Type scale only
// generated one F_SMALL weight) -- the month's "caps Bold ink" is drawn at
// ink colour in the same (non-bold) F_SMALL weight, the closest available
// approximation; flagged in PROJECT_LOG.
static void drawHomeHeader(bool force) {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  // Spec "Hardening" > "Edge cases": "NTP not yet synced (show '--:--')."
  // epoch values before ~2023 mean NTP hasn't landed yet (same sanity
  // threshold waitForNtpTime() uses at boot) -- the whole header shows
  // placeholders rather than a misleading 1970 date.
  bool synced = timeIsSynced();

  String timeBuf = synced ? homeClockText(tmNow) : "--:--";
  String dayName, monthName, weekLine;
  String dayNum = synced ? String(tmNow.tm_mday) : "--";
  if (synced) {
    char dayNameBuf[8];
    strftime(dayNameBuf, sizeof(dayNameBuf), "%a", &tmNow);
    dayName = String(dayNameBuf);
    char monthBuf[8];
    strftime(monthBuf, sizeof(monthBuf), "%b", &tmNow);
    monthName = String(monthBuf);
    weekLine = "W" + weekNumberText();
  } else {
    dayName = "---";
    monthName = "---";
    weekLine = "W--";
  }

  String sunLine = "Sunrise " + homeMinuteOfDayText(sunriseMin) + " " +
                    (char)0xC2 + (char)0xB7 + " Sunset " + homeMinuteOfDayText(sunsetMin);

  String combined = timeBuf + "|" + dayName + "|" + dayNum + "|" + monthName + "|" +
                     sunLine + "|" + weekLine + "|" +
                     String(COL_BG) + "|" + String(COL_TEXT) + "|" + String(COL_DIM) + "|" + String(COL_STROKE);
  if (!force && combined == cacheClock) return;
  cacheClock = combined;

  tft.fillRect(0, 0, SCREEN_W, 100, COL_BG);

  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&F_DISPLAY);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(timeBuf, V2_MARGIN_L, 10);
  tft.setFreeFont(NULL);

  drawSmallCapsLabel(dayName, V2_MARGIN_R, 14, TR_DATUM, COL_DIM, COL_BG);

  tft.setFreeFont(&F_TITLE);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(dayNum, V2_MARGIN_R, 30);
  tft.setFreeFont(NULL);
  tft.setTextDatum(TL_DATUM);

  drawSmallCapsLabel(monthName, V2_MARGIN_R, 56, TR_DATUM, COL_TEXT, COL_BG);

  tft.setFreeFont(&F_BODY_REG);
  drawTextWithDegree(V2_MARGIN_L, 76, sunLine, COL_DIM, COL_BG);
  tft.setFreeFont(NULL);

  drawSmallCapsLabel(weekLine, V2_MARGIN_R, 78, TR_DATUM, COL_DIM, COL_BG);

  tft.drawFastHLine(V2_MARGIN_L, 100, V2_CONTENT_W, COL_STROKE);
}


// Spec "Home" > "Layout"/"Components": Tile (104x64) -- label F_SMALL caps
// muted at y+0, value F_NUM at y+16 (ink unless valueColor overrides it,
// e.g. accent for "Arr"), sub-line F_SMALL muted (plain case, not tracked)
// at y+52.
static void drawHomeTileValue(int x, int y, const String& value, uint16_t valueColor) {
  tft.setFreeFont(&F_NUM);
  drawTextWithDegree(x, y, value, valueColor, COL_BG);
  tft.setFreeFont(NULL);
}


static void drawHomeTileSub(int x, int y, const String& sub) {
  if (sub.length() == 0) return;
  tft.setFreeFont(&F_SMALL);
  drawTextWithDegree(x, y, sub, COL_DIM, COL_BG);
  tft.setFreeFont(NULL);
}


// Bus tile value gets its own drawer: a numeric ETA has an F_BODY Bold
// "min" unit baseline-aligned 3px after it (spec "Components" > "Tile");
// "Arr"/"~3"/"-" have no numeric-prefix split and are drawn as one token.
static void drawHomeBusValue(int x, int y, const String& value, uint16_t valueColor) {
  String numPart, restPart;
  splitNumericPrefix(value, numPart, restPart);

  int cx = x;
  tft.setTextDatum(TL_DATUM);
  if (numPart.length() > 0) {
    tft.setFreeFont(&F_NUM);
    tft.setTextColor(valueColor, COL_BG);
    tft.drawString(numPart, cx, y);
    cx += tft.textWidth(numPart);
    tft.setFreeFont(NULL);
    if (restPart.length() > 0) {
      tft.setFreeFont(&F_BODY_BOLD);
      tft.setTextColor(valueColor, COL_BG);
      tft.drawString(restPart, cx + 3, y + 10);
      tft.setFreeFont(NULL);
    }
  } else {
    tft.setFreeFont(&F_NUM);
    drawTextWithDegree(cx, y, restPart, valueColor, COL_BG);
    tft.setFreeFont(NULL);
  }
}


static void drawHomeBusTile(bool force) {
  int x, y, w, h;
  getHomeTileRect(0, x, y, w, h);

  String label = homeBusLabelText();
  String value; uint16_t valueColor;
  homeBusValueParts(value, valueColor, COL_ACCENT, COL_TEXT, COL_DIM);
  String sub = homeBusSubText();

  String combined = label + "|" + value + "|" + String(valueColor) + "|" + sub + "|" + String(COL_BG);
  if (!force && combined == cacheHomeTiles[0]) return;
  cacheHomeTiles[0] = combined;

  tft.fillRect(x, y, w, h, COL_BG);
  drawSmallCapsLabel(label, x, y, TL_DATUM, COL_DIM, COL_BG);
  drawHomeBusValue(x, y + 16, value, valueColor);
  drawHomeTileSub(x, y + 52, sub);
}


static void drawHomeOutdoorTile(bool force) {
  int x, y, w, h;
  getHomeTileRect(1, x, y, w, h);

  String value = formatDegreeOnly(tempC);
  String sub = tempRangeDegreeText();

  String combined = value + "|" + sub + "|" + String(COL_BG) + "|" + String(COL_TEXT);
  if (!force && combined == cacheHomeTiles[1]) return;
  cacheHomeTiles[1] = combined;

  tft.fillRect(x, y, w, h, COL_BG);
  drawSmallCapsLabel("Outdoor", x, y, TL_DATUM, COL_DIM, COL_BG);
  drawHomeTileValue(x, y + 16, value, COL_TEXT);
  drawHomeTileSub(x, y + 52, sub);
}


static void drawHomeRainTile(bool force) {
  int x, y, w, h;
  getHomeTileRect(2, x, y, w, h);

  String value = rainChanceText();
  String sub = rainIntensityWord();

  String combined = value + "|" + sub + "|" + String(COL_BG) + "|" + String(COL_TEXT);
  if (!force && combined == cacheHomeTiles[2]) return;
  cacheHomeTiles[2] = combined;

  tft.fillRect(x, y, w, h, COL_BG);
  drawSmallCapsLabel("Rain", x, y, TL_DATUM, COL_DIM, COL_BG);
  drawHomeTileValue(x, y + 16, value, COL_TEXT);
  drawHomeTileSub(x, y + 52, sub);
}


static void drawHomeTimerTile(bool force) {
  int x, y, w, h;
  getHomeTileRect(3, x, y, w, h);

  String value = formatTimerClock(focusRemainingSec);
  String sub = focusSubText();
  bool accented = (focusTimerState == TIMER_RUNNING);
  uint16_t valueColor = (focusTimerState == TIMER_PAUSED) ? COL_DIM : (accented ? COL_ACCENT : COL_TEXT);

  String combined = value + "|" + sub + "|" + String(valueColor) + "|" + String(accented) + "|" + String(COL_BG);
  if (!force && combined == cacheHomeTiles[3]) return;
  cacheHomeTiles[3] = combined;

  tft.fillRect(x, y, w, h, COL_BG);
  drawSmallCapsLabel("Timer", x, y, TL_DATUM, COL_DIM, COL_BG);
  drawHomeTileValue(x, y + 16, value, valueColor);
  if (sub.length() > 0) {
    tft.setFreeFont(&F_SMALL);
    drawTextWithDegree(x, y + 52, sub, accented ? COL_ACCENT : COL_DIM, COL_BG);
    tft.setFreeFont(NULL);
  }
}


// Spec "Home" > "Layout": "Status whisper | hidden normally; shows
// '● Offline · showing data from 10:21' when any feed is stale." A feed
// counts as stale once its own interval has elapsed with no success --
// reusing each FetchBackoff's failCount>0 as the "stale" signal keeps this
// in sync with the same retry bookkeeping network.cpp already updates.
// Spec "Staleness and offline": "Stale = now - lastOkEpoch > 'stale
// after'" -- weather/PSI use 2x their own refresh interval (so changing
// WEATHER_INTERVAL_SEC/PSI_INTERVAL_SEC keeps this in sync automatically);
// bus uses the flat BUS_STALE_SEC, matching the Status Synced row's own
// staleness test.
static bool anyFeedStale(time_t& oldestFetch) {
  bool stale = false;
  time_t now = time(nullptr);
  oldestFetch = now;

  if (lastWeatherFetch > 0 && (now - lastWeatherFetch) > (time_t)(2 * WEATHER_INTERVAL_SEC)) {
    stale = true;
    oldestFetch = min(oldestFetch, lastWeatherFetch);
  }
  if (busStops[busActiveStopIdx].code.length() > 0 && lastBusFetchByStop[busActiveStopIdx] > 0 &&
      (now - lastBusFetchByStop[busActiveStopIdx]) > (time_t)BUS_STALE_SEC) {
    stale = true;
    oldestFetch = min(oldestFetch, lastBusFetchByStop[busActiveStopIdx]);
  }
  return stale;
}


static void drawHomeOfflineWhisper(bool force) {
  time_t oldest;
  bool stale = anyFeedStale(oldest);

  String text = "";
  if (stale) {
    struct tm tmOldest;
    localtime_r(&oldest, &tmOldest);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d:%02d", tmOldest.tm_hour, tmOldest.tm_min);
    text = String("showing data from ") + buf;
  }

  String combined = text + "|" + String(COL_ACCENT) + "|" + String(COL_BG);
  if (!force && combined == cacheHomeEmpty1) return;
  cacheHomeEmpty1 = combined;

  tft.fillRect(V2_MARGIN_L, 258, V2_CONTENT_W, 14, COL_BG);
  if (!stale) return;

  tft.setFreeFont(&F_SMALL);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.setTextDatum(TL_DATUM);
  int fh = tft.fontHeight();
  tft.fillCircle(V2_MARGIN_L + 3, 262 + fh / 2, 3, COL_ACCENT);
  drawTextWithDegree(V2_MARGIN_L + 10, 262, String((char)0xC2) + (char)0xB7 + " " + text, COL_ACCENT, COL_BG);
  tft.setFreeFont(NULL);
}


void drawHomePageFull() {
  tft.fillScreen(COL_BG);

  cacheClock = "";
  clearHomeTileCaches();
  cacheHomeEmpty1 = "";

  pageDirty = false;
  lastDrawnPage = PAGE_HOME;

  drawHomeHeader(true);
  drawHomeBusTile(true);
  drawHomeOutdoorTile(true);
  drawHomeRainTile(true);
  drawHomeTimerTile(true);
  drawHomeOfflineWhisper(true);
  drawV2TabBar();
}


void updateHomeDynamic() {
  drawHomeHeader(false);
  drawHomeBusTile(false);
  drawHomeOutdoorTile(false);
  drawHomeRainTile(false);
  drawHomeTimerTile(false);
  drawHomeOfflineWhisper(false);
}


// Spec "Weather" > "Layout": header (LOCATION_NAME + relative age), hero
// temp/icon/condition/details, hairline, hourly-or-daily strip (toggled by
// tapping the strip), hairline, two stat columns (PSI / UV max).
static String weatherAgeText() {
  if (lastWeatherFetch <= 0) return "--";
  long ageSec = (long)(time(nullptr) - lastWeatherFetch);
  if (ageSec < 0) ageSec = 0;
  if (ageSec < 45) return "just now";
  if (ageSec < 3600) return String((ageSec + 30) / 60) + " min ago";
  return String((ageSec + 1800) / 3600) + " h ago";
}


static String joinWithMiddot(const String& a, const String& b) {
  if (a.length() == 0) return b;
  if (b.length() == 0) return a;
  return a + " " + (char)0xC2 + (char)0xB7 + " " + b;
}


// Spec Details row: "Feels 34° · Hum 78% · 25–31°".
static String weatherDetailsLine() {
  String feels = isnan(apparentTempC) ? "" : "Feels " + formatDegreeOnly(apparentTempC);
  String hum = isnan(humidity) ? "" : "Hum " + String((int)roundf(humidity)) + "%";
  String range = tempRangeDegreeText();
  return joinWithMiddot(joinWithMiddot(feels, hum), range);
}


// Spec "times as 8p, 12a" -- always 12h/lowercase for the hourly strip,
// independent of the region date-format setting (which only ever governed
// the clock/date elsewhere); assumption flagged in PROJECT_LOG.
static String hourLabel12Lower(int hour24) {
  if (hour24 < 0) return "--";
  int h12 = hour24 % 12;
  if (h12 == 0) h12 = 12;
  return String(h12) + (hour24 >= 12 ? "p" : "a");
}


void drawWeatherPageFull() {
  tft.fillScreen(COL_BG);
  drawV2TabBar();

  pageDirty = false;
  lastDrawnPage = PAGE_WEATHER;

  lastWeatherHeaderText = "";
  lastNowText = "";
  lastHourlyText = "";
  lastUvText = "";
  lastPsiText = "";

  tft.drawFastHLine(V2_MARGIN_L, 124, V2_CONTENT_W, COL_STROKE);
  tft.drawFastHLine(V2_MARGIN_L, 224, V2_CONTENT_W, COL_STROKE);
}


static void drawWeatherHeader(bool force) {
  String loc = locationName;
  loc.toUpperCase();
  String label = joinWithMiddot("WEATHER", loc);
  String age = weatherAgeText();

  String combined = label + "|" + age + "|" + String(COL_DIM) + "|" + String(COL_BG);
  if (!force && combined == lastWeatherHeaderText) return;
  lastWeatherHeaderText = combined;

  tft.fillRect(0, 0, SCREEN_W, 26, COL_BG);
  drawSmallCapsLabel(label, V2_MARGIN_L, 12, TL_DATUM, COL_DIM, COL_BG);

  tft.setFreeFont(&F_SMALL);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(age, V2_MARGIN_R, 12);
  tft.setFreeFont(NULL);
  tft.setTextDatum(TL_DATUM);
}


static void drawWeatherHero(bool force) {
  String t = formatDegreeOnly(tempC);
  String cond = weatherConditionText(weatherCode);
  String details = weatherDetailsLine();

  String combined = t + "|" + cond + "|" + details + "|" + String(weatherCode) + "|" +
                     String(weatherIsDay ? 1 : 0) + "|" + String(COL_TEXT) + "|" + String(COL_DIM) + "|" + String(COL_BG);
  if (!force && combined == lastNowText) return;
  lastNowText = combined;

  tft.fillRect(0, 26, SCREEN_W, 124 - 26, COL_BG);

  tft.setFreeFont(&F_DISPLAY);
  tft.setTextDatum(TL_DATUM);
  drawTextWithDegree(V2_MARGIN_L, 28, t, COL_TEXT, COL_BG);
  tft.setFreeFont(NULL);

  drawConditionIcon(tft, 188 + 20, 34 + 20, weatherCode, weatherIsDay, COL_TEXT, COL_DIM, COL_BG, 1.3f);

  tft.setFreeFont(&F_BODY_BOLD);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(cond, V2_MARGIN_L, 88);
  tft.setFreeFont(NULL);

  tft.setFreeFont(&F_BODY_REG);
  drawTextWithDegree(V2_MARGIN_L, 104, details, COL_DIM, COL_BG);
  tft.setFreeFont(NULL);
}


// Spec "Hourly column": centre cx = 12 + 36i + 18. Reused at a narrower
// 43px pitch for the 5-day view (same anatomy, day name/max-temp instead
// of time/current-temp, per "Tap the strip: 5-day view").
static void drawWeatherStripColumn(int cx, const String& topLabel, bool topBold, int code, bool isDay,
                                     const String& tempLabel, int chancePercent) {
  tft.setFreeFont(&F_SMALL);
  tft.setTextColor(topBold ? COL_TEXT : COL_DIM, COL_BG);
  int tw = measureTextWithGlyphs(topLabel);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(topLabel, cx - tw / 2, 132);
  tft.setFreeFont(NULL);

  drawConditionIcon(tft, cx, 148 + 8, code, isDay, COL_TEXT, COL_DIM, COL_BG, 0.55f);

  tft.setFreeFont(&F_BODY_BOLD);
  tft.setTextColor(COL_TEXT, COL_BG);
  drawTextWithGlyphsCentered(cx, 168, tempLabel, COL_TEXT, COL_BG);
  tft.setFreeFont(NULL);

  drawRainBar(cx, 186, chancePercent, COL_STROKE, COL_ACCENT, COL_BG);

  tft.setFreeFont(&F_SMALL);
  String pct = chancePercent < 0 ? "--%" : String(chancePercent) + "%";
  drawTextWithGlyphsCentered(cx, 208, pct, COL_DIM, COL_BG);
  tft.setFreeFont(NULL);
}


static String dailyDayName(int ymd) {
  if (ymd < 0) return "--";
  struct tm tmDay = {};
  tmDay.tm_year = (ymd / 10000) - 1900;
  tmDay.tm_mon = ((ymd / 100) % 100) - 1;
  tmDay.tm_mday = ymd % 100;
  tmDay.tm_hour = 12;
  mktime(&tmDay); // normalizes and fills tm_wday
  char buf[8];
  strftime(buf, sizeof(buf), "%a", &tmDay);
  return String(buf);
}


static void drawWeatherStrip(bool force) {
  String combined = String(weatherShowDaily ? 1 : 0) + "|" + String(hourlyFilled) + "|" + String(dailyFilled);
  if (weatherShowDaily) {
    for (int i = 1; i < dailyFilled; i++) {
      combined += "|" + String(dailyYmd[i]) + ":" + String(dailyWeatherCode[i]) + ":" +
                  String((int)roundf(dailyTempMaxC[i])) + ":" + String(dailyPrecipProbMax[i]);
    }
  } else {
    for (int i = 0; i < hourlyFilled; i++) {
      combined += "|" + String(hourlyHour[i]) + ":" + String(hourlyCode[i]) + ":" +
                  String((int)roundf(hourlyTempC[i])) + ":" + String(hourlyPrecipProb[i]) + ":" + String(hourlyIsDay[i] ? 1 : 0);
    }
  }
  combined += "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);
  if (!force && combined == lastHourlyText) return;
  lastHourlyText = combined;

  tft.fillRect(V2_MARGIN_L, 126, V2_CONTENT_W, 224 - 126, COL_BG);

  if (weatherShowDaily) {
    int n = dailyFilled - 1; // index 0 is today -- the 5-day view shows the next 5 days
    if (n > 5) n = 5;
    if (n <= 0) {
      tft.setTextDatum(MC_DATUM);
      tft.setFreeFont(&F_BODY_REG);
      tft.setTextColor(COL_DIM, COL_BG);
      tft.drawString("No daily data yet", SCREEN_W / 2, 170);
      tft.setFreeFont(NULL);
      tft.setTextDatum(TL_DATUM);
      return;
    }
    const int colW = V2_CONTENT_W / 5;
    for (int i = 0; i < n; i++) {
      int srcIdx = i + 1;
      int cx = V2_MARGIN_L + colW * i + colW / 2;
      drawWeatherStripColumn(cx, dailyDayName(dailyYmd[srcIdx]), false, dailyWeatherCode[srcIdx], true,
                              formatDegreeOnly(dailyTempMaxC[srcIdx]), dailyPrecipProbMax[srcIdx]);
    }
  } else {
    if (hourlyFilled == 0) {
      tft.setTextDatum(MC_DATUM);
      tft.setFreeFont(&F_BODY_REG);
      tft.setTextColor(COL_DIM, COL_BG);
      tft.drawString("No hourly data yet", SCREEN_W / 2, 170);
      tft.setFreeFont(NULL);
      tft.setTextDatum(TL_DATUM);
      return;
    }
    const int colW = V2_CONTENT_W / HOURLY_COUNT;
    for (int i = 0; i < hourlyFilled; i++) {
      int cx = V2_MARGIN_L + colW * i + colW / 2;
      String label = (i == 0) ? "Now" : hourLabel12Lower(hourlyHour[i]);
      drawWeatherStripColumn(cx, label, i == 0, hourlyCode[i], hourlyIsDay[i],
                              formatDegreeOnly(hourlyTempC[i]), hourlyPrecipProb[i]);
    }
  }
}


static void drawWeatherStatColumn(int x, const String& label, const String& value, const String& word, bool accent) {
  tft.fillRect(x, 232, 104, V2_TABBAR_Y0 - 232, COL_BG);
  drawSmallCapsLabel(label, x, 232, TL_DATUM, COL_DIM, COL_BG);

  tft.setFreeFont(&F_TITLE);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(accent ? COL_ACCENT : COL_TEXT, COL_BG);
  int vw = tft.textWidth(value);
  tft.drawString(value, x, 246);
  tft.setFreeFont(NULL);

  tft.setFreeFont(&F_BODY_REG);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(word, x + vw + 6, 246 + 9);
  tft.setFreeFont(NULL);
}


static void drawWeatherStats(bool force) {
  String uKey = isnan(psiValue) ? "--" : String((int)roundf(psiValue));
  bool psiAccent;
  String uWord = psiLevelWord(psiValue, psiAccent);
  String psiCombined = uKey + "|" + uWord + "|" + String(psiAccent) + "|" + String(COL_BG);
  if (force || psiCombined != lastPsiText) {
    lastPsiText = psiCombined;
    drawWeatherStatColumn(V2_MARGIN_L, "Air " + String((char)0xC2) + (char)0xB7 + " PSI", uKey, uWord, psiAccent);
  }

  String mKey = isnan(uvIndexMax) ? "--" : String((int)roundf(uvIndexMax));
  String mWord = uvLevelWord(uvIndexMax);
  String uvCombined = mKey + "|" + mWord + "|" + String(COL_BG);
  if (force || uvCombined != lastUvText) {
    lastUvText = uvCombined;
    drawWeatherStatColumn(124, "UV max", mKey, mWord, false);
  }
}


void updateWeatherDynamic() {
  drawWeatherHeader(false);
  drawWeatherHero(false);
  drawWeatherStrip(false);
  drawWeatherStats(false);
}


// Spec "Tap the strip: 5-day view": tapping y 124-224 toggles hourly/daily.
bool handleWeatherTouch(int x, int y) {
  (void)x;
  if (currentPage != PAGE_WEATHER) return false;
  if (y < 124 || y >= 224) return false;

  weatherShowDaily = !weatherShowDaily;
  lastHourlyText = "";
  pageDirty = true;
  return true;
}



// Spec "Bus" > "Layout"/"Interactions": header (stop code, stop dots,
// stop name, page count), four 52px rows, favourite pinned first.
static void busGoToNextStop() {
  for (int step = 1; step <= BUS_MAX_STOPS; step++) {
    int idx = (busActiveStopIdx + step) % BUS_MAX_STOPS;
    if (busStops[idx].code.length() > 0) {
      busActiveStopIdx = idx;
      lastBusPageText = "";
      return;
    }
  }
}


// Favourite first (if present in this stop's list), then the rest in
// their already-soonest-first fetch order. orderOut must hold at least
// BUS_MAX_SERVICES ints.
static void busDisplayOrder(int active, int* orderOut, int& countOut) {
  int count = busServiceCountByStop[active];
  countOut = count;
  int favIdx = -1;
  if (favBusService.length() > 0) {
    for (int i = 0; i < count; i++) {
      if (busServicesByStop[active][i].serviceNo == favBusService) { favIdx = i; break; }
    }
  }
  int w = 0;
  if (favIdx >= 0) orderOut[w++] = favIdx;
  for (int i = 0; i < count; i++) {
    if (i == favIdx) continue;
    orderOut[w++] = i;
  }
}


static String busLoadWord(int pips) {
  switch (pips) {
    case 1: return "Seats";
    case 2: return "Standing";
    case 3: return "Packed";
    default: return "";
  }
}


static String busLoadLineText(const BusServiceInfo& svc) {
  String word = busLoadWord(svc.loadPips);
  if (word.length() == 0) return svc.vehicleType;
  if (svc.vehicleType.length() == 0) return word;
  return joinWithMiddot(word, svc.vehicleType);
}


// Spec row anatomy's ETA value: "Arr" accent at <=0min (no unit); "~N min"
// muted when unmonitored; otherwise "N min" in accent (favourite) or ink.
static void busEtaMainParts(const BusServiceInfo& svc, bool isFavourite, String& value, uint16_t& color,
                              uint16_t accentColor, uint16_t inkColor, uint16_t mutedColor) {
  int eta1 = busMinutesUntil(svc.arrival1);
  if (!svc.monitored) {
    value = "~" + String(eta1 < 0 ? 0 : eta1) + " min";
    color = mutedColor;
    return;
  }
  if (eta1 <= 0) { value = "Arr"; color = accentColor; return; }
  value = String(eta1) + " min";
  color = isFavourite ? accentColor : inkColor;
}


// Spec: "then 11 · 19" (minutes only); unmonitored -> "scheduled"; both
// NextBus2/3 empty -> "last bus".
static String busNextLineText(const BusServiceInfo& svc) {
  if (!svc.monitored) return "scheduled";
  int eta2 = busMinutesUntil(svc.arrival2);
  int eta3 = busMinutesUntil(svc.arrival3);
  if (eta2 < 0 && eta3 < 0) return "last bus";
  String parts = "";
  if (eta2 >= 0) parts = String(eta2);
  if (eta3 >= 0) parts = joinWithMiddot(parts, String(eta3));
  return "then " + parts;
}


void drawBusPageFull() {
  tft.fillScreen(COL_BG);
  drawV2TabBar();

  pageDirty = false;
  lastDrawnPage = PAGE_BUS;
  lastBusPageText = "";
  lastBusHeaderText = "";

  tft.drawFastHLine(V2_MARGIN_L, 54, V2_CONTENT_W, COL_STROKE);
}


static void drawBusHeader(int active, bool force) {
  int configuredCount = 0;
  int activePos = -1;
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    if (busStops[i].code.length() == 0) continue;
    if (i == active) activePos = configuredCount;
    configuredCount++;
  }

  String label = busStops[active].code.length() > 0 ? ("BUS STOP " + busStops[active].code) : "BUS STOP";
  String name = busStops[active].code.length() > 0
                  ? (busStopDisplayName(active).length() > 0 ? busStopDisplayName(active) : ("Stop " + busStops[active].code))
                  : "No stop configured";

  int total = busServiceCountByStop[active];
  int pageCount = (total + BUS_ROWS_PER_PAGE - 1) / BUS_ROWS_PER_PAGE;
  String pageLabel = (pageCount > 1) ? (String(busServicePage[active] + 1) + "/" + String(pageCount)) : "";

  String combined = label + "|" + name + "|" + pageLabel + "|" + String(configuredCount) + "|" + String(activePos) + "|" + String(COL_BG);
  if (!force && combined == lastBusHeaderText) return;
  lastBusHeaderText = combined;

  tft.fillRect(0, 0, SCREEN_W, 54, COL_BG);
  drawSmallCapsLabel(label, V2_MARGIN_L, 12, TL_DATUM, COL_DIM, COL_BG);

  tft.setFreeFont(&F_HEAD);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(name, V2_MARGIN_L, 26);
  tft.setFreeFont(NULL);

  if (configuredCount > 1) {
    int dotCount = configuredCount > 3 ? 3 : configuredCount;
    int totalW = dotCount * 6 + (dotCount - 1) * 2;
    int startX = V2_MARGIN_R - totalW;
    for (int i = 0; i < dotCount; i++) {
      int cx = startX + i * 8 + 3;
      tft.fillCircle(cx, 14 + 3, 3, (i == activePos) ? COL_TEXT : COL_STROKE);
    }
  }

  if (pageLabel.length() > 0) {
    tft.setFreeFont(&F_SMALL);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString(pageLabel, V2_MARGIN_R, 30);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
  }
}


static void drawBusRow(int rowTop, const BusServiceInfo& svc, bool isFavourite, bool drawHairline) {
  tft.fillRect(V2_MARGIN_L, rowTop, V2_CONTENT_W, BUS_ROW_H - 1, COL_BG);

  tft.setFreeFont(&F_NUM);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(isFavourite ? COL_ACCENT : COL_TEXT, COL_BG);
  tft.drawString(svc.serviceNo, V2_MARGIN_L, rowTop + 8);
  tft.setFreeFont(NULL);

  int pipsX = V2_MARGIN_L;
  int pipsY = rowTop + 36;
  for (int p = 0; p < 3; p++) {
    int px = pipsX + p * 9;
    tft.drawRect(px, pipsY + 3, 6, 6, COL_DIM);
    if (p < svc.loadPips) tft.fillRect(px, pipsY + 3, 6, 6, COL_DIM);
  }
  String loadLine = busLoadLineText(svc);
  if (loadLine.length() > 0) {
    tft.setFreeFont(&F_SMALL);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString(loadLine, pipsX + 3 * 9 + 4, pipsY);
    tft.setFreeFont(NULL);
  }

  String etaValue; uint16_t etaColor;
  busEtaMainParts(svc, isFavourite, etaValue, etaColor, COL_ACCENT, COL_TEXT, COL_DIM);
  // Numeric prefix in F_NUM, "min" unit in F_BODY Bold baseline-aligned --
  // "Arr" has no numeric prefix and is drawn as one F_NUM token.
  String numPart, restPart;
  splitNumericPrefix(etaValue, numPart, restPart);
  tft.setTextDatum(TR_DATUM);
  if (numPart.length() > 0) {
    tft.setFreeFont(&F_BODY_BOLD);
    tft.setTextColor(etaColor, COL_BG);
    int unitW = restPart.length() > 0 ? tft.textWidth(restPart) + 3 : 0;
    tft.setFreeFont(&F_NUM);
    int numW = tft.textWidth(numPart);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
    int startX = V2_MARGIN_R - unitW - numW;
    tft.setFreeFont(&F_NUM);
    tft.setTextColor(etaColor, COL_BG);
    tft.drawString(numPart, startX, rowTop + 8);
    tft.setFreeFont(NULL);
    if (restPart.length() > 0) {
      tft.setFreeFont(&F_BODY_BOLD);
      tft.drawString(restPart, startX + numW + 3, rowTop + 8 + 10);
      tft.setFreeFont(NULL);
    }
  } else {
    tft.setFreeFont(&F_NUM);
    tft.setTextColor(etaColor, COL_BG);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(restPart, V2_MARGIN_R, rowTop + 8);
    tft.setFreeFont(NULL);
  }
  tft.setTextDatum(TL_DATUM);

  String nextLine = busNextLineText(svc);
  tft.setFreeFont(&F_SMALL);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(nextLine, V2_MARGIN_R, rowTop + 36);
  tft.setFreeFont(NULL);
  tft.setTextDatum(TL_DATUM);

  if (drawHairline) {
    tft.drawFastHLine(V2_MARGIN_L, rowTop + BUS_ROW_H - 1, V2_CONTENT_W, COL_STROKE);
  }
}


static String busTimeOfDayText(time_t t) {
  if (t <= 0) return "--:--";
  struct tm tmT;
  localtime_r(&t, &tmT);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", tmT.tm_hour, tmT.tm_min);
  return String(buf);
}


void updateBusDynamic() {
  int active = busActiveStopIdx;
  if (active < 0 || active >= BUS_MAX_STOPS) active = 0;

  drawBusHeader(active, false);

  bool stale = busDataValidByStop[active] && busBackoff[active].failCount > 0;

  String combined = String(active) + "|" + busStops[active].code + "|" + String(busDataValidByStop[active] ? 1 : 0) +
                    "|" + String(busServiceCountByStop[active]) + "|" + String(busServicePage[active]) +
                    "|" + String(favBusService) + "|" + String(stale);
  for (int i = 0; i < busServiceCountByStop[active]; i++) {
    const BusServiceInfo& svc = busServicesByStop[active][i];
    combined += "|" + svc.serviceNo + ":" + String((long)svc.arrival1) + ":" + String((long)svc.arrival2) +
                ":" + String((long)svc.arrival3) + ":" + String(svc.monitored) + ":" + String(svc.loadPips);
  }

  if (combined == lastBusPageText) return;
  lastBusPageText = combined;

  tft.fillRect(V2_MARGIN_L, BUS_ROW_Y0, V2_CONTENT_W, (BUS_ROWS_PER_PAGE * BUS_ROW_H) + 14, COL_BG);

  if (busStops[active].code.length() == 0) {
    tft.setFreeFont(&F_HEAD);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("No stop configured", SCREEN_W / 2, 140);
    tft.setFreeFont(NULL);
    tft.setFreeFont(&F_BODY_REG);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("Add one on the settings page", SCREEN_W / 2, 164);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  if (!busDataValidByStop[active]) {
    tft.setFreeFont(&F_BODY_REG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("Fetching bus times...", SCREEN_W / 2, 140);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  int total = busServiceCountByStop[active];
  if (total == 0) {
    tft.setFreeFont(&F_HEAD);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("No buses right now", SCREEN_W / 2, 140);
    tft.setFreeFont(NULL);
    tft.setFreeFont(&F_BODY_REG);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("Tap the header to switch stop", SCREEN_W / 2, 164);
    tft.setFreeFont(NULL);
    tft.setFreeFont(&F_SMALL);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("Checked " + busTimeOfDayText(lastBusFetchByStop[active]), SCREEN_W / 2, 180);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  int order[BUS_MAX_SERVICES];
  int count;
  busDisplayOrder(active, order, count);

  int pageCount = (count + BUS_ROWS_PER_PAGE - 1) / BUS_ROWS_PER_PAGE;
  if (busServicePage[active] >= pageCount) busServicePage[active] = 0;
  int startIdx = busServicePage[active] * BUS_ROWS_PER_PAGE;

  for (int slot = 0; slot < BUS_ROWS_PER_PAGE; slot++) {
    int displayIdx = startIdx + slot;
    if (displayIdx >= count) break;
    int svcIdx = order[displayIdx];
    const BusServiceInfo& svc = busServicesByStop[active][svcIdx];
    bool isFav = (favBusService.length() > 0 && svc.serviceNo == favBusService);
    int rowTop = BUS_ROW_Y0 + slot * BUS_ROW_H;
    bool lastVisibleRow = (slot == BUS_ROWS_PER_PAGE - 1) || (displayIdx == count - 1);
    drawBusRow(rowTop, svc, isFav, !lastVisibleRow);
  }

  if (stale) {
    tft.setFreeFont(&F_SMALL);
    tft.setTextColor(COL_ACCENT, COL_BG);
    int fh = tft.fontHeight();
    tft.fillCircle(V2_MARGIN_L + 3, 268 + fh / 2, 3, COL_ACCENT);
    drawTextWithDegree(V2_MARGIN_L + 10, 268,
                         String((char)0xC2) + (char)0xB7 + " bus data from " + busTimeOfDayText(lastBusFetchByStop[active]),
                         COL_ACCENT, COL_BG);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
  }
}



// Spec "Status" > "Layout": header+version, product name/owner/case swatch,
// colourway swatches, brightness/night-mode segmented options, info rows.
static String wifiInfoText(uint16_t& color) {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) {
    color = COL_ACCENT;
    return "Not connected";
  }
  color = COL_TEXT;
  String ssid = storedWifiSsid.length() > 0 ? storedWifiSsid : WiFi.SSID();
  return ssid + " " + (char)0xC2 + (char)0xB7 + " " + String(WiFi.RSSI()) + " dBm";
}


static String uptimeInfoText() {
  return uptimeText() + " " + (char)0xC2 + (char)0xB7 + " " + String(ESP.getFreeHeap() / 1024) + " KB free";
}


static uint16_t currentCaseColor() {
  const char* key; const char* label; uint16_t swatch;
  for (int i = 0; i < shellColourCount(); i++) {
    shellColourAt(i, &key, &label, &swatch);
    if (shellColourKey == key) return swatch;
  }
  shellColourAt(0, &key, &label, &swatch);
  return swatch;
}


// Spec "Components" > "Segmented option": options right-aligned at a given
// x with 14px gaps; active Bold ink with a 2px underline 4px below.
// computeSegmentedLayout() is shared by the drawer and the hit-tester so
// their geometry can never drift apart.
static void computeSegmentedLayout(int rightX, const char* const* labels, int count, int activeIdx, int* startXs, int* widths) {
  int totalW = 0;
  for (int i = 0; i < count; i++) {
    tft.setFreeFont(i == activeIdx ? &F_BODY_BOLD : &F_BODY_REG);
    widths[i] = tft.textWidth(labels[i]);
    tft.setFreeFont(NULL);
    totalW += widths[i];
    if (i > 0) totalW += STATUS_SEGMENTED_GAP;
  }
  int cx = rightX - totalW;
  for (int i = 0; i < count; i++) {
    startXs[i] = cx;
    cx += widths[i] + STATUS_SEGMENTED_GAP;
  }
}


static void drawSegmentedOption(int rightX, int y, const char* const* labels, int count, int activeIdx) {
  int startXs[6], widths[6];
  computeSegmentedLayout(rightX, labels, count, activeIdx, startXs, widths);

  tft.setTextDatum(TL_DATUM);
  for (int i = 0; i < count; i++) {
    bool active = (i == activeIdx);
    tft.setFreeFont(active ? &F_BODY_BOLD : &F_BODY_REG);
    tft.setTextColor(active ? COL_TEXT : COL_DIM, COL_BG);
    tft.drawString(labels[i], startXs[i], y);
    tft.setFreeFont(NULL);
    if (active) {
      tft.drawFastHLine(startXs[i], y + 20, widths[i], COL_TEXT);
      tft.drawFastHLine(startXs[i], y + 21, widths[i], COL_TEXT);
    }
  }
}


static int segmentedHitTest(int x, int y, int labelY, int rightX, const char* const* labels, int count, int activeIdx) {
  if (y < labelY - 6 || y > labelY + 26) return -1;
  int startXs[6], widths[6];
  computeSegmentedLayout(rightX, labels, count, activeIdx, startXs, widths);
  for (int i = 0; i < count; i++) {
    if (x >= startXs[i] - 6 && x < startXs[i] + widths[i] + 6) return i;
  }
  return -1;
}


void drawStatusPageFull() {
  tft.fillScreen(COL_BG);
  drawV2TabBar();

  pageDirty = false;
  lastDrawnPage = PAGE_STATUS;

  lastStatusHeaderText = "";
  lastStatusColourwayText = "";
  lastStatusSegmentsText = "";
  lastStatusInfoText = "";

  tft.drawFastHLine(V2_MARGIN_L, 76, V2_CONTENT_W, COL_STROKE);
  tft.drawFastHLine(V2_MARGIN_L, 208, V2_CONTENT_W, COL_STROKE);
}


static void drawStatusHeader(bool force) {
  String combined = homeTitleText() + "|" + ownerName + "|" + String(COL_BG);
  if (!force && combined == lastStatusHeaderText) return;
  lastStatusHeaderText = combined;

  tft.fillRect(0, 0, SCREEN_W, 76, COL_BG);

  drawSmallCapsLabel("STATUS", V2_MARGIN_L, 12, TL_DATUM, COL_DIM, COL_BG);

  tft.setFreeFont(&F_SMALL);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(String("v") + FIRMWARE_VERSION, V2_MARGIN_R, 12);
  tft.setFreeFont(NULL);
  tft.setTextDatum(TL_DATUM);

  tft.setFreeFont(&F_TITLE);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(homeTitleText(), V2_MARGIN_L, 26);
  tft.setFreeFont(NULL);

  if (ownerName.length() > 0) {
    tft.setFreeFont(&F_BODY_REG);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString("Made by " + ownerName, V2_MARGIN_L, 52);
    tft.setFreeFont(NULL);
  }

  int swatchCx = 214, swatchCy = 44, swatchR = 14;
  tft.fillCircle(swatchCx, swatchCy, swatchR, currentCaseColor());
  tft.drawCircle(swatchCx, swatchCy, swatchR, COL_STROKE);
}


static void drawStatusColourway(bool force) {
  String combined = shellColourKey + "|" + String(COL_BG) + "|" + String(COL_TEXT);
  if (!force && combined == lastStatusColourwayText) return;
  lastStatusColourwayText = combined;

  tft.fillRect(0, 82, SCREEN_W, 118 + STATUS_SWATCH_RING_R + 6 - 82, COL_BG);

  drawSmallCapsLabel("COLOURWAY", V2_MARGIN_L, 86, TL_DATUM, COL_DIM, COL_BG);
  drawSmallCapsLabel(shellColourLabel(shellColourKey), V2_MARGIN_R, 86, TR_DATUM, COL_TEXT, COL_BG);

  int n = shellColourCount();
  for (int i = 0; i < n; i++) {
    const char* key; const char* label; uint16_t swatch;
    shellColourAt(i, &key, &label, &swatch);
    int cx = STATUS_SWATCH_X0 + i * STATUS_SWATCH_PITCH;
    bool selected = (shellColourKey == key);
    if (selected) {
      tft.drawCircle(cx, STATUS_SWATCH_Y, STATUS_SWATCH_RING_R, COL_TEXT);
      tft.drawCircle(cx, STATUS_SWATCH_Y, STATUS_SWATCH_RING_R - 1, COL_TEXT);
    }
    tft.fillCircle(cx, STATUS_SWATCH_Y, STATUS_SWATCH_R, swatch);
    tft.drawCircle(cx, STATUS_SWATCH_Y, STATUS_SWATCH_R, COL_STROKE);
  }
}


static const char* const BRIGHTNESS_LABELS[3] = {"Low", "Mid", "High"};
static const char* const NIGHT_LABELS[2] = {"Off", "Auto"};


static void drawStatusSegments(bool force) {
  int nightIdx = (nightModeKey == "auto") ? 1 : 0;
  String combined = String(brightnessLevel) + "|" + String(nightIdx) + "|" + String(COL_BG);
  if (!force && combined == lastStatusSegmentsText) return;
  lastStatusSegmentsText = combined;

  tft.fillRect(0, 142, SCREEN_W, 208 - 142, COL_BG);

  tft.setFreeFont(&F_BODY_REG);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Brightness", V2_MARGIN_L, STATUS_BRIGHTNESS_Y);
  tft.drawString("Night mode", V2_MARGIN_L, STATUS_NIGHT_Y);
  tft.setFreeFont(NULL);

  drawSegmentedOption(V2_MARGIN_R, STATUS_BRIGHTNESS_Y, BRIGHTNESS_LABELS, 3, brightnessLevel);
  drawSegmentedOption(V2_MARGIN_R, STATUS_NIGHT_Y, NIGHT_LABELS, 2, nightIdx);
}


static String formatAgeShort(long ageSec) {
  if (ageSec < 0) return "--";
  if (ageSec < 60) return String(ageSec) + "s";
  if (ageSec < 3600) return String(ageSec / 60) + "m";
  return String(ageSec / 3600) + "h";
}


static void drawStatusInfoRows(bool force) {
  uint16_t wifiColor;
  String wifi = wifiInfoText(wifiColor);

  time_t now = time(nullptr);
  long busAge = (lastBusFetchByStop[busActiveStopIdx] > 0) ? (long)(now - lastBusFetchByStop[busActiveStopIdx]) : -1;
  long wxAge  = (lastWeatherFetch > 0) ? (long)(now - lastWeatherFetch) : -1;
  long airAge = (lastPsiFetch > 0) ? (long)(now - lastPsiFetch) : -1;

  bool syncing = forcedSyncStartedMs != 0 && (millis() - forcedSyncStartedMs < 10000);
  String ip = ipText();
  String uptime = uptimeInfoText();

  String combined = wifi + "|" + String(syncing) + "|" + String(busAge) + "|" + String(wxAge) + "|" +
                     String(airAge) + "|" + ip + "|" + uptime + "|" + String(COL_BG);
  if (!force && combined == lastStatusInfoText) return;
  lastStatusInfoText = combined;

  tft.fillRect(V2_MARGIN_L, STATUS_INFO_Y0 - 2, V2_CONTENT_W, STATUS_INFO_ROW_H * 4 + 4, COL_BG);

  tft.setFreeFont(&F_SMALL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString("Wi-Fi", V2_MARGIN_L, STATUS_INFO_Y0);
  tft.drawString("Synced", V2_MARGIN_L, STATUS_INFO_Y0 + STATUS_INFO_ROW_H);
  tft.drawString("IP", V2_MARGIN_L, STATUS_INFO_Y0 + STATUS_INFO_ROW_H * 2);
  tft.drawString("Uptime", V2_MARGIN_L, STATUS_INFO_Y0 + STATUS_INFO_ROW_H * 3);
  tft.setFreeFont(NULL);

  tft.setFreeFont(&F_SMALL);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(wifiColor, COL_BG);
  tft.drawString(wifi, V2_MARGIN_R, STATUS_INFO_Y0);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(ip, V2_MARGIN_R, STATUS_INFO_Y0 + STATUS_INFO_ROW_H * 2);
  tft.drawString(uptime, V2_MARGIN_R, STATUS_INFO_Y0 + STATUS_INFO_ROW_H * 3);
  tft.setFreeFont(NULL);
  tft.setTextDatum(TL_DATUM);

  int syncedY = STATUS_INFO_Y0 + STATUS_INFO_ROW_H;
  if (syncing) {
    tft.setFreeFont(&F_SMALL);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("Syncing...", V2_MARGIN_R, syncedY);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
  } else {
    // Spec "Synced": "Any feed older than 2x its refresh interval is
    // drawn in accent" -- built right-to-left so each segment's own
    // colour survives (a single drawString can't mix colours).
    struct Seg { String label; String age; bool stale; };
    Seg segs[3] = {
      {"Bus ", formatAgeShort(busAge), busAge < 0 || busAge > (long)BUS_STALE_SEC},
      {"Wx ",  formatAgeShort(wxAge),  wxAge  < 0 || wxAge  > (long)(2 * WEATHER_INTERVAL_SEC)},
      {"Air ", formatAgeShort(airAge), airAge < 0 || airAge > (long)(2 * PSI_INTERVAL_SEC)},
    };

    String sep = String(" ") + (char)0xC2 + (char)0xB7 + " ";

    tft.setFreeFont(&F_SMALL);
    tft.setTextDatum(TL_DATUM);
    int sepW = tft.textWidth(sep);
    int totalW = 0;
    for (int i = 0; i < 3; i++) {
      totalW += tft.textWidth(segs[i].label) + tft.textWidth(segs[i].age);
      if (i > 0) totalW += sepW;
    }
    int cx = V2_MARGIN_R - totalW;
    for (int i = 0; i < 3; i++) {
      if (i > 0) {
        tft.setTextColor(COL_DIM, COL_BG);
        tft.drawString(sep, cx, syncedY);
        cx += sepW;
      }
      tft.setTextColor(COL_DIM, COL_BG);
      tft.drawString(segs[i].label, cx, syncedY);
      cx += tft.textWidth(segs[i].label);
      tft.setTextColor(segs[i].stale ? COL_ACCENT : COL_TEXT, COL_BG);
      tft.drawString(segs[i].age, cx, syncedY);
      cx += tft.textWidth(segs[i].age);
    }
    tft.setFreeFont(NULL);
  }
}


void updateStatusDynamic() {
  drawStatusHeader(false);
  drawStatusColourway(false);
  drawStatusSegments(false);
  drawStatusInfoRows(false);
}


// Spec "Status" > "Interactions": swatch -> apply colourway instantly;
// brightness/night-mode option -> set and save; Synced row -> force sync.
// STATUS header long-press (calibration) is handled in main.cpp via
// touchHoldMsInRect(), not here.
bool handleStatusTouch(int x, int y) {
  if (currentPage != PAGE_STATUS) return false;

  if (y >= STATUS_SWATCH_Y - STATUS_SWATCH_HIT && y < STATUS_SWATCH_Y + STATUS_SWATCH_HIT) {
    int n = shellColourCount();
    for (int i = 0; i < n; i++) {
      int cx = STATUS_SWATCH_X0 + i * STATUS_SWATCH_PITCH;
      if (x >= cx - STATUS_SWATCH_HIT && x < cx + STATUS_SWATCH_HIT) {
        const char* key; const char* label; uint16_t swatch;
        shellColourAt(i, &key, &label, &swatch);
        applyShellColourByKey(String(key));
        prefs.putString("shellColour", shellColourKey);
        pageDirty = true;
        return true;
      }
    }
  }

  int nightIdx = (nightModeKey == "auto") ? 1 : 0;
  int brightHit = segmentedHitTest(x, y, STATUS_BRIGHTNESS_Y, V2_MARGIN_R, BRIGHTNESS_LABELS, 3, brightnessLevel);
  if (brightHit >= 0) {
    setBrightnessLevel(brightHit);
    pageDirty = true;
    return true;
  }

  int nightHit = segmentedHitTest(x, y, STATUS_NIGHT_Y, V2_MARGIN_R, NIGHT_LABELS, 2, nightIdx);
  if (nightHit >= 0) {
    nightModeKey = (nightHit == 1) ? "auto" : "off";
    prefs.putString("nightMode", nightModeKey);
    applyNightOverride();
    pageDirty = true;
    return true;
  }

  int syncedY = STATUS_INFO_Y0 + STATUS_INFO_ROW_H;
  if (y >= syncedY - 7 && y < syncedY + 8) {
    forceSyncAllFeeds();
    return true;
  }

  return false;
}



void drawCurrentPageFull() {
  switch (currentPage) {
    case PAGE_HOME:    drawHomePageFull(); break;
    case PAGE_WEATHER: drawWeatherPageFull(); break;
    case PAGE_BUS:     drawBusPageFull(); break;
    case PAGE_STATUS:  drawStatusPageFull(); break;
  }

  if (focusTimerState == TIMER_DONE) drawTimerDoneOverlay(true);
  else if (timerCustomEntryOpen) drawCustomTimerStepperOverlay(true);
  else if (timerPickerOpen) drawTimerPickerOverlay(true);
}


void updateCurrentPageDynamic() {
  if (focusTimerState == TIMER_DONE) {
    drawTimerDoneOverlay(false);
    return;
  }
  if (timerCustomEntryOpen) {
    drawCustomTimerStepperOverlay(false);
    return;
  }
  if (timerPickerOpen) {
    drawTimerPickerOverlay(false);
    return;
  }

  switch (currentPage) {
    case PAGE_HOME:    updateHomeDynamic(); break;
    case PAGE_WEATHER: updateWeatherDynamic(); break;
    case PAGE_BUS:     updateBusDynamic(); break;
    case PAGE_STATUS:  updateStatusDynamic(); break;
  }
}




// Spec "Home" > "Interactions": Bus tile -> Bus page; Outdoor/Rain tile ->
// Weather page; Timer tile -> start/pause/resume (state.cpp's own state
// machine); Clock has no tap action (only the long-press in
// handleHomeLongPress() below).
bool handleHomeTouch(int x, int y) {
  if (currentPage != PAGE_HOME) return false;
  if (focusTimerState == TIMER_DONE) return false; // handled by handleTimerDoneDialogTouch() first

  int tx, ty, tw, th;

  getHomeTileRect(0, tx, ty, tw, th); // Bus
  if (x >= tx && x < tx + tw && y >= ty && y < ty + th) {
    currentPage = PAGE_BUS;
    pageDirty = true;
    return true;
  }

  getHomeTileRect(1, tx, ty, tw, th); // Outdoor
  if (x >= tx && x < tx + tw && y >= ty && y < ty + th) {
    currentPage = PAGE_WEATHER;
    pageDirty = true;
    return true;
  }

  getHomeTileRect(2, tx, ty, tw, th); // Rain
  if (x >= tx && x < tx + tw && y >= ty && y < ty + th) {
    currentPage = PAGE_WEATHER;
    pageDirty = true;
    return true;
  }

  getHomeTileRect(3, tx, ty, tw, th); // Timer
  if (x >= tx && x < tx + tw && y >= ty && y < ty + th) {
    handleTimerTileTap();
    return true;
  }

  return false;
}


// Spec "Home" > "Interactions": Clock long-press toggles 12h/24h (saved).
// The Timer tile no longer has a long-press action -- idle/paused taps
// open the picker instead (handleHomeTouch() above); see CONTEXT.md's
// "timer picker" entry.
bool handleHomeLongPress(int x, int y) {
  if (currentPage != PAGE_HOME) return false;
  if (focusTimerState == TIMER_DONE) return false;

  if (x >= HOME_CLOCK_HIT_X0 && x < HOME_CLOCK_HIT_X1 && y >= HOME_CLOCK_HIT_Y0 && y < HOME_CLOCK_HIT_Y1) {
    saveClockIs24h(!clockIs24h);
    cacheClock = "";
    pageDirty = true;
    return true;
  }

  return false;
}


bool handleTimerDoneDialogTouch(int x, int y) {
  (void)x;
  (void)y;
  if (focusTimerState != TIMER_DONE) return false;
  dismissTimerDone();
  return true;
}


// Hit-testing mirrors drawTimerPickerOverlay()'s row layout exactly --
// same timerPickerPresetStartIdx()/TIMER_PICKER_ROWS_Y0/ROW_H constants.
bool handleTimerPickerTouch(int x, int y) {
  if (!timerPickerOpen) return false;
  (void)x;

  int rowCount = timerPickerRowCount();
  int idx = (y - TIMER_PICKER_ROWS_Y0) / TIMER_PICKER_ROW_H;
  if (y < TIMER_PICKER_ROWS_Y0 || idx < 0 || idx >= rowCount) return true; // modal: swallow, no row hit

  int presetStart = timerPickerPresetStartIdx();
  if (focusTimerState == TIMER_PAUSED && idx == 0) { resumeFocusTimer(); return true; }
  if (focusTimerState == TIMER_PAUSED && idx == 1) { stopFocusTimerToIdle(); return true; }
  if (idx < presetStart + FOCUS_PRESET_COUNT) {
    startFocusTimerWithMinutes(timerPresetMin[idx - presetStart]);
    return true;
  }
  openCustomTimerStepper();
  return true;
}


// Hit-testing mirrors drawCustomTimerStepperOverlay()'s layout exactly.
// Outside the panel band (y < TIMER_CUSTOM_PANEL_Y0 or
// y >= TIMER_CUSTOM_PANEL_Y1) closes everything via closeTimerPicker() --
// see the comment above drawCustomTimerStepperOverlay() for why that's
// the right target rather than forcing idle.
bool handleCustomTimerStepperTouch(int x, int y) {
  if (!timerCustomEntryOpen) return false;

  if (y < TIMER_CUSTOM_PANEL_Y0 || y >= TIMER_CUSTOM_PANEL_Y1) {
    closeTimerPicker();
    return true;
  }

  if (y >= TIMER_CUSTOM_STEP_Y0 && y < TIMER_CUSTOM_STEP_Y1) {
    int stepW = V2_CONTENT_W / 4;
    int i = (x - V2_MARGIN_L) / stepW;
    if (i >= 0 && i < 4) adjustCustomTimerMinutes(TIMER_CUSTOM_STEP_DELTAS[i]);
    return true;
  }

  if (y >= TIMER_CUSTOM_ACTIONS_Y0 - 8 && y < TIMER_CUSTOM_ACTIONS_Y1) {
    if (x < SCREEN_W / 2) {
      cancelCustomTimerStepper();
    } else {
      startFocusTimerWithMinutes(customTimerMinutes);
    }
    return true;
  }

  return true; // inside the panel but not on a control -- swallow, no-op
}


// Spec "Bus" > "Interactions": header (y0-54) -> next configured stop,
// wraps; rows area (y56-264) -> next page of four, wraps.
bool handleBusTouch(int x, int y) {
  (void)x;
  if (currentPage != PAGE_BUS) return false;

  if (y < 54) {
    busGoToNextStop();
    pageDirty = true;
    return true;
  }

  if (y >= BUS_ROW_Y0 && y < BUS_ROW_Y0 + BUS_ROWS_PER_PAGE * BUS_ROW_H) {
    int active = busActiveStopIdx;
    int total = busServiceCountByStop[active];
    int pageCount = (total + BUS_ROWS_PER_PAGE - 1) / BUS_ROWS_PER_PAGE;
    if (pageCount > 1) {
      busServicePage[active] = (busServicePage[active] + 1) % pageCount;
      lastBusPageText = "";
      pageDirty = true;
    }
    return true;
  }

  return false;
}


// Spec "Bus" > "Interactions": long-press a row -> that service becomes
// the favourite (saved to NVS, Home updates too).
bool handleBusLongPress(int x, int y) {
  (void)x;
  if (currentPage != PAGE_BUS) return false;
  if (y < BUS_ROW_Y0 || y >= BUS_ROW_Y0 + BUS_ROWS_PER_PAGE * BUS_ROW_H) return false;

  int active = busActiveStopIdx;
  int total = busServiceCountByStop[active];
  if (total == 0) return false;

  int order[BUS_MAX_SERVICES];
  int count;
  busDisplayOrder(active, order, count);

  int row = (y - BUS_ROW_Y0) / BUS_ROW_H;
  int displaySlot = busServicePage[active] * BUS_ROWS_PER_PAGE + row;
  if (displaySlot < 0 || displaySlot >= count) return false;

  favBusService = busServicesByStop[active][order[displaySlot]].serviceNo;
  prefs.putString("favBusSvc", favBusService);
  clearHomeTileCaches();
  lastBusPageText = "";
  pageDirty = true;
  return true;
}


// =========================================================
// NAVIGATION
// =========================================================
void handleNavTouch(int x, int y) {
  // Touch strip per spec: full SCREEN_W/4 columns, y >= V2_TAB_TOUCH_Y0 --
  // wider than the visual label cells in drawV2TabBar() on purpose.
  if (y < V2_TAB_TOUCH_Y0) return;

  int btnW = SCREEN_W / 4;
  int idx = x / btnW;
  if (idx < 0 || idx > 3) return;

  Page newPage = (Page)idx;
  if (newPage != currentPage) {
    // Spec "Tap the strip: 5-day view": "The view resets to hourly when
    // you leave the page" -- the only way to leave Weather is this nav tap.
    if (currentPage == PAGE_WEATHER) weatherShowDaily = false;
    currentPage = newPage;
    pageDirty = true;
  }
}


// =========================================================
// #define COLOUR_TEST -- boot-time palette check (spec Prompt 1,
// acceptance: "COLOUR_TEST shows all six palettes with the right colours,
// no swapped red/blue, not inverted"). Six stripes, each stripe's own bg,
// with ink/muted/rule/accent samples and the colourway's name. Called once
// from setup() in place of the normal boot sequence -- no WiFi, no touch.
// =========================================================
void drawColourTestScreen() {
  const int n = shellColourCount();
  const int stripeH = SCREEN_H / n;

  for (int i = 0; i < n; i++) {
    const char* key; const char* label; uint16_t caseColor;
    shellColourAt(i, &key, &label, &caseColor);
    uint16_t bg, ink, muted, rule, accent;
    colourwayRolesAt(i, &bg, &ink, &muted, &rule, &accent);

    int y0 = i * stripeH;
    int h = (i == n - 1) ? (SCREEN_H - y0) : stripeH;

    tft.fillRect(0, y0, SCREEN_W, h, bg);

    tft.setTextDatum(TL_DATUM);
    tft.setFreeFont(&F_HEAD);
    tft.setTextColor(ink, bg);
    tft.drawString(label, V2_MARGIN_L, y0 + 4);
    tft.setFreeFont(NULL);

    tft.setTextFont(1);
    tft.setTextColor(muted, bg);
    tft.drawString("muted sample", V2_MARGIN_L, y0 + h - 14);

    int ruleY = y0 + h / 2 + 6;
    tft.drawFastHLine(V2_MARGIN_L, ruleY, V2_CONTENT_W - 60, rule);

    tft.fillRect(V2_MARGIN_R - 36, y0 + h / 2 - 8, 24, 24, accent);
  }
}
