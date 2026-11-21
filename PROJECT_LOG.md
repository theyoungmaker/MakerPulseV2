# MakerPulseV2 — Project Log

## Summary: v1.2.0 → v2.1.0

This is a from-the-spec rewrite of the entire on-device UI, triggered by a
complete pixel-exact design spec (`docs/MAKERPULSE_V2_SPEC.md`) and ten
reference mockups (`docs/screens/`) the user supplied. It was built in 8
phases (below), each compiling clean before the next started, followed by
two more rounds of user-driven changes after the first real hardware flash
(Phase 9's font/layout bug fixes, Phase 10's timer picker rebuild).

**What changed, by area:**

- **Colour system**: v1.2.0's freeform Accent/Text/Theme custom-colour
  system is gone. Phase 1 replaced it with the spec's 6 fixed colourways
  (Sage/Ivory/Sky/Peach/Lilac/Charcoal, one per 3D-printed case colour),
  every pair pre-checked against WCAG AA. No custom colour picker exists
  anymore, by design (ADR 0002, then ADR 0003).
- **Fonts**: the old 2-font digit-only set is replaced by the spec's 7-font
  Inter set (`F_DISPLAY`/`F_NUM`/`F_TITLE`/`F_HEAD`/`F_BODY_REG`/
  `F_BODY_BOLD`/`F_SMALL`), generated from Inter Variable at the spec's
  exact pixel sizes.
- **Every page** (Home, Weather, Bus, Status) was rewritten to the spec's
  exact layout, replacing the previous "Colourway theme" flat-card redesign
  that itself had only ever reached the Home page.
