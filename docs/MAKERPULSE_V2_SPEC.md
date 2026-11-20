# MakerPulse V2 — Colourway Design Spec & Build Prompts

Nov 20, 2026 · @Jack

## Design intent

MakerPulse V2 makes the screen part of the object: the display takes the colour of the student's printed case, drops every card border, and lets big numbers do the work. It keeps the v1.2.0 feature set (clock, bus, outdoor, rain, PSI, timer, week number) and the four tabs, so the code can migrate rather than restart.

The screens are drawn at 1:1 on the canvas [MakerPulse V2 — Colourway screens](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A). Every coordinate in this doc matches that canvas pixel for pixel.

**Five rules that hold the design together**

1. **One colourway, five roles.** Each palette has exactly `bg`, `ink`, `muted`, `rule` and `accent`. No other colours appear on screen, so a new colourway is one struct, not a redesign.
2. **No boxes.** Grouping comes from space and 1 px hairlines, never borders or filled cards. This frees about 20 px per tile versus v1.2.0 and is what makes the type big.
3. **Accent means “look now”.** Accent is reserved for the favourite bus, a bus arriving, a running timer, the rain bars, and stale-data warnings. If everything is accented, nothing is.
4. **Numbers first, words second.** The value is the largest thing in each block; its label sits above in small caps and its context sits below in muted text. A glance from across a desk reads the number alone.
5. **Same skeleton on every page.** 12 px side margins, a small-caps header at y 12, hairlines at the same few heights, and the text tab bar at y 280–320. Students learn one layout and see it four times.

**Why this suits the workshop:** students already choose a filament colour for the case. Picking the matching colourway on the Status page closes the loop between the print and the code, and the palette struct is a gentle first look at how a theme system works.

## Design system

Everything below is in device pixels on the ESP32-2432S028 (CYD), 240 × 320 portrait. Text positions are the **top-left of the text box** (TFT\_eSPI `TL_DATUM`, or LVGL top alignment) unless a row says right- or centre-aligned.

### Screen grid

| Zone | y range | Rule |
| --- | --- | --- |
| Side margins | x 12 and x 228 | All content sits inside a 216 px column |
| Page header | y 12–24 | Small-caps label left, muted meta right |
| Content | y 26–272 | Page-specific; hairlines snap to y 54, 76, 100, 124, 208 or 224 |
| Tab bar | y 280–320 | Four 54 px cells from x 12; whole-width touch strip y 276–320 |

### Type scale

One family, **Inter**, in three weights. Digits must not jitter as they change, so the clock and timer draw each digit in a fixed-width cell (width of “0”) whatever the font does.

| Token | Size / weight | Used for | Glyphs to generate |
| --- | --- | --- | --- |
| `F_DISPLAY` | 56 px Bold | Clock, weather hero temp, timer-done | `0-9 : °` |
| `F_NUM` | 28 px Bold | Tile values, bus service and ETA | ASCII 0x20–0x7E, `°`, `~` |
| `F_TITLE` | 22 px Bold | Date number, PSI / UV value, “MakerPulse” | ASCII + `°` |
| `F_HEAD` | 17 px Bold | Bus stop name, empty-state headline | ASCII |
| `F_BODY` | 13 px Regular + Bold | Sun line, condition, tabs, settings rows, units (`min`) | ASCII + `· ° – −` |
| `F_SMALL` | 11 px Medium | Small-caps labels (+0.08 em tracking), sub-lines, info rows | ASCII + `· – − ●` |

Small-caps labels are simply uppercase strings in `F_SMALL`; letter-spacing is applied by drawing with +1 px per character.

### Colour roles

| Role | Use it for | Never use it for |
| --- | --- | --- |
| `bg` | Whole screen fill, text on accent | Text on bg |
| `ink` | Values, headings, active tab and its 2 px underline, icons | Hairlines |
| `muted` | Labels, sub-lines, inactive tabs, load pips | Values |
| `rule` | 1 px hairlines, rain-bar tracks, swatch outlines | Text |
| `accent` | Favourite bus number, “Arr”, running timer label, rain bars, rain/storm icon details, offline line, timer-done takeover | Decoration |

### Colourways

Each colourway is named after the case filament it matches. RGB565 values are pre-computed so the firmware never converts at runtime. All text pairs pass WCAG AA (4.5:1) against `bg`: ink 11.7–15.0, muted 4.8–7.2, accent 4.8–9.4.