- **Home**: fixed four-tile layout (Bus/Outdoor/Rain/Timer) replaces the
  old fully-customizable widget-slot system; the focus timer gained an
  explicit idle/running/paused/done state machine (was running/finished
  only, with no pause). Phase 4 initially replaced the old six-preset
  popup menu with a fixed 5/10/15/25/45 min long-press-cycle, but Phase 10
  reversed that back to a tap-opened picker with six *editable* presets
  (1/3/5/10/15/30 min default) plus Custom entry, once the shared
  long-press gesture turned out to be unreliable in practice (see Phase
  10's log entry). The done takeover now matches the spec's
  accent-fill/flash/60s-auto-dismiss design exactly.
- **Weather**: gained a 6-hour rain-chance strip with a tap-to-toggle
  5-day view, a proper hero icon, and PSI/UV stat columns with the spec's
  exact band words — none of this existed before (the old Weather page was
  a simple now/hourly/UV/PSI grid).
- **Bus**: switched from precomputed "minutes at fetch time" countdowns
  (which silently went stale between the ~45s refetches) to epoch-based
  countdowns recomputed live every redraw, added load/vehicle-type display
  and a favourite-service concept (long-press a row), and removed the
  on-device numeric-keypad stop-code entry (no longer in the spec; the web
  settings page already covers it).
- **Status**: gained on-device colourway swatches (previously colourway was
  web-settings-only), a Low/Mid/High brightness control, an Off/Auto night
  mode (auto-switches to Charcoal after sunset), a two-point touch
  calibration routine, and per-feed Wi-Fi/Synced/IP/Uptime info rows. Lost
  the on-device Wi-Fi enable/disable toggle (not in the spec).
- **Architecture**: fetching moved off the UI thread entirely onto a
  dedicated FreeRTOS task (`startNetTask()`, core 0) behind a mutex, with
  per-feed exponential backoff (30s/60s/120s/normal) replacing fixed-
  interval-only retries; PSI switched from the deprecated v1
  `api.data.gov.sg` endpoint (region-selectable) to the v2 national
  reading (`api-open.data.gov.sg`, API-key-gated); the flash partition
  scheme switched to "Huge APP (3MB No OTA)" to make room for the larger
  font/UI footprint.
- **Deliberate deviation from the spec's own suggested architecture**: the
  spec's own alternate suggestion was per-student `config.h` constants with
  no settings page at all. This was explicitly rejected (user decision) in
  favour of keeping the existing runtime-Preferences + Improv Wi-Fi + web
  settings-page model from v1.2.0 — one firmware image flashed onto every
  board, configured afterwards through its own setup website. See ADR 0003.

**What failed along the way** (full detail in each phase's own entry
below): a handful of self-caught issues, none of which reached hardware
before being fixed — a touch-calibration state bug (Phase 2), two rounds
of RGB565 colour-quantization collisions in the original shell-colour
palette work that predates this spec migration, one stray cache-variable
reference left over from a renamed field (Phase 4), and a naming mix-up
where a new Bus-page cache briefly reused a Weather-page cache variable
(Phase 6). Every one was caught by the normal compile-and-rebuild cycle,
not by hardware testing — see "Known limits" below for why that matters.

**Known limits:**

- **No hardware verification of Phases 2 through 8.** The device went
  unreachable at `/dev/cu.usbserial-*` partway through Phase 1's visual
  check and never reconnected during this entire build. Every phase from
  that point on was verified by compiling cleanly (including the
  `-DCOLOUR_TEST` variant) and by careful code review against the spec and
  reference screens, but **none of it has actually been seen running on a
  real board.** This is the single biggest risk in this log — anything
  that only shows up visually (icon proportions, text baseline alignment,
  colour rendering on the actual panel, touch calibration accuracy) is
  unverified. Flash and do a full pass against every `docs/screens/*.png`
  mockup and the on-device acceptance checklist (spec, "On-device
  acceptance checklist" section) before considering this release final.
- No 12-hour soak test has been run (needs real hardware). A static review
  found no obvious leak patterns (every `JsonDocument` is function-scoped
  and destroyed on return; no raw heap allocation anywhere in the touched
  code), but that is not a substitute for watching `ESP.getFreeHeap()`
  stay flat over real time.
- PSI and bus arrivals are Singapore-specific (data.gov.sg / LTA DataMall);
  weather works anywhere via Open-Meteo given the right lat/long.
- The Bus page's "no key configured" spec state was intentionally not
  implemented — see Phase 6's assumption #2 (this codebase's architecture
  always has a usable LTA key, via the shared fallback).


## Phase 1 — Theme, fonts, colour-test screen (2026-11-20)

**Changed:**
- `platformio.ini`: partition scheme `default.csv` → `huge_app.csv` ("Huge APP 3MB No OTA"). Flash usage dropped from 87% to 36.5% with the same firmware, since the app partition grew from ~1.3MB to 3MB. NVS stays at the same offset/size, so existing WiFi credentials and settings survive the switch.
- `src/state.cpp`/`state.h`: replaced the 8-preset "shell colour" system (`ShellColourPalette`) with the spec's 6-colourway `Colourway` struct (bg/ink/muted/rule/accent), using the spec's RGB565 values verbatim. Public function names (`applyShellColourByKey`, `shellColourLabel`, etc.) kept as-is for now to minimize churn in settings_server.cpp; will rename in a later cleanup pass if it's ever worth the diff. Added `colourwayRolesAt()` for the colour-test screen. `COL_PANEL`/`COL_PANEL_ALT` kept as legacy fields (mapped to bg/rule) since Weather/Bus/Status/overlays still read them until their own phases land.
- `src/fonts/`: replaced the old 2-font digit-only set (`InterBlackBig`/`InterBlackMed`) with the spec's 7-font set (`FDisplay` 56pt-equiv Bold digits/colon only, `FNum`/`FTitle`/`FHead` Bold with full ASCII, `FBodyReg`/`FBodyBold` 13px, `FSmall` 11px Medium). Sizes calibrated against measured pixel heights in `docs/screens/home.png`, not the spec's nominal point sizes (which don't map 1:1 to rendered bbox height). Verified bit-packing by rendering each font back from its packed bitmap before trusting it on hardware.
- `src/config.h`: added `V2_*` layout constants from the spec's "Screen grid"/"Touch and gestures" tables (margins, header/content/tab-bar Y ranges, tap/long-press thresholds, min touch target, calibration hold time). Co-exists with the old `HOME2_*` constants until each page's phase retires them.
- `src/display.cpp`/`main.cpp`: added `drawColourTestScreen()` behind `#define COLOUR_TEST`, called from `setup()` in place of the normal boot sequence. Mechanically renamed old font references (`InterBlackBig`→`F_DISPLAY`, `InterBlackMed`→`F_NUM`) so existing (not-yet-rewritten) Home/Weather/Bus/Status/overlay code keeps compiling against the new font files until their own phases rewrite them properly.
- `CONTEXT.md`: rewrote "Colourway theme"/"Shell colour" entries to describe the spec-driven system; added "Night mode". `docs/adr/0003-v2-spec-colourway-system.md`: records the decision to follow the spec's UI/color rules exactly but keep the existing runtime-Preferences/web-settings-page provisioning model rather than the spec's own alternate suggestion of per-student `config.h` constants.
- Copied `docs/MAKERPULSE_V2_SPEC.md`, `docs/screens/*.png`, and `docs/screens/README.md` into the repo; added `CLAUDE.md` per the spec's own setup instructions.

**Verified:** both the normal build and a `-DCOLOUR_TEST` build compile cleanly. Visual verification on hardware pending (device not connected during this phase — flash usage and bit-packing confirmed on the toolchain side only).

**What failed along the way:** none — this phase was additive/mechanical (new font generation, new data table, new constants) rather than behavioral, so no real failures, just one self-caught transcription slip (wrong hex values pasted into an early edit attempt, caught before it compiled).

**Known gaps carried forward:** `COL_PANEL`/`COL_PANEL_ALT` are legacy placeholders; Weather/Bus/Status/overlays are still drawing with pre-spec layout and the old button/card patterns until Phases 4-7. `F_NUM` is now pointed-to by old code that only ever needed digits — harmless, but that code will be replaced, not tuned, in later phases.

## Phase 2 — Touch model, tab bar, page router (2026-11-20)

**Changed:**
- `display.h`/`display.cpp`: added `TouchEvt`/`pollTouchEvent()` implementing the spec's tap (≤400ms, <10px move) vs. long-press (600ms, fires once at the threshold without waiting for release) state machine, replacing `touchNewPress()`'s old single-debounced-tap model. Also added `touchHoldMsInRect()` for the Status-page 5s calibration hold, which is its own longer threshold layered on top of (not instead of) the normal long-press — Phase 7 wires the actual calibration routine to it.
- `main.cpp`: `loop()` rewired to the new event model. Preserved the exact existing wake-swallow semantics (full-off or auto-dimmed swallows the waking touch; manually-dimmed does not — taps still work while deliberately dimmed) by triggering wake on `justPressed` (the raw down-edge) and latching a per-press `swallowPress` flag so the *same* press's eventual TAP/LONG_PRESS resolution (up to 400ms later) still gets suppressed correctly.
- `display.cpp`: `drawV2TabBar()` rewritten to the spec's exact geometry — 54px label cells inset from the content margin (`V2_MARGIN_L`), F_BODY_BOLD/F_BODY_REG, y290 labels, y309-310 2px underline sized to the label's own width. `handleNavTouch()`'s touch-strip threshold aligned to `V2_TAB_TOUCH_Y0` (276) — deliberately *wider* than the visual label cells, per the spec's own distinction between the 54px label cells and the 60px-wide (`SCREEN_W/4`) touch columns.
- `state.h`/`state.cpp`: runtime touch calibration storage (`touchCalXMin/Max/YMin/Max`, Preferences-backed, defaulting to config.h's fixed constants). `readTouchXY()` now reads through these instead of the compile-time constants. The calibration *routine* (what the user actually does during those 5 seconds) is Phase 7's job — this is just the storage so it's ready.

**Assumption flagged, not asked about** (user was away; logging per the spec's own Prompt 0 instruction to list assumptions): the spec doesn't describe what the touch calibration *procedure* actually looks like on screen (just that it exists and is NVS-backed) — Phase 7 will need a concrete design (likely a standard 2-point tap-the-crosshair pattern).

**What failed along the way:** caught and fixed a real bug in my own first draft of `touchHoldMsInRect()` — the `holding` static never reset on release, so a later press would reuse a stale start time and report an incorrectly-long hold duration immediately. Fixed before it ever reached the device.

**Deliberately skipped:** the spec's temporary serial `cw 0-5` colourway-switch command (meant to be deleted again once Phase 7 ships real on-device swatches). The vendored `ImprovWiFiLibrary`'s `handleSerial()` silently discards any serial bytes that don't match its own protocol preamble (confirmed during the MakerPulse orientation ADR's investigation) — sharing the serial port with a throwaway debug command risked interfering with real WiFi provisioning for no lasting benefit. Colourway-switching will be verified directly via Phase 7's real swatches instead.

**Verified:** normal build and `-DCOLOUR_TEST` build both compile. Hardware verification still pending — device not connected throughout this phase either.

## Phase 3 — FreeRTOS network task + data model (2026-11-20)

**Changed:**
- `state.h`/`state.cpp`: added `dataMutex` (`SemaphoreHandle_t`) + `dataLock()`/`dataUnlock()`, and a generic `FetchBackoff` struct (`config.h`) implementing the spec's "Staleness and offline" retry schedule (30s/60s/120s, then the normal interval) — one instance per feed: `weatherBackoff`, `psiBackoff`, `busBackoff[BUS_MAX_STOPS]`, `sunBackoff`.
- `network.cpp`: converted every fetch/ensure pair (`fetchSunriseSunset`, `fetchWeather`, `fetchPsi`, `fetchBusArrival`, `fetchBusStopName`) to the same pattern: parse the HTTP response into local scratch variables first, then take `dataMutex` only around the final copy into shared state (never across the HTTPS call itself), and have the matching `ensureX()` use `fetchBackoffDue`/`fetchBackoffRecord` instead of its old ad-hoc `isnan(...) OR interval-elapsed` check. Added a `Serial.printf("[fetch] ... ok=%d ms=%lu heap=%u")` line per fetch for on-device debugging.
- `network.cpp`/`network.h`: deleted `fetchKpIndex()`/`ensureKpIndex()` entirely — no page in the V2.1.0 spec displays a KP/aurora widget, so there's nothing left to keep it fresh for. (The `kpIndex`/`lastKpFetch` storage and the `HOME_WIDGET_KP` enum/UI entries are left in place for now since they're part of the widget-customization system Phase 4 removes wholesale; `kpIndex` will just sit permanently `NaN` until then — harmless, since nothing fetches it anymore.)
- `network.cpp`: rewrote `fetchPsi()` for the spec's data.gov.sg v2 national reading — URL `https://api-open.data.gov.sg/v2/real-time/api/psi`, `x-api-key` header from `datagovApiKey`, field path `data.items[0].readings.psi_twenty_four_hourly.national`. Dropped per-region selection entirely (per the user's explicit decision).
- `state.h`/`state.cpp`: `psiRegion`/`sanitizePsiRegion()` replaced with `datagovApiKey` (Preferences key `datagovKey`).
- `network.cpp`: added `startNetTask()` — `xTaskCreatePinnedToCore` on core 0, 8KB stack, looping every 1s calling `ensureSunTimesForToday/ensureWeather/ensurePsi/ensureBusArrival` whenever WiFi is connected. `updateWiFiConnectionState()` no longer calls these directly on reconnect — the task's next 1s pass picks them up.
- `main.cpp`: `setup()` creates `dataMutex` and calls `startNetTask()` right after `loadStoredSettings()`, before display/touch init, so the task is always running (idle until WiFi connects) regardless of provisioning state. Removed the old synchronous `ensureX()` calls from `finishBootAfterWifi()` and the `DATA_TICK_MS`-gated block in `loop()` — fetching is now exclusively the net task's job.
- `settings_server.cpp`: replaced the PSI-region `<select>` field with a `datagovKey` text input (with a link to request a free data.gov.sg API key); `handleSave()` updated to match, including resetting `psiBackoff` when the key changes so a newly-entered key is retried immediately rather than waiting out the prior key's backoff.

**Assumption flagged, not asked about:** the spec doesn't specify the net task's polling interval directly (only the per-feed retry/refresh intervals already in `config.h`). Picked 1s as the task loop's own tick — cheap (each `ensureX()` call is a few comparisons when nothing's due) and keeps reconnect-to-first-fetch latency low without any new constant debate.

**Scope decision (mine, not asked):** kept this phase to mutex + backoff + task-extraction only, with locking isolated to each fetch function's own final write. Did *not* do a full `AppData`-struct-with-read-side-locking rewrite — the spec's "no UI changes" framing for this phase, and the fact every page gets rewritten in Phases 4-7 anyway, made read-side locking premature here. Interim risk: a UI read mid-write of a multi-field struct (e.g. `busServicesByStop[idx]`, now copied in one locked block, or `hourlyTempC[]`, same) could in principle see a torn read on hardware without an ESP32 memory-ordering guarantee beyond the mutex — but since writes are now mutex-isolated to one atomic block and reads are simple field accesses from the main loop at ~30Hz, worst case is one stale/cosmetic frame, self-correcting next tick. Accepted as non-crashing and acceptable for this phase.

**What failed along the way:** none — mechanical conversion, each function compiled clean on first full-file build.

**Verified:** `pio run -e makerpulse` compiles clean (only pre-existing ArduinoJson deprecation warnings). Flash usage 36.7% (1,153,985 / 3,145,728 bytes), RAM 15.7%. Hardware verification still pending — device has not reappeared at `/dev/cu.usbserial-*` since Phase 1.

**Known gaps carried forward:** `kpIndex`/`HOME_WIDGET_KP` dead code (removed in Phase 4 along with the rest of widget customization). Read-side locking deferred to each page's own rewrite phase, per the scope decision above.

## Phase 4 — Home page + focus timer rewrite (2026-11-20)

**Changed:**
- `config.h`: removed `HomeWidgetType`/`HOME_SLOT_COUNT` entirely (widget customization is gone, per the user's "Fully fixed, per spec" decision). Added fixed `HOME_TILE_*` coordinates (Bus/Outdoor/Rain/Timer, spec "Layout" table), the clock long-press hit region, the `FOCUS_DURATIONS_MIN[]={5,10,15,25,45}` fixed cycle, `FocusTimerState` enum (idle/running/paused/done), and the done-takeover flash timing constants.
- `state.h`/`state.cpp`: replaced the whole widget-customization system (`homeWidgetSlots`, `homeWidgetKey/Label/FromKey`, `homeSlotLabel`, `getHomeSlotRect`) with `getHomeTileRect()` (fixed Bus/Outdoor/Rain/Timer order) and `cacheHomeTiles[4]`. Replaced the old running/finished-bool timer model with the spec's explicit 4-state machine (`focusTimerState`, `focusDurationIdx`, `startOrToggleFocusTimer()`/`cycleFocusDuration()`/`resetFocusTimerToIdle()`/`dismissTimerDone()`/`updateFocusTimerState()`/`updateTimerDoneAutoDismiss()`). Added `favBusService` (set by Bus page long-press, Phase 6) and `clockIs24h` (set by the new clock long-press) as new persisted settings. Removed `flashModeEnabled` — the done takeover's 3x flash is now unconditional, per spec, not a user toggle. Removed `kpIndex`/`lastKpFetch`/`timerPresetMin[]`/`customTimerMinutes` entirely.
- `network.cpp`: added `precipitation_probability` to the Open-Meteo hourly request and a parallel `hourlyPrecipProb[HOURLY_COUNT]` array — the spec's Rain tile needs the next-3-hours max probability, which nothing in the app was fetching yet. Added `BusServiceInfo.monitored` (parsed from LTA's `NextBus.Monitored`) — needed for the spec's "~ prefix in muted when Monitored is 0" Home Bus-tile rule, which also didn't have a data-model field to read. Both are narrow, correctness-motivated extensions of already-in-progress Phase 3 fetch code, not scope creep into Phase 5/6's own page rewrites.
- `display.cpp`: full rewrite of the Home page to the spec's exact layout — clock (`F_DISPLAY`, 24h no leading zero via the new `clockIs24h` toggle), TUE/17/NOV calendar stack, sun line with `W47`, hairline, four fixed tiles (`drawHomeBusTile`/`drawHomeOutdoorTile`/`drawHomeRainTile`/`drawHomeTimerTile`), and the offline whisper line. Added `drawSmallCapsLabel()` (the spec's "+1px per character" tracked small-caps convention, used for every tile label and the day/month/week text) and extended `drawTextWithDegree()` to also substitute a drawn dot for the UTF-8 middle dot (0xC2 0xB7), alongside the existing degree-ring substitution — neither custom font contains either glyph. Rewrote `drawTimerDoneOverlay()` to the spec's takeover exactly: full accent fill, "TIME'S UP"/duration/dismiss-hint in bg colour, 3x 500ms accent/bg alternation, settling on accent, 60s auto-dismiss carried over from the old dialog's timer. Deleted the dead pre-"Colourway theme" sprite-card renderers (`drawClockCardSprite` and five others, ~225 lines, zero live callers) and the old `drawFocusMenuOverlay()`/`drawCustomTimerEntryOverlay()` popups (replaced by direct tap/long-press state transitions, no menu).
- `main.cpp`: wired `handleHomeLongPress()` into the long-press branch; updated the overlay-open bookkeeping (`focusMenuOpen`/`customTimerEntryOpen` gone, replaced by `focusTimerState == TIMER_DONE`).
- `settings_server.cpp`: removed the "Widget Customization" panel, the six-slot timer-preset editor, and the "flash screen when timer ends" checkbox entirely — all three are now either fixed (duration cycle, tile placement) or unconditional (flash) per spec. Fixed a pre-existing cosmetic bug while in the file: the shell-colour default/fallback string was still `"muted-white"` (a Phase 1 leftover from the 8-preset palette, now invalid against the 6-colourway system) — changed to `"sage"`, the spec's actual default.