| Colourway | case | bg | ink | muted | rule | accent |
| --- | --- | --- | --- | --- | --- | --- |
| Sage (default) | #A3B18A / 0xA591 | #DDE6D3 / 0xDF3A | #1B2A1A / 0x1943 | #56654F / 0x5329 | #B8C5AC / 0xBE35 | #3F6B30 / 0x3B46 |
| Ivory | #E9E2D0 / 0xEF1A | #F6F2E8 / 0xF79D | #1F1D1A / 0x18E3 | #645E53 / 0x62EA | #D9D1BF / 0xDE97 | #B8410E / 0xBA01 |
| Sky | #94BDE0 / 0x95FC | #E2EDF6 / 0xE77E | #10263A / 0x1127 | #4C6175 / 0x4B0E | #BCD2E3 / 0xBE9C | #1C64A6 / 0x1B34 |
| Peach | #F0A68F / 0xF531 | #FCE8E1 / 0xFF5C | #3A1710 / 0x38A2 | #7E4E43 / 0x7A68 | #EEC4B7 / 0xEE36 | #B83C0C / 0xB9E1 |
| Lilac | #B6A5D8 / 0xB53B | #EDE8F7 / 0xEF5E | #22183A / 0x20C7 | #5F5478 / 0x5AAF | #D2C8E8 / 0xD65D | #5B37C9 / 0x59B9 |
| Charcoal (night) | #2E2E2E / 0x2965 | #1C1D1C / 0x18E3 | #F2F1EC / 0xF79D | #A9A9A2 / 0xAD54 | #3A3B39 / 0x39C7 | #F5B700 / 0xF5A0 |

`case` is only drawn as the swatch on the Status page. Panel colour accuracy on the CYD varies, so the first build step draws all six palettes as stripes for a side-by-side check against the printed case. If a board shows inverted or swapped colours, fix it with the driver's invert or BGR setting, not by editing hex values.

### Components

| Component | Anatomy (relative to its top-left) |
| --- | --- |
| **Tile** (104 × 64) | Label `F_SMALL` caps muted at y+0 · value `F_NUM` ink at y+16 (optional unit `F_BODY` Bold, baseline-aligned, 3 px gap) · sub-line `F_SMALL` muted at y+52 |
| **Hairline** | 1 px `rule`, x 12–228 |
| **Tab bar** | Labels `F_BODY` centred in 54 px cells at y 290; active = Bold ink + 2 px ink underline at y 309, exactly the label's width; inactive = Regular muted |
| **Segmented option** | Same as a tab: options right-aligned at x 228 with 14 px gaps, active Bold ink with 2 px underline 4 px below |
| **Icon** | Drawn from circles, lines and triangles on a 16-unit grid, scaled to 16 or 40 px. Kinds: clear (sun / moon), partly, cloud, rain, storm. Ink body, accent for rain lines and lightning |
| **Load pips** | Three 6 × 6 squares, 2 px apart, muted outline; filled count = load level |

### Touch and gestures

The CYD has resistive touch (XPT2046), which is imprecise and dislikes swipes, so V2 uses taps and long-presses only.

- **Tap**: press and release within 400 ms, moving under 10 px.
- **Long-press**: held 600 ms; fires once on the threshold, without waiting for release, and the UI gives instant feedback.
- Every target is at least 36 × 36 px; tab cells are 60 × 44 (full width split in four, y 276–320).
- **Wake tap**: when the backlight is dimmed, the first touch only wakes the screen and is never passed to the page.
- **Calibration**: long-press the STATUS header for 5 s to run touch calibration; values are stored in NVS.

## Home

Home answers “what time is it, do I need to leave, do I need an umbrella” in one glance, with the focus timer one tap away.

### Layout