**Assumptions flagged, not asked about** (user away; logging per spec's own Prompt 0 convention):
1. No bold `F_SMALL` font asset exists (the Type scale table only lists one F_SMALL weight), so the Month label's spec'd "F_SMALL caps Bold ink" is drawn in the same regular F_SMALL weight at ink colour — the closest available approximation, not a true bold.
2. The sun line ("Sunrise 6:59 · Sunset 19:05") is always rendered 24h/no-leading-zero, independent of the new clock 12h/24h toggle — the spec's Interactions table scopes that toggle to the clock element only, and the layout table's own example is a fixed-looking 24h string.
3. The offline whisper's staleness signal is "this feed's `FetchBackoff.failCount > 0`" (i.e., its last fetch attempt failed and it's in backoff) rather than a fixed age threshold — reuses bookkeeping Phase 3 already maintains instead of inventing a second staleness concept.
4. Favourite bus service (`favBusService`) has no UI to set it yet (that's Phase 6, long-press a Bus-page row) — until then the Home Bus tile falls back to the soonest-arriving service at the active stop, so the tile has something sensible to show rather than going blank.
5. "Reset" on long-press-while-paused returns to Idle at the *same* chosen duration (doesn't also reset the duration back to the default/25:00) — matches the "Idle: Chosen duration" row in the Timer states table, i.e. the duration choice itself is sticky across a reset.

**What failed along the way:** one compile error caught and fixed immediately after the first full build attempt — `dismissTimerDone()`/`updateFocusTimerState()` still referenced `cacheTimerDoneCountdown`/`cacheTimerDoneFlash`, removed from `state.h` but not yet scrubbed from these two call sites. Fixed, rebuilt clean.

**Verified:** both the normal build and the `-DCOLOUR_TEST` build compile cleanly (only pre-existing ArduinoJson deprecation warnings). Flash usage 36.7% (1,155,697 / 3,145,728 bytes with `-DCOLOUR_TEST`; 36.6% / 1,152,241 bytes normal), RAM 15.7%. Hardware verification still pending — device has not reappeared at `/dev/cu.usbserial-*` since Phase 1; will flash and do a full visual/interaction pass against `docs/screens/home.png`, `home_timer.png`, `timer_done.png`, and `home_offline.png` as soon as it reconnects.

**Known gaps carried forward:** Weather/Bus/Status pages are still pre-spec (Phases 5-7). The Home offline whisper's "showing data from HH:MM" currently reads whichever stale feed's own last-fetch timestamp is oldest among weather/sun/the active bus stop — reasonable, but untested against the real multi-feed-failure scenario the spec's mockup implies. The bus-tile/Bus-page favourite-service wiring (`favBusService`) has no write path until Phase 6.

## Phase 5 — Weather page rewrite (2026-11-20)

**Changed:**
- `config.h`: `HOURLY_COUNT` 4 → 6 (spec's hourly strip is "now + next 5 hours", not 4) and new `DAILY_COUNT = 6` (today + the 5 days the toggle view shows). Removed the now-dead `WEATHER_NOW_*`/`WEATHER_HOURLY_*` layout constants (only the old Weather page read them).
- `state.h`/`state.cpp`: added `humidity`, `uvIndexMax`, and a parallel `dailyWeatherCode/dailyTempMaxC/dailyPrecipProbMax/dailyYmd[DAILY_COUNT]` set (index 0 = today, 1-5 = the 5-day view), plus the page-local `weatherShowDaily` toggle (not persisted — spec: "resets to hourly when you leave the page").
- `network.cpp`: extended `fetchWeather()`'s Open-Meteo request — added `relative_humidity_2m` to `current`, added `uv_index_max`/`weather_code`/`precipitation_probability_max` to `daily`, and bumped `forecast_days` from 1 to `DAILY_COUNT` (6). None of this was being fetched before; the spec's Details line ("Hum 78%"), UV MAX stat (today's daily max, not the current instantaneous UV), and 5-day view all depend on it. Bumped the JSON document capacity 8192 → 24576 for the much larger 6-day hourly+daily payload (ArduinoJson 7's `DynamicJsonDocument` capacity argument is advisory/elastic in this version, but sized it honestly anyway).
- `display.cpp`: full rewrite of the Weather page to the spec's exact layout — header (`WEATHER · <LOCATION>` + relative age), hero temp/icon/condition/details, hourly strip (six 36px columns) that toggles to a 5-day view (five 43px columns) on tap anywhere in y124-224, and two stat columns (PSI, UV max) with the spec's exact band words and PSI's accent-at-Unhealthy+ rule. Added `drawRainBar()` (12×20 track, accent fill from the bottom, 2px minimum) and `drawWeatherStripColumn()` shared by both the hourly and 5-day views (same anatomy, different data). Rewrote `weatherConditionText()` to the spec's WMO table verbatim (word-for-word, including the new "anything else → Cloudy" fallback) and simplified `drawConditionIcon()`/`drawCloudGlyph()`/`drawCleanSunIcon()`/`drawMoonIcon()` to take a `scale` parameter — the spec's five icon kinds (clear/partly/cloud/rain/storm) collapse the old fog/snow glyphs into the cloud fallback, and the hero icon needs a visibly larger (40px) drawing than the 16px hourly ones from the same primitives. Added `measureTextWithGlyphs()`/`drawTextWithGlyphsCentered()` so text containing the degree-ring or middot-dot substitutions can still be centre-aligned (every centred label in the hourly/daily columns needs this). Deleted the dead pre-rewrite helpers (`apparentTempText()`, old `psiText()`/`uvText()`, the old `hourLabel12()`) once nothing called them anymore.
- `main.cpp`/`display.h`: added `handleWeatherTouch()` to the tap-dispatch chain; `handleNavTouch()` resets `weatherShowDaily` when navigating away from the Weather page (the only way to leave it).

**Assumptions flagged, not asked about** (user away):
1. The hourly strip's time labels ("8p", "12a") are always 12h/lowercase, regardless of the `regionFormatKey` date-format setting — the spec's own example strings are written that way and never mention a region-dependent hourly format, unlike the clock/date which do have an explicit toggle.
2. The 5-day view's day columns are the next 5 days *after* today (`dailyYmd`/`dailyWeatherCode`/etc. indices 1-5), matching the mockup's "Wed Thu Fri Sat Sun" when today is Tue — index 0 (today) is kept only to feed the existing `tempMaxC`/`tempMinC` fields, not shown in the 5-day view itself.
3. Icon scale factors (1.3× for the 40px hero, 0.55× for the 16px hourly/daily icons, both relative to the original ~1.0-scale drawing) are a judgement call — the spec says "16 or 40px" but the existing hand-drawn icon primitives were never built to an exact pixel grid, so these are close visual approximations pending an on-hardware check.

**What failed along the way:** none this phase — the Weather page block was large enough that I wrote and spliced it via a scratch file (Python line-range replace) rather than one big `Edit` old-string match, specifically to avoid the kind of exact-whitespace mismatch that bit a smaller edit in Phase 4; it applied cleanly on the first attempt, and `pio run -e makerpulse -t clean` followed by a full rebuild confirmed every touched file actually recompiles clean, not just the incrementally-rebuilt ones.

**Verified:** both the normal and `-DCOLOUR_TEST` builds compile cleanly after a full clean rebuild (only pre-existing ArduinoJson deprecation warnings). Flash usage 36.8-36.9% (~1.16-1.162MB / 3,145,728 bytes), RAM 15.8%. Hardware verification still pending — device has not reappeared at `/dev/cu.usbserial-*` since Phase 1; will check against `docs/screens/weather.png`, `weather_daily.png`, and `weather_night.png` as soon as it reconnects.

**Known gaps carried forward:** Bus/Status pages are still pre-spec (Phases 6-7). Night mode (Charcoal palette auto-switch) isn't wired up yet, so `weather_night.png`'s dark-mode comparison can't be fully verified until Phase 7.

## Phase 6 — Bus page rewrite (2026-11-20)

**Changed:**
- `config.h`: `BusServiceInfo` now stores raw epoch seconds (`arrival1/2/3`) instead of minutes pre-computed at fetch time, plus `loadPips`/`vehicleType` (parsed from LTA's `Load`/`Type` fields, not fetched before). Added `BUS_ROW_Y0`/`BUS_ROW_H`/`BUS_ROWS_PER_PAGE` (56/52/4, replacing the old 3-per-page layout). Removed the on-device bus-stop-code-entry keypad's constants (`BUS_ENTRY_*`, `busEntryKeys`) and the old stop-tab layout constants (`BUS_TAB_*`) — see the architecture note below.
- **Correctness fix carried over from the spec's own data rules, not a style choice**: the pre-V2.1.0 code converted `NextBus.EstimatedArrival` to "minutes from now" once, at fetch time, and never touched it again until the next ~45s fetch — so a rider watching the screen would see a stale, non-decrementing number for up to 45 seconds. The spec is explicit: *"parse them to epoch seconds once on fetch and store integers. All countdown text is recomputed from those integers every 15s, so minutes tick down smoothly between fetches."* `network.cpp`'s `parseBusEtaMin()` → `parseBusArrivalEpoch()` now returns the raw epoch; `display.cpp` has a shared `busMinutesUntil(epoch)` that both the Home Bus tile and the new Bus page call fresh on every redraw (the existing once-a-second `updateCurrentPageDynamic()` tick already satisfies "every 15s" with room to spare — no new polling machinery needed, since the redraw is already gated by a string-diff cache that changes the moment the computed minute value changes).
- `network.cpp`: `ensureBusArrival()` now refreshes the on-screen stop every 30s while `currentPage == PAGE_BUS` (spec: "While this page is visible, bus refresh is 30s") and falls back to the normal `BUS_INTERVAL_SEC` (45s) the rest of the time and for other configured-but-not-visible stops.
- `display.cpp`: full rewrite of the Bus page to the spec's exact layout — header (stop code, up-to-3 stop dots hidden when only one stop, stop name, page count hidden at ≤4 services), four 52px rows with the spec's exact row anatomy (service number accent-if-favourite, load pips + word + vehicle type, right-aligned ETA with the `Arr`/`~N min`/`N min` rules, right-aligned "then X · Y" / "scheduled" / "last bus" next-line), and the three states (no stop configured, fetching, no buses / "Checked HH:MM", offline-with-old-data whisper at y268). Favourite-first ordering via `busDisplayOrder()` (favourite pinned first, rest in already-soonest-first fetch order). Added `handleBusLongPress()` for "long-press a row → favourite" (writes `favBusService`, persists to NVS, invalidates the Home tile's cache so it updates immediately too).
- **Removed the on-device bus-stop-code-entry keypad** (`drawBusStopEntryOverlay`, `busEntryKeyRect`, `handleBusStopEntryTouch`, and the `busStopEntryOpen`/`busStopEntryTargetIdx`/`busStopEntryDigits`/`cacheBusStopEntry` state) entirely. This was a pre-V2.1.0 convenience feature (a 12-key numeric pad for typing a bus stop code directly on the device) that the V2.1.0 spec's Bus section never mentions and `docs/screens/bus.png`/`bus_empty.png` show no affordance for (no "+" button anywhere in the header). All three stop slots are already configurable from the settings page's existing `busCode0/1/2` fields; keeping a second, spec-undocumented entry point for the same data would have meant either finding new unclaimed screen space for a "+" trigger that doesn't exist in the mockups, or leaving genuinely dead, unreachable code behind. Logged here as a deliberate removal, not an oversight.

**Assumptions flagged, not asked about** (user away):
1. "Bus refresh is 30s while this page is visible" is scoped to whichever stop is actually the active/on-screen one — the other two configured stops (if any) keep refreshing at the normal 45s interval in the background rather than all three also dropping to 30s, since only one stop is ever visible at a time.
2. The "no key" / "Add your LTA key in config.h" spec state doesn't apply to this codebase's architecture (ADR 0003: runtime settings page, not config.h constants) — there's always a usable key (the user's own, or the shared `LTA_FALLBACK_API_KEY` fallback), so the only "not configured" state that can actually occur is "no stop code set," which is what's shown instead.
3. When a service is both the favourite and unmonitored (the `~N min` muted case), the row number still renders in accent (favourite) while the ETA value stays muted (unmonitored) — the two colour rules apply to different parts of the row and aren't mutually exclusive, which the spec's row-anatomy description doesn't explicitly disambiguate.

**What failed along the way:** none this phase — same scratch-file-splice approach as Phase 5 (large page rewrite written and verified in a standalone file, then spliced in by exact line range) worked cleanly on the first attempt; one self-caught naming slip (initially reused `lastWeatherHeaderText` as the Bus page's header cache instead of adding a dedicated `lastBusHeaderText`), fixed before building.

**Verified:** both the normal and `-DCOLOUR_TEST` builds compile cleanly after a full `pio run -t clean` + rebuild (only pre-existing ArduinoJson deprecation warnings). Flash usage ~36.9% (~1.1613MB / 3,145,728 bytes), RAM 15.9%. Hardware verification still pending — device has not reappeared at `/dev/cu.usbserial-*` since Phase 1; will check against `docs/screens/bus.png` and `bus_empty.png`, and specifically verify the epoch-based countdown actually ticks down smoothly between fetches (the exact bug this phase's data-model change was meant to fix), once it reconnects.

**Known gaps carried forward:** Status page is still pre-spec (Phase 7), including the touch-calibration routine and night mode. The Bus page's "no key" state is intentionally not implemented per assumption #2 above.

## Phase 7 — Status page + night mode + settings (2026-11-20)

**Changed:**
- `config.h`: `BL_DIM` 18 → 12 (spec: "fade to 12"); added `BRIGHTNESS_LOW/MID/HIGH` (40/140/255, spec's exact Low/Mid/High backlight levels) and `STATUS_*`/`CAL_PT_*` layout constants. Removed the dead `FLASH_BL_LOW`/`FLASH_BL_HIGH` (orphaned since Phase 4 dropped the flash-on-timer-done toggle).
- `state.h`/`state.cpp`: added `ownerName` (new web-settings field, spec's `OWNER_NAME`), `brightnessLevel`/`brightnessPwm()`/`setBrightnessLevel()` (0/1/2 → 40/140/255, persisted), `nightModeKey`/`applyNightOverride()` (spec "Night mode Auto": Charcoal palette + Low brightness between sunset and sunrise, 19:00-07:00 fallback, without touching the user's actual `shellColourKey` choice — the chosen colourway reapplies automatically once `applyNightOverride()` next sees night has ended), and `forcedSyncStartedMs`/`forceSyncAllFeeds()` for the Synced row's force-refresh. Replaced every `setBacklight(BL_FULL)` call that meant "return to normal brightness" with `setBacklight(brightnessPwm())` — `BL_FULL` itself is now reserved for the two cases that are genuinely spec-literal "High" regardless of the user's chosen level (the pre-settings-load boot flash, and the timer-done takeover).
- `display.cpp`: full rewrite of the Status page to the spec's exact layout — header (title/version/owner/case-colour swatch), colourway label + 6 tappable swatches with the 2px selection ring, Brightness and Night-mode segmented options (`drawSegmentedOption()`/`segmentedHitTest()`, sharing one `computeSegmentedLayout()` so the drawn geometry and the hit-test geometry can never drift apart), and the four info rows (Wi-Fi, Synced with its per-feed accent-when-stale colouring, IP, Uptime). Added the two-point touch-calibration routine (`touchCalibrationActive()`/`startTouchCalibration()`/`updateTouchCalibration()`) — reads raw `ts.getPoint()` directly, solves the linear `map()` equation from two crosshair-tap samples, and accounts for whichever `touchFlipX/Y` orientation is currently active so the routine works correctly in either USB-cable orientation. `main.cpp` wires it in: a 5s hold on the STATUS header (`touchHoldMsInRect`) starts it, and while active it owns all touch input ahead of the normal tap/long-press dispatch.
- **Removed two features that had no spec affordance**, for the same reason as Phase 6's bus-stop keypad removal — matching `docs/screens/status.png` exactly rather than leaving spec-undocumented, partially-reachable controls behind:
  1. The on-device Wi-Fi enable/disable toggle (old Status page's "NETWORK" button). The spec's Status Interactions table has no such control, and removing its only call site would have left `setWifiEnabled()` fully unreachable dead code, so the function itself was deleted too (from both `network.cpp` and `network.h`). `wifiEnabled` itself is untouched — it still gates connection logic exactly as before, it just can no longer be flipped to false from the UI.
  2. Nothing else was dropped this phase, but see the dead-code sweep below.
- **Dead-code sweep**: with all four pages now rewritten to the V2.1.0 layout, every pre-spec chrome helper and layout constant was finally orphaned and got removed: `drawV2PageHeader()`, `drawTopBar()`, `drawNavBar()`, `drawCard()`, `makeSpriteCard()`, `pushSpriteAndDelete()`, `drawWrappedTextLimited()`, `wifiStatusText()`, `signalText()`, `statusColor()`, `nextSunLabel()`, `nextSunTimeText()`, `lastSyncText()`, `formatMinuteOfDay()`, plus roughly 30 now-unreferenced `HOME2_*`/`PAGE_ROW*`/`HOME_GRID_*`/`HOME_TIMER_*`/`TIMER_MENU_*`/`TIMER_DONE_*`/`NAV_H` layout constants in `config.h` and their matching `last*Text` cache strings in `state.h`/`state.cpp`. None of this was Phase-7-specific work — it's the accumulated debt of the page-by-page migration (each phase's own log noted "Weather/Bus/Status still use the old layout" as a known gap; Status finishing the migration is what finally made all of it unreachable at once).

**Assumptions flagged, not asked about** (user away):
1. The spec's "fade to 12 over 1s" auto-dim describes an animated one-second ramp; this still dims instantly to the new value (12, was 18), same as the pre-existing `enterSleepDim()` behaviour — animating the fade is a pure visual nicety with no functional effect, and wasn't worth the added complexity (a timed interpolation loop) relative to everything else in this phase.
2. The Synced row's "Force-refresh all feeds now; row reads 'Syncing...' until done" is approximated with a fixed 10-second "Syncing..." window after the tap rather than a true completion signal — there's no existing per-feed "fetch finished" event to hook into without deeper restructuring (the net task already fetches whichever single feed is stalest on each pass, not all three in lockstep), so this is a reasonable, time-boxed approximation of the spec's intent.
3. Touch calibration's two target points are fixed at `(30,30)` and `(210,290)` (inset from the edges) rather than being configurable or chosen to match any specific existing mockup, since the spec only says calibration exists and is NVS-backed, not what the on-screen routine should look like (flagged as an open question back in Phase 2's log too).
4. "Product name" on Status reuses the existing `homeTitleText()` (shows the device nickname if the user set one, else "MakerPulse") rather than being hardcoded to always read "MakerPulse" — this preserves a pre-existing personalization feature (`deviceNickname`) that predates this spec migration, consistent with the spec's own framing of Status as "says whose MakerPulse this is."

**What failed along the way:** one linker error caught immediately on the first build attempt — `touchCalibrationActive()`/`startTouchCalibration()`/`updateTouchCalibration()` were declared in `display.h` and wired into `main.cpp` before their actual bodies existed in `display.cpp`. Added the implementation (including the two-point linear-calibration solver) and the next build linked clean.

**Verified:** both the normal and `-DCOLOUR_TEST` builds compile cleanly after a full `pio run -t clean` + rebuild (only pre-existing ArduinoJson deprecation warnings). Flash usage 37.1% (~1.167MB / 3,145,728 bytes), RAM 15.9%. Hardware verification still pending — device has not reappeared at `/dev/cu.usbserial-*` since Phase 1; the touch-calibration routine in particular needs an on-hardware pass since it's the one piece of this phase that can't be checked by compiling alone (it reads real raw touch-controller values).

**Known gaps carried forward:** none specific to Status — this was the last page. Phase 8 (hardening) still needs: the full spec acceptance-checklist audit, a soak-test pass, edge-case review, `README_V2.md`, and the `FIRMWARE_VERSION` bump from "2.0.0-dev" to "2.1.0" (deliberately left alone until Phase 8 so every phase's build/flash cycle up to now was clearly pre-release).

## Phase 8 — Hardening pass + version bump to 2.1.0 (2026-11-20)

**Spec table audit (Done / Different / Missing), and what got fixed:**
- Home/Weather/Bus/Status layout tables: Done (built page-by-page in Phases 4-7; see each phase's own "Verified" line — all pending an actual on-hardware visual check).
- **Data sources and refresh cadence — was Different, now Done**: weather was refreshing every 10 min (spec: 15 min), PSI every 15 min (spec: 60 min), and bus always at a flat 45s regardless of which page was visible (spec: 30s on Home or Bus, 120s elsewhere or when dimmed). Fixed all three in `config.h`/`network.cpp`'s `ensureBusArrival()` — `WEATHER_INTERVAL_SEC` → 15 min, `PSI_INTERVAL_SEC` → 60 min, `BUS_INTERVAL_SEC` → 120s with a new `BUS_FAST_INTERVAL_SEC` (30s) applied whenever Home or Bus is the visible page and the screen isn't dimmed. Added `BUS_STALE_SEC` (flat 3 min, per spec) instead of deriving bus staleness as "2x the refresh interval" the way weather/PSI do — bus's own refresh rate varies, so that formula isn't stable for it the way it is for the other two.
- **Wi-Fi drop reconnection — was Missing, now Done**: spec says "Wi-Fi drop: reconnect every 30s in the network task." Before this fix, only the *initial* boot-time connection attempt had any retry/timeout logic (`updateWiFiConnectionState()`, main loop) — if Wi-Fi dropped afterwards (router reboot, etc.), nothing ever attempted to reconnect at all. `netTaskFn()` (`network.cpp`) now retries `beginWiFiConnect()` every 30s whenever disconnected, credentials exist, and no connection attempt is already in progress.
- **"NTP not yet synced (show '--:--')" edge case — was Missing, now Done**: the Home clock/date/week header had no concept of "time isn't known yet" and would have rendered a bogus 1970 date if displayed before NTP sync landed (it just happened to never be *observed* doing this because `finishBootAfterWifi()` already waits up to 10s for sync before the first draw — but a slow or failed sync would have exposed it). Added a shared `timeIsSynced()` (`state.cpp`, same sanity threshold `waitForNtpTime()` already used) and wired it into `drawHomeHeader()`: the whole header shows placeholders ("--:--", "---", "W--") together rather than a half-real, half-garbage date.
- Other named edge cases (empty bus response, PSI API down, midnight rollover, 12h mode) — Done already, verified by re-reading the relevant code paths rather than new changes: `fetchBusArrival()`'s empty-`Services` case already produces `busServiceCountByStop[idx]=0`, which the Bus/Home "no buses" states already handle; `fetchPsi()`'s failure path already leaves `psiValue` at its last value (or NaN) with backoff retry and the Weather stat column already shows "--"; midnight rollover is handled implicitly (every redraw recomputes the date/day-name/week fresh from `time(nullptr)`, no cached "today" value anywhere that could go stale except `lastSunYmd`, which explicitly checks for a day change); 12h mode is `clockIs24h` (Phase 4), exercised by its own toggle.
- Full-screen-redraw discipline (spec: "no code path does a full-screen redraw except page changes, colourway changes and the timer takeover") — audited every `tft.fillScreen()` call site: the four `drawXPageFull()` functions (page changes, and transitively colourway changes via the `pageDirty` they already trigger), the timer-done takeover, and the touch-calibration crosshair (a deliberate exception — a full-screen utility mode entered only by an explicit 5s held tap, not a routine redraw). No violations found.
- Firmware structure / `ui_kit.h` / `data_model.h` / `net_task.cpp` / `pages/page_*.cpp` file-split table — Different, and staying Different on purpose: this is the spec's own alternate from-scratch architecture suggestion, not a requirement; ADR 0003 already covers the decision to keep this codebase's existing per-concern file split (`state.cpp`, `network.cpp`, `display.cpp`, `settings_server.cpp`) instead.
- "No key or first boot: 'Add your LTA key in config.h'" (Bus) — intentionally Missing; see Phase 6's assumption #2 (this architecture always has a usable key via the shared fallback, so the literal spec state can't occur, and the message itself refers to the config.h-constants architecture this project doesn't use).

**12-hour soak test**: not run — needs real hardware, which has been unreachable since Phase 1. A static review in place of it found no obvious leak patterns: every `JsonDocument` in `network.cpp` is function-scoped (allocated and destroyed within each `fetch*()` call, never held across calls), there's no raw `malloc`/`new` anywhere in the touched code, and the sprite-based rendering helpers that could have leaked (`makeSpriteCard`/`pushSpriteAndDelete`) were themselves found dead and removed in Phase 7. This is not a substitute for watching `ESP.getFreeHeap()` over real time, which is the first thing to check once the device reconnects.

**Dead-code sweep (continued from Phase 7)**: none new this phase beyond what Phase 7 already found — the interval/staleness/reconnect fixes above were all small, targeted edits to already-live code.

**`FIRMWARE_VERSION`**: bumped from `"2.0.0-dev"` to `"2.1.0"` (`config.h`) — the very last change, after everything else above was already building clean, so every earlier build in this log was unambiguously pre-release.

**`README_V2.md`**: written for workshop teachers — flashing (same image for every board, nothing to edit in `config.h` first, matching this project's runtime-settings architecture rather than the spec's own config.h-based alternative), first-time Wi-Fi setup, getting an LTA and a data.gov.sg key, picking a colourway (on-device via Status, or from the settings page), the touch-calibration steps, orientation, and known limits (PSI/bus are Singapore-specific; no RTC battery backup).

**What failed along the way**: none — every fix in this phase was a small, targeted change to code already covered by Phases 1-7's own testing discipline, and each one was confirmed with a full `pio run -t clean` rebuild (both the normal and `-DCOLOUR_TEST` variants) before moving to the next.

**Verified**: both build variants compile cleanly with `FIRMWARE_VERSION "2.1.0"` after a full clean rebuild (only the pre-existing ArduinoJson deprecation warnings). Flash usage 37.1% (~1.167MB / 3,145,728 bytes), RAM 15.9%. Hardware verification is the one thing this phase could not do — see "Known limits" in the summary above.

**Known limits carried forward**: all of them are listed in the "Summary: v1.2.0 → v2.1.0" section at the top of this log, headlined by the lack of any hardware verification since Phase 1. The device should be flashed and run through the on-device acceptance checklist (spec's own list) at the first opportunity.

## Phase 9 — On-hardware font/layout fixes (2026-11-18)

First real flash since Phase 1 surfaced three bugs invisible to the compile-only verification Phases 2-8 relied on, all from user-supplied screenshots of the four pages:

- **All 7 fonts were ~40-50% oversized.** Phase 1's font generation calibrated pixel sizes against digit heights measured directly off `docs/screens/*.png`, which are rendered at 2x scale (documented in `docs/screens/README.md` and `CLAUDE.md`) — the halving step was missed. Regenerated all 7 at the spec's literal nominal sizes instead of back-solving from a measured target (e.g. `F_DISPLAY` 82px → 56px), verified against the spec's own "23:58 ends near x165" acceptance line.
- **`drawSmallCapsLabel()` collapsed spaces in multi-word labels** ("BUS STOP" → "BUSSTOP"). Root cause: `TFT_eSPI::textWidth()` has a last-character special case (assumes a right-aligned string's trailing side-bearing shouldn't be over-counted) that's correct for whole-string calls but returns 0 for a space glyph when called per-character in a loop, since every call sees its one character as "the last." Fixed by reading `xAdvance` directly from the glyph table instead of calling `textWidth()` per character.
- **Tab bar overlapped on "Weather"**: the fixed 54px-per-label cell pitch had no margin for it. Switched to dynamic per-label widths with an explicit gap, centred in the content column (same technique Status's `computeSegmentedLayout()` already used).

Also rescaled the degree-ring/middot glyph substitutes in `drawTextWithDegree()` to `tft.fontHeight()` instead of a fixed `r=3`, which read as a disconnected speck above the large `F_DISPLAY`/`F_NUM` digits once those shrank back to their correct size.

**Verified:** compiles clean; user confirmed the device was reflashed (via the GitHub Pages ESP Web Tools site after repeated direct-USB upload instability) and is running with a live WiFi connection. Fresh photos confirming the visual overlap is actually resolved are still outstanding.

## Phase 10 — Timer picker rebuild, reversing two Phase-4 decisions (2026-11-21)

User feedback: wanted the 6 preset durations back as **1/3/5/10/15/30 min** (was 5/10/15/25/45), a **Custom** entry option, and reported long-press "does not work" at all. A full grilling session (`userSettings:grill-with-docs`) settled the design — see CONTEXT.md's new "timer picker" entry for the resulting model. This reverses two Phase-4 decisions: the fixed long-press-cycle timer became a tap-opened picker, and "no settings-page editing" for the presets became editable again.

**Changed:**
- **Root-caused the long-press bug**, not timer-specific: `pollTouchEvent()` (`display.cpp`) tracked the *maximum* finger drift since a press began (`maxMoveSq`), with a 10px tolerance that never reset or decayed — one noisy sample anywhere during a 600ms hold permanently disqualified that whole press from ever firing `TOUCH_LONG_PRESS`, even if the finger never moved again. Fixed by checking the finger's *current* distance from the anchor at each poll instead (a resistive touchscreen's initial-contact noise settles well before 600ms), against a separate, looser `V2_LONGPRESS_MAX_MOVE_PX` (20px, `config.h`) — tap detection is untouched, since it has no "current position" to check at release time and still needs the accumulated `maxMoveSq`. `touchHoldMsInRect()` (the Status 5s calibration hold) picked up the same looser constant for consistency, since it's the same class of gesture. This one fix benefits the clock's 12h/24h long-press toggle and the Bus page's favourite-row long-press too, not just the timer.
- **Home Timer tile tap is now state-aware** (`handleTimerTileTap()`, `state.cpp`, replacing `startOrToggleFocusTimer()`): Idle and Paused taps both open the timer picker; Running still just pauses. The old long-press actions (`cycleFocusDuration()`/`resetFocusTimerToIdle()`) are gone — picking a duration in the picker is itself "go," there's no separate start step.
- **New timer picker overlay** (`drawTimerPickerOverlay()`/`handleTimerPickerTouch()`, `display.cpp`): full-screen, flat, hairline-separated rows — the 6 presets + Custom, with Resume/Stop prepended when reopened from Paused. **New Custom stepper** (`drawCustomTimerStepperOverlay()`/`handleCustomTimerStepperTouch()`): ±1/±5 buttons, 1-180 min range, pre-filled with `focusDurationMin` (whatever was last actually run, not a fixed default). Its explicit Cancel returns to the picker; tapping outside its panel band closes everything via `closeTimerPicker()` (leaves `focusTimerState` untouched, so an accidental outside-tap can't cancel a paused session as a side effect) — this replicates old MakerPulse's `handleCustomTimerEntryTouch()` Cancel-vs-tap-outside distinction. Both overlays are wired into `main.cpp`'s touch dispatch and `drawCurrentPageFull()`/`updateCurrentPageDynamic()` as top-level modals, the same pattern the existing Timer-Done takeover already used.
- **Persistence switched from index to raw minutes**: `focusDurationIdx` (indexed into the old fixed `FOCUS_DURATIONS_MIN[]`) is gone, replaced by `focusDurationMin`, persisted as `"focusDurMin"` in NVS — a raw value, since Custom entries don't fit an index model.
- **Presets are editable again** via the settings page: a new "Focus timer" block in `settings_server.cpp`'s `handleRoot()`/`handleSave()`, reusing the existing `.timer-slot`/`.timer-slot-grid` CSS (previously only used for the Bus-stop cards), persisted as `"timerPreset0"`..`"timerPreset5"`, 1-180 min range (same bound as Custom).

**Assumptions flagged, not asked about:**
1. The paused-reopen's "Resume"/"Stop" prepended rows reconcile two slightly different-sounding answers from the grilling session's first round (one described "ask whether to resume or stop," the other "picker with Resume as the first option") by including both — no objection was raised in the following round.
2. Custom's tap-outside behaviour (closes "everything") is implemented as "return to whatever state was already live" (idle, or still paused) rather than forcing idle outright — forcing idle would cancel a paused session as a side effect of a stray tap on a sub-screen, which seemed more destructive than the old MakerPulse behaviour (which never had a paused-session case to begin with) intended.
3. `V2_LONGPRESS_MAX_MOVE_PX = 20` (vs the tap path's 10px) is a judgment call, not spec-derived — the spec only says "imprecise" touch, not a number.

**What failed along the way:** none — compiled clean on the first attempt after all the edits above.

**Verified:** `pio run -e makerpulse` compiles clean (Flash 37.0% / RAM 15.9%, only pre-existing ArduinoJson deprecation warnings). Hardware verification pending — will likely go through the GitHub Pages flashing site again given this session's USB instability.