| Element | Position | Style | Content |
| --- | --- | --- | --- |
| Clock | x 12, y 10 | `F_DISPLAY` ink | 24 h, no leading zero: `7:42`, `19:05`. Colon static (no blink). Widest case `23:58` ends near x 165 |
| Day | right x 228, y 14 | `F_SMALL` caps muted | `TUE` |
| Date | right x 228, y 30 | `F_TITLE` ink | `17` |
| Month | right x 228, y 56 | `F_SMALL` caps Bold ink | `NOV` (three-letter month). Day, date and month stack like a tear-off calendar page about 28 px wide |
| Sun line | x 12, y 76 | `F_BODY` muted | `Sunrise 6:59 · Sunset 19:05` |
| Week | right x 228, y 78 | `F_SMALL` caps muted | ISO week `W47` (`strftime %V`), sharing a baseline with the sun line |
| Hairline | y 100 | rule |  |
| Tile 1 · Bus | x 12, y 112 | Tile | Label `BUS 72` (favourite service) · value next ETA + unit `min` · sub `next 11 min` |
| Tile 2 · Outdoor | x 124, y 112 | Tile | `29°` (current temperature, rounded) · sub `25–31°` (today's min–max) |
| Tile 3 · Rain | x 12, y 188 | Tile | Highest rain chance over the next 3 hours, `20%` · sub = intensity word (below) |
| Tile 4 · Timer | x 124, y 188 | Tile | `25:00` · sub depends on state (below) |
| Status whisper | x 12, y 262 | `F_SMALL` Medium accent | Hidden normally; shows `● Offline · showing data from 10:21` when any feed is stale |
| Tab bar | y 280–320 |  | Home active |

### Data rules

- **Bus ETA value**: minutes = floor((EstimatedArrival − now) / 60). Show `Arr` in accent when ≤ 0; show `~` before the number in muted when `Monitored` is 0; show `–` with sub `no bus` when there is no NextBus. Recompute every 15 s from the stored timestamps, without refetching.
- **Rain intensity word** (from the strongest WMO weather code in the next 3 hours): no rain code → `dry now`; 51–55 or 61 or 80 → `light`; 63 or 81 → `showers`; 65 or 82 → `heavy`; 95–99 → `storm`.

### Interactions

| Target | Tap | Long-press |
| --- | --- | --- |
| Bus tile | Go to Bus page |  |
| Outdoor or Rain tile | Go to Weather page |  |
| Timer tile | Start, pause or resume | Idle: cycle duration 5 → 10 → 15 → 25 → 45 min. Paused: reset |
| Clock |  | Toggle 12 h / 24 h (saved) |

### Timer states

| State | Value | Sub-line | Colour |
| --- | --- | --- | --- |
| Idle | Chosen duration, `25:00` | `tap to start` | Normal |
| Running | Remaining, `18:42`, updates each second | `tap to pause` | Label and sub in accent |
| Paused | Remaining | `paused · hold to reset` | Value in muted |
| Done | Takeover screen (below) |  | Whole screen accent |

**Timer done takeover:** fill the screen with `accent`; draw `TIME'S UP` (`F_SMALL` caps, bg colour, centred, y 112), the original duration `25:00` (`F_DISPLAY`, bg, centred, y 130) and `Tap anywhere to dismiss` (`F_BODY`, bg, centred, y 200). Backlight goes to High; the screen alternates accent and bg three times at 500 ms; it auto-dismisses after 60 s. The timer keeps running when you leave Home and the takeover appears on any page.

State art: [Home](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-Main), [timer running](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-state-timer-running), [timer done](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-state-timer-done), [offline](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-state-home-offline).

## Weather

Weather shows now, the next six hours and today's air: one big temperature, one strip of hourly rain chances, and two numbers that change behaviour (PSI and UV).

### Layout

| Element | Position | Style | Content |
| --- | --- | --- | --- |
| Header | x 12, y 12 | `F_SMALL` caps muted | `WEATHER · BISHAN` (`LOCATION_NAME`) |
| Age | right x 228, y 12 | `F_SMALL` muted | `4 min ago` / `just now` / `1 h ago` |
| Hero temp | x 12, y 28 | `F_DISPLAY` ink | `29°` |
| Hero icon | x 188, y 34, 40 × 40 | Icon | From current weather code + day/night |
| Condition | x 12, y 88 | `F_BODY` Bold ink | `Partly cloudy` |
| Details | x 12, y 104 | `F_BODY` muted | `Feels 34° · Hum 78% · 25–31°` |
| Hairline | y 124 | rule |  |
| Hourly strip | y 132–219, six 36 px columns from x 12 | see below | Now + next 5 hours |
| Hairline | y 224 | rule |  |
| Stat 1 | x 12, y 232 | Label `F_SMALL` caps muted; value `F_TITLE` ink at y 246 + word `F_BODY` muted | `AIR · PSI` · `52 Good` |
| Stat 2 | x 124, y 232 | as Stat 1 | `UV MAX` · `11 Extreme` |
| Tab bar | y 280–320 |  | Weather active |

**Hourly column** (centre cx = 12 + 36i + 18): time label at y 132 (`F_SMALL`; `Now` in Bold ink, others muted; times as `8p`, `12a`) · 16 px icon at y 148 · temperature at y 168 (`F_BODY` Bold ink) · rain-bar track 12 × 20 at y 186 (`rule`, 2 px radius) with an `accent` bar filled from the bottom, height = round(chance × 20), minimum 2 px · chance at y 208 (`F_SMALL` muted).

### Tap the strip: 5-day view

Tapping anywhere in y 124–224 toggles between **Next 6 hours** and **Next 5 days**. The 5-day view uses five 43 px columns: day name (`Wed`), daily icon, max temperature, and a bar for that day's maximum rain chance. The view resets to hourly when you leave the page.

### Data mapping

| WMO code | Icon | Condition word |
| --- | --- | --- |
| 0 | clear (moon at night) | Clear |
| 1, 2 | partly | Mostly clear / Partly cloudy |
| 3 | cloud | Overcast |
| 45, 48 | cloud | Fog |
| 51, 53, 55, 56, 57 | rain | Drizzle |
| 61 / 63 / 65 (66, 67) | rain | Light rain / Rain / Heavy rain |
| 80 / 81 / 82 | rain | Showers / Heavy showers / Violent showers |
| 95, 96, 99 | storm | Thunderstorm |
| anything else | cloud | Cloudy |

| PSI (24-hr, national) | Word |  | UV index max | Word |
| --- | --- | --- | --- | --- |
| 0–50 | Good |  | 0–2 | Low |
| 51–100 | Moderate |  | 3–5 | Moderate |
| 101–200 | Unhealthy (value turns accent) |  | 6–7 | High |
| 201–300 | Very unhealthy (accent) |  | 8–10 | Very high |
| 301+ | Hazardous (accent) |  | 11+ | Extreme |

Night is decided by the `is_day` flag from the weather API for the hero icon, and by the hour falling between sunset and sunrise for the hourly icons.

State art: [Weather](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-Weather), [5-day view](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-state-weather-5day), [night in Charcoal](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-state-night-weather).

## Bus

Bus is a personal departure board: up to four services per page, the favourite pinned first, each showing when it comes, the two after it, and how full it is.

### Layout

| Element | Position | Style | Content |
| --- | --- | --- | --- |
| Header | x 12, y 12 | `F_SMALL` caps muted | `BUS STOP 01012` |
| Stop dots | right edge x 228, y 14 | 6 px circles, 2 px gap | One per configured stop (max 3); current = ink, others = rule. Hidden if only one stop |
| Stop name | x 12, y 26 | `F_HEAD` ink | `Hotel Grand Pacific` (from config) |
| Page count | right x 228, y 30 | `F_SMALL` muted | `1/2`; hidden when ≤ 4 services |
| Hairline | y 54 | rule |  |
| Rows 1–4 | row top y = 56 + 52i | see below |  |
| Row hairline | y = row top + 51 | rule | Between rows only |
| Tab bar | y 280–320 |  | Bus active |

**Row anatomy** (row top = y): service number at x 12, y+8 in `F_NUM` (accent if it is `FAV_SERVICE`, else ink) · load line at x 12, y+36: three load pips, then `Seats · SD` in `F_SMALL` muted · ETA right-aligned at x 228, y+8: number in `F_NUM` + unit `min` in `F_BODY` Bold · next line right-aligned at x 228, y+36: `then 11 · 19` in `F_SMALL` muted.

### Data rules (LTA DataMall Bus Arrival v3)

| Field | Shows as |
| --- | --- |
| `ServiceNo` | Row number; favourite first, then by soonest ETA |
| `NextBus.EstimatedArrival` | Big ETA. ≤ 0 min → `Arr` in accent, no unit. Empty → row hidden |
| `NextBus2`, `NextBus3` | `then 11 · 19` (minutes only). If both are empty → `last bus` |
| `NextBus.Monitored` = 0 | ETA in muted with `~` prefix; next line reads `scheduled` |
| `NextBus.Load` | `SEA` → 1 pip, `Seats` · `SDA` → 2 pips, `Standing` · `LSD` → 3 pips, `Packed` |
| `NextBus.Type` | `SD`, `DD` or `BD` appended after the load word |

The API returns ISO 8601 times with a +08:00 offset; parse them to epoch seconds once on fetch and store integers. All countdown text is recomputed from those integers every 15 s, so minutes tick down smoothly between fetches.

### Interactions

| Target | Tap | Long-press |
| --- | --- | --- |
| Header (y 0–54) | Next configured stop (wraps) |  |
| Rows area (y 56–264) | Next page of four services (wraps) | Make the service under your finger the favourite (saved) |

### States

- **No buses** (empty `Services` array, usually late at night): centred `No buses right now` (`F_HEAD` ink, y 140), `Tap the header to switch stop` (`F_BODY` muted, y 164), `Checked 19:31` (`F_SMALL` muted, y 180).
- **Fetch failed but old data exists**: keep showing the old rows, counted down from their timestamps, and add the whisper line `● Offline · bus data from 19:21` at y 268 in accent.
- **No key or first boot**: `Add your LTA key in config.h` in the empty-state position.

State art: [Bus](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-Bus), [no buses](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-state-bus-empty).

## Status

Status says whose MakerPulse this is, lets the owner set its look, and shows whether every feed is healthy. It is the only page with settings.

### Layout

| Element | Position | Style | Content |
| --- | --- | --- | --- |
| Header | x 12, y 12 | `F_SMALL` caps muted | `STATUS` |
| Version | right x 228, y 12 | `F_SMALL` muted | `v2.0.0` |
| Product name | x 12, y 26 | `F_TITLE` ink | `MakerPulse` |
| Owner | x 12, y 52 | `F_BODY` muted | `Made by Aisha` (`OWNER_NAME`, the name on the student's Name Cap) |
| Case swatch | circle at x 200–228, y 30–58 | `case` fill, 1 px rule outline | Current colourway's filament colour |
| Hairline | y 76 | rule |  |
| Colourway label | x 12, y 86 | `F_SMALL` caps muted | `COLOURWAY`; current name right-aligned in ink Bold |
| Swatches | centres x 30 + 36i, y 118 | 26 px circles in each `case` colour, 1 px rule outline | Selected gets a 2 px ink ring at 34 px diameter. Hit box 36 × 36 each |
| Brightness | label x 12, y 148; options right-aligned | Segmented option | `Low` `Mid` `High` |
| Night mode | label x 12, y 180; options right-aligned | Segmented option | `Off` `Auto` |
| Hairline | y 208 | rule |  |
| Info rows | y 216, 231, 246, 261 | Label `F_SMALL` muted left; value `F_SMALL` ink right | See below |
| Tab bar | y 280–320 |  | Status active |

### Info rows

| Label | Value example | Notes |
| --- | --- | --- |
| Wi-Fi | `HomeNet · −58 dBm` | `Not connected` in accent when down |
| Synced | `Bus 12s · Wx 4m · Air 32m` | Any feed older than 2× its refresh interval is drawn in accent |
| IP | `192.168.1.42` |  |
| Uptime | `3d 4h · 142 KB free` | Free heap from `ESP.getFreeHeap()` |

### Interactions

| Target | Tap | Long-press |
| --- | --- | --- |
| A swatch | Apply that colourway instantly, full redraw, save to NVS |  |
| Brightness option | Set backlight: Low 40, Mid 140, High 255 (8-bit PWM), save |  |
| Night mode option | Off / Auto, save |  |
| Synced row | Force-refresh all feeds now; row reads `Syncing…` until done |  |
| STATUS header |  | Hold 5 s → touch calibration |

**Night mode Auto:** between today's sunset and the next sunrise, render with the Charcoal palette and Low brightness, whatever colourway is chosen. The chosen colourway returns at sunrise. If sun times are unknown, use 19:00–07:00.

State art: [Status](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-Status) (tap the swatches there to recolour it), [all six colourways](https://claude.ai/artifact/YcMKRUQCx6gmEAwsq8XG3A#artboard-Colourways).

## Shared behaviour and architecture

Network work runs in its own FreeRTOS task, so the screen never freezes during a slow HTTPS call. The UI only reads a shared data snapshot and redraws the regions that changed.

### Data sources and refresh cadence

| Feed | Endpoint | Auth | Refresh | Stale after |
| --- | --- | --- | --- | --- |
| Time | NTP `pool.ntp.org`, `time.google.com`, TZ `SGT-8` | none | Boot, then every 6 h | 24 h |
| Weather | `https://api.open-meteo.com/v1/forecast` (query below) | none | 15 min | 30 min |
| Air (PSI) | `https://api-open.data.gov.sg/v2/real-time/api/psi` | `x-api-key` header | 60 min | 2 h |
| Bus | `https://datamall2.mytransport.sg/ltaodataservice/v3/BusArrival?BusStopCode=<code>` | `AccountKey` header | 30 s on Home or Bus, 120 s on other pages or when dimmed | 3 min |

**Weather query** (one call gives everything both pages need):

```
?latitude=1.351&longitude=103.848
&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,is_day
&hourly=temperature_2m,precipitation_probability,weather_code
&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,uv_index_max,precipitation_probability_max
&timezone=Asia%2FSingapore&timeformat=unixtime&forecast_days=6&forecast_hours=6
```

`timeformat=unixtime` returns plain epoch numbers, so no date parsing is needed for weather.

**PSI:** read the national 24-hour reading from the newest item (`data.items[0].readings.psi_twenty_four_hourly.national`). data.gov.sg now rate-limits its real-time APIs and issues API keys, so get a free key and put it in `config.h`. The first build prompt asks Claude to curl the endpoint once and confirm the exact field path before writing the parser.

**Bus:** the v3 Bus Arrival API needs a free DataMall AccountKey and gives `NextBus`, `NextBus2` and `NextBus3` per service, each with `EstimatedArrival`, `Load`, `Type` and `Monitored`.

### Staleness and offline

- Each feed stores `lastOkEpoch` and `lastErr`. Stale = now − lastOkEpoch > “stale after”.
- Stale data stays on screen. Pages add the accent whisper line; Status colours that feed's age in accent.
- On failure, retry with back-off: 30 s, 60 s, 120 s, then the normal interval.
- Wi-Fi drop: reconnect every 30 s in the network task; the UI keeps running on cached data.

### Backlight and dimming

- Backlight is the PWM pin from v1.2.0 (GPIO 21 on most ESP32-2432S028 boards). Levels: Low 40, Mid 140, High 255.
- After 5 min with no touch, fade to 12 over 1 s. The first touch wakes and is swallowed.
- The timer-done takeover forces High until dismissed.

### Firmware structure

| File | Responsibility |
| --- | --- |
| `config.h` | Everything a student edits: `WIFI_SSID`, `WIFI_PASS`, `LTA_KEY`, `DATAGOV_KEY`, `BUS_STOPS[]` (code + name, max 3), `FAV_SERVICE`, `LAT`, `LON`, `LOCATION_NAME`, `OWNER_NAME`, `DEFAULT_COLOURWAY` |
| `theme.h/.cpp` | `Colourway` struct (6 × uint16\_t), the six palettes, `theme()` accessor, NVS load/save, night override |
| `fonts/` | Generated Inter fonts compiled into flash as arrays (no filesystem upload step) |
| `ui_kit.h/.cpp` | `drawLabel`, `drawValueUnit`, `drawTile`, `drawRule`, `drawTabBar`, `drawSegmented`, `drawIcon`, `drawFixedDigits` |
| `touch.h/.cpp` | XPT2046 read, calibration, tap / long-press detection, wake swallowing |
| `data_model.h` | `AppData` struct + mutex + per-feed `seq` counters that the UI watches |
| `net_task.cpp` + `fetch_*.cpp` | Wi-Fi, NTP, scheduler, one fetcher per feed, ArduinoJson filters |
| `pages/page_*.cpp` | Each page exposes `enter()`, `tick(nowMs)` and `onTouch(evt)` |
| `focus_timer.cpp` | Timer state machine, independent of the page shown |

### Engineering guardrails

- **Memory**: the board has no PSRAM, and a full-screen 16-bit buffer (153.6 KB) will not fit reliably. Redraw per region with small sprites (the clock region is about 18 KB), or use LVGL with a partial draw buffer.
- **TLS**: run one HTTPS request at a time. Stream JSON through an ArduinoJson filter rather than loading whole bodies. Workshop builds may use `setInsecure()`; note that it skips certificate checks.
- **Flash**: fonts, Wi-Fi and TLS can outgrow the default 1.2 MB app slot. Use the “Huge APP (3MB No OTA)” partition scheme.
- **Redraw budget**: clock once per minute, timer once per second while running, bus countdown every 15 s, everything else only when its feed's `seq` changes. Page switch = fill `bg`, draw all regions once.

## Build prompts for Claude Code

Run these one at a time in Claude Code, from the root of your v1.2.0 firmware repo. Flash and check the result on the board before you send the next prompt. Each one is a tracer bullet: it ends with something you can see on the screen.

### Set-up (once)

1. Export this doc as Markdown and save it in the repo as `docs/MAKERPULSE_V2_SPEC.md`.
2. Save the canvas screens as PNGs in `docs/screens/` (`home.png`, `weather.png`, `bus.png`, `status.png`, plus the state screens). Claude Code can open images and compare against them.
3. Add this line to the repo's `CLAUDE.md` (create it if needed): `MakerPulse V2 UI spec: docs/MAKERPULSE_V2_SPEC.md. Screens: docs/screens/. Read both before changing UI code. Log each step in PROJECT_LOG.md.`
4. Start `claude` in the repo and paste Prompt 0.

### Prompt 0 — Audit, no code changes

```text
We are building MakerPulse V2 on top of this repo (v1.2.0, ESP32-2432S028 "CYD", 240x320 portrait).
Read docs/MAKERPULSE_V2_SPEC.md and look at every image in docs/screens/.

Do NOT change any code yet. Report back:
1. Build system (Arduino IDE or PlatformIO), board and partition settings, library versions.
2. Graphics stack: TFT_eSPI, LVGL, or something else? Display driver (ILI9341 or ST7789), rotation, colour order and invert settings, SPI frequency.
3. Touch: library, pins, how calibration is stored.
4. Where WiFi, NTP, weather, PSI and bus fetching happen today; which endpoints and JSON libraries are used; whether fetching blocks the UI.
5. File map: what each file does, and which parts can be reused for V2.
6. Any v1.2.0 behaviour the spec drops or changes.

Then propose a migration plan that follows the spec's "Firmware structure" table and keeps the existing graphics library.
List every assumption, and ask me about anything the spec leaves open. Do not add features the spec does not describe.
```

### Prompt 1 — Theme, fonts and the colour check

```text
Implement the foundation from the spec's "Design system" section. Keep the existing graphics library.

1. config.h: create the fields listed in the spec's "Firmware structure" row, with placeholder values and comments a 12-year-old can follow. Keys and passwords live in this file as constants.
2. theme.h/.cpp: a Colourway struct {name, caseC, bg, ink, muted, rule, accent} as uint16_t RGB565, with the six palettes using the exact RGB565 values from the Colourways table. Add theme(), setColourway(index) and NVS persistence (Preferences, namespace "mp").
3. fonts/: generate Inter fonts for F_DISPLAY 56 Bold, F_NUM 28 Bold, F_TITLE 22 Bold, F_HEAD 17 Bold, F_BODY 13 Regular and 13 Bold, F_SMALL 11 Medium. Subset glyphs exactly as the Type scale table says. Compile them into flash as arrays (TFT_eSPI smooth-font arrays, or lv_font_conv output for LVGL). Write a short script, tools/make_fonts, so the fonts can be rebuilt. Switch to the "Huge APP (3MB No OTA)" partition scheme if flash runs short.
4. ui_kit: drawLabel (supports small caps with +1 px tracking), drawValueUnit (baseline-aligned unit, 3 px gap), drawTile, drawRule, drawFixedDigits (every digit centred in a cell as wide as "0" so the clock never jitters), drawIcon(kind, x, y, size, night) built only from circles, lines and triangles on the spec's 16-unit grid.
5. A boot-time test screen, behind #define COLOUR_TEST: six horizontal stripes, one per colourway, each showing bg, ink text, muted text, a rule line and an accent block, labelled with the colourway name.

Acceptance: it compiles; COLOUR_TEST shows all six palettes with the right colours (no swapped red/blue, not inverted); a demo screen draws "7:42" in F_DISPLAY and one tile that match docs/screens/home.png at the same coordinates.
Stop and tell me what to check on the board.
```

### Prompt 2 — Touch, tab bar and page router

```text
Implement navigation from the spec's "Touch and gestures" section and the tab bar component.

1. touch.h/.cpp: emit TouchEvt {type: TAP | LONG_PRESS, x, y}. A tap is under 400 ms and moves under 10 px. A long-press fires once at 600 ms without waiting for release. Reuse v1.2.0 pins and calibration. Long-pressing the STATUS header for 5 s runs calibration and saves the result to NVS.
2. Page interface: struct Page { void (*enter)(); void (*tick)(uint32_t nowMs); void (*onTouch)(const TouchEvt&); }. Add a router with goTo(pageId) that fills bg, draws the tab bar, then calls enter().
3. drawTabBar(active): four 54 px cells from x 12, labels at y 290 in F_BODY, active Bold ink with a 2 px underline at y 309 as wide as the label, inactive Regular muted. Touch strip y 276-320 split into four equal columns.
4. Four placeholder pages that draw their small-caps header at y 12 and nothing else.
5. Temporary: a serial command "cw 0-5" switches colourway and redraws.

Acceptance: tapping each tab switches page with no flicker beyond one bg fill; the underline matches docs/screens/*.png; serial "cw 3" recolours everything to Peach.
Update PROJECT_LOG.md with what changed and anything that failed along the way.
```

### Prompt 3 — Data layer

```text
Implement the spec's "Shared behaviour and architecture" data layer. No UI changes except the serial log.

1. data_model.h: AppData holds Time, Weather (current, 6 hourly, 6 daily), Air (psi24 national), Bus (up to 3 stops x up to 12 services, each with three arrivals stored as epoch seconds plus load, type and monitored). Each feed has lastOkEpoch, lastErr and a seq counter that goes up on every successful update. Guard it with one FreeRTOS mutex; the UI copies what it needs under the lock.
2. net_task: a FreeRTOS task pinned to core 0. It handles WiFi connect and reconnect (every 30 s), NTP with TZ "SGT-8", and a scheduler that uses the refresh intervals and 30/60/120 s back-off from the spec. It makes one HTTPS request at a time.
3. Fetchers, each using an ArduinoJson filter and streaming the body (useHTTP10):
   - fetch_weather: the exact Open-Meteo query in the spec, with timeformat=unixtime.
   - fetch_psi: before writing the parser, curl the endpoint once with my DATAGOV_KEY and show me the real JSON, then confirm the field path for the national 24-hour PSI.
   - fetch_bus: LTA DataMall v3 BusArrival with the AccountKey header. Parse EstimatedArrival (ISO 8601, +08:00) to epoch seconds once. An empty string means no bus.
4. Expose busPageVisible / dimmed flags so the bus interval switches between 30 s and 120 s.
5. Serial log, one line per fetch: feed, HTTP status, bytes, ms taken, free heap, key values.

Acceptance: 30 minutes running with no reboot; the serial log shows weather every 15 min and bus every 30 s; free heap stays flat (print the minimum); unplugging the router shows back-off and recovery.
Log results in PROJECT_LOG.md.
```

### Prompt 4 — Home page and focus timer

```text
Build the Home page exactly as the spec's "Home" section and docs/screens/home.png describe, using the ui_kit from Prompt 1 and data from Prompt 3.

- Use every coordinate and style in the Home layout table: clock, the TUE / 17 / NOV calendar stack, the sun line with W47 on its right, the hairline at y 100, the four tiles and the offline whisper line.
- Bus tile: favourite service from FAV_SERVICE, countdown rules from "Data rules" (Arr in accent, ~ and muted when not monitored, a dash with "no bus"). Recompute every 15 s from stored epochs.
- Rain tile: highest precipitation_probability in the next 3 hours plus the intensity word mapping.
- focus_timer.cpp: the state machine from "Timer states" (idle, running, paused, done) with the 5/10/15/25/45 duration cycle on long-press, resetting on long-press while paused, and the done takeover (accent fill, three 500 ms flashes, High backlight, auto-dismiss after 60 s, shows on any page). Save the chosen duration in NVS.
- Tap Bus tile -> Bus page; tap Outdoor or Rain -> Weather page; long-press clock toggles 12/24 h.
- Redraw rules: clock region once a minute, timer region once a second only while running, tiles only when their feed's seq changes or on the 15 s bus tick. Use small sprites or partial redraw per region, never a full-screen buffer.

Acceptance: side by side with docs/screens/home.png and state-timer-running.png, every element is within 1-2 px; the clock never flickers or jitters at 23:59 -> 0:00; the timer still fires while I am on another page.
```

### Prompt 5 — Weather page

```text
Build the Weather page from the spec's "Weather" section and docs/screens/weather.png.

- Header with LOCATION_NAME and a relative age ("just now", "4 min ago", "1 h ago").
- Hero: F_DISPLAY temperature, 40 px icon at x 188 y 34 chosen from weather_code and is_day, then the condition word and the details line.
- Hourly strip: six 36 px columns, with the exact y positions from the "Hourly column" paragraph. Rain bars fill from the bottom in accent over a rule-coloured track.
- Tapping y 124-224 toggles to the 5-day view (five 43 px columns: day name, icon, max temperature, max rain chance). It resets to hourly on page exit.
- PSI and UV stats with the band words from the tables; PSI over 100 turns the value accent.
- Use the WMO mapping table exactly; unknown codes fall back to cloud / "Cloudy".

Acceptance: matches weather.png, state-weather-5day.png and (with night mode on) state-night-weather.png.
```

### Prompt 6 — Bus page

```text
Build the Bus page from the spec's "Bus" section and docs/screens/bus.png.

- Header: stop code in small caps, stop dots (hidden if one stop), stop name, page count.
- Rows at y = 56 + 52i with the row anatomy from the spec. The favourite is pinned first and its number is drawn in accent; the rest are sorted by soonest ETA. Hairlines go between rows only.
- Data rules table: Arr, ~ for unmonitored, "scheduled", "last bus", load pips and words, bus type.
- Tap header -> next stop; tap rows -> next page of four; long-press a row -> that service becomes the favourite (saved to NVS, Home updates too).
- States: no buses, offline with old data (keep counting down, whisper line at y 268), and a missing key.
- While this page is visible, bus refresh is 30 s; the countdown text re-renders every 15 s without refetching.

Acceptance: matches bus.png and state-bus-empty.png; Arr appears exactly when ETA <= 0; switching stops shows cached data instantly and then refreshes.
```

### Prompt 7 — Status page, settings, night mode, dimming

```text
Build the Status page from the spec's "Status" section and docs/screens/status.png, then the global behaviours that depend on it.

- Identity block: MakerPulse, "Made by " + OWNER_NAME, and the case swatch.
- Colourway picker: six 26 px swatches in case colours, a 2 px ink ring on the selected one, 36x36 hit boxes. A tap applies the colourway instantly (full redraw) and saves it to NVS. Remove the temporary serial "cw" command.
- Brightness Low/Mid/High (PWM 40/140/255) and Night mode Off/Auto as segmented options, saved to NVS.
- Info rows: Wi-Fi SSID and RSSI, per-feed age (any stale feed in accent), IP, uptime and free heap. Tapping the Synced row forces a refresh of every feed.
- Night mode Auto: between sunset and sunrise, render with Charcoal and Low brightness without changing the saved colourway; fall back to 19:00-07:00 if sun times are missing.
- Dimming: after 5 min idle, fade to 12 over 1 s; the first touch only wakes. The timer-done takeover forces High.

Acceptance: matches status.png and status in Charcoal; the chosen colourway survives a power cycle; night mode switches at the real sunset time.
```

### Prompt 8 — Hardening and handover

```text
Review the whole V2 firmware against docs/MAKERPULSE_V2_SPEC.md and fix what you find.

1. Go through every table in the spec and list each item as Done / Different / Missing. Fix the Missing and Different ones, or explain why not.
2. Run a 12-hour soak (or simulate it with shortened intervals): report minimum free heap, the largest free block, and any WiFi or TLS failures.
3. Check that no code path does a full-screen redraw except page changes, colourway changes and the timer takeover.
4. Edge cases: no WiFi at boot, NTP not yet synced (show "--:--"), empty bus response, PSI API down, midnight rollover, 12 h mode.
5. Write README_V2.md for workshop teachers: how to fill in config.h, get the LTA and data.gov.sg keys, pick a colourway, flash, and calibrate touch.
6. Finish PROJECT_LOG.md with what changed from v1.2.0, what failed and why, and known limits.
```

## On-device acceptance checklist

Tick these on a real board, held at desk distance, before calling V2 done.

- [ ] All six colourways look right beside their printed cases; muted labels are readable in Sage and Peach
- [ ] The clock reads from about 2 m away; digits do not shift sideways as they change
- [ ] TUE / 17 / NOV never touches the clock, even at 23:58
- [ ] Every tap target works first time with a fingertip (tabs, tiles, swatches, segmented options)
- [ ] The bus countdown ticks down between fetches; `Arr` appears in accent at 0
- [ ] The timer keeps running across pages; the takeover appears wherever you are and dismisses on tap
- [ ] Pulling the Wi-Fi shows the offline line within one stale window, with old data still on screen
- [ ] Night mode switches to Charcoal at sunset and back at sunrise; the chosen colourway is kept
- [ ] The screen dims after 5 min; the wake tap does not also press a button
- [ ] Colourway, brightness, favourite service, timer length and 12/24 h survive a power cycle
- [ ] 12 hours powered on: no reboot, and free heap is flat

**Open questions for you:** which bus stops and favourite service the workshop default should ship with, and whether students get their own data.gov.sg key or share a TYM one.

## Sources

- [LTA DataMall dynamic datasets (Bus Arrival)](https://datamall.lta.gov.sg/content/datamall/en/dynamic-data.html)
- [data.gov.sg real-time API developer guide](https://guide.data.gov.sg/developer-guide/real-time-apis)
- [Open-Meteo forecast API docs](https://open-meteo.com/en/docs)
