// MakerPulse firmware -- ESP32 "Cheap Yellow Display" desk companion.
// See CONTEXT.md for the settings-page/setup-site glossary and README.md
// for the build/flash/release process.

#include "config.h"
#include "state.h"
#include "network.h"
#include "display.h"
#include "settings_server.h"



// =========================================================
// SETUP / LOOP
// =========================================================
void waitForNtpTime() {
  unsigned long t0 = millis();
  while (!timeIsSynced() && millis() - t0 < 10000) {
    delay(200);
  }
}


void drawSetupScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("MakerPulse", SCREEN_W / 2, 60, 4);
  tft.setTextColor(0x5EFA, TFT_BLACK);
  tft.drawCentreString("Set up your device at:", SCREEN_W / 2, 140, 2);
  tft.drawCentreString(SETUP_SITE_URL, SCREEN_W / 2, 164, 2);
  tft.setTextColor(0x94B2, TFT_BLACK);
  tft.drawCentreString("Connect via USB and", SCREEN_W / 2, 220, 1);
  tft.drawCentreString("follow the on-screen steps", SCREEN_W / 2, 236, 1);
}


void finishBootAfterWifi() {
  tft.drawString("Syncing time...", 10, 58, 2);
  configTzTime(
    "SGT-8",
    "pool.ntp.org",
    "time.google.com",
    "time.cloudflare.com"
  );
  waitForNtpTime();

  // Fetching is the net task's job (startNetTask(), already running on
  // core 0) -- it picks these up on its next pass now that NTP/WiFi are
  // ready. The first page draw below may briefly show placeholder values
  // until that pass completes (within ~1s).

  setupWebServer();

  pageDirty = true;
  dataDirty = true;

  drawCurrentPageFull();
  updateCurrentPageDynamic();

  lastClockTick = millis();
  lastDataTick = millis();

  Serial.print("MakerPulse web: http://");
  Serial.println(WiFi.localIP());
}


void onImprovWiFiConnectedCb(const char *ssid, const char *password) {
  storedWifiSsid = String(ssid);
  storedWifiPass = String(password);
  prefs.putString("wifiSsid", storedWifiSsid);
  prefs.putString("wifiPass", storedWifiPass);
  wifiEnabled = true;
  prefs.putBool("wifiEnabled", true);

  bool wasProvisioned = deviceProvisioned;
  deviceProvisioned = true;

  if (!wasProvisioned) {
    finishBootAfterWifi();
  }
  pageDirty = true;
  dataDirty = true;
}


void onImprovWiFiErrorCb(ImprovTypes::Error err) {
  // Improv reports errors back over serial to the website itself; nothing
  // additional required on-device.
}


void setup() {
  Serial.begin(115200);
  improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "MakerPulse",
                              FIRMWARE_VERSION, "MakerPulse", "http://{LOCAL_IPV4}");
  improvSerial.onImprovConnected(onImprovWiFiConnectedCb);
  improvSerial.onImprovError(onImprovWiFiErrorCb);

  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, BL_FULL);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  loadStoredSettings();
  setBacklight(brightnessPwm());
  lastInteractionMs = millis();

  dataMutex = xSemaphoreCreateMutex();
  startNetTask();

  delay(200);

  tft.init();
  tft.invertDisplay(INV);
  tft.setSwapBytes(true);

  touchSPI.begin(T_SCK, T_MISO, T_MOSI);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  ts.begin(touchSPI);
  applyOrientationByKey(orientationKey); // tft/ts are ready now; see state.cpp

#ifdef COLOUR_TEST
  drawColourTestScreen();
  return; // static debug screen -- no WiFi, no touch, no normal boot
#endif

  if (!deviceProvisioned) {
    drawSetupScreen();
    return; // skip WiFi connect / NTP / webserver / first page draw entirely
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Booting MakerPulse...", 10, 10, 2);
  tft.drawString("Connecting WiFi...", 10, 34, 2);
  connectWiFi(true);

  finishBootAfterWifi();
}


void loop() {
  improvSerial.handleSerial();

  if (!deviceProvisioned) {
    delay(10);
    return; // Improv is the only thing running; TFT just shows the setup screen
  }

  if (touchCalibrationActive()) {
    updateTouchCalibration();
    delay(10);
    return; // owns all touch input until the two-point routine finishes
  }

  server.handleClient();
  updateWiFiConnectionState();
  updateFocusTimerState();
  updateTimerDoneAutoDismiss();
  handleAutoSleep();

  // Spec "Touch and gestures": tap vs long-press, resolved by
  // pollTouchEvent(). justPressed fires on the raw down-edge, before a tap
  // or long-press has resolved -- used here for "wake swallows the first
  // touch," same swallow rule as before (full-off or auto-dimmed swallows;
  // manually-dimmed does not, taps still work while dim on purpose).
  TouchEvt evt;
  bool justPressed = false;
  bool haveEvt = pollTouchEvent(evt, justPressed);
  static bool swallowPress = false;

  // Spec "Status" > "Interactions": "STATUS header: Hold 5s -> touch
  // calibration." Independent of the 600ms long-press used everywhere
  // else (handled via touchHoldMsInRect(), not pollTouchEvent()).
  if (currentPage == PAGE_STATUS && touchHoldMsInRect(0, 0, SCREEN_W, 54) >= (unsigned long)V2_CALIBRATION_HOLD_MS) {
    startTouchCalibration();
  }

  if (justPressed) {
    swallowPress = false;
    lastInteractionMs = millis();

    if (sleepOff) {
      swallowPress = true;
      if (manualDimMode) {
        sleepOff = false;
        sleepDimmed = true;
        setBacklight(BL_DIM);
        pageDirty = true;
      } else {
        wakeDisplay();
      }
    } else if (sleepDimmed && !manualDimMode) {
      swallowPress = true;
      wakeDisplay();
    }
  }

  if (haveEvt && !swallowPress) {
    int tx = evt.x, ty = evt.y;

    if (evt.type == TOUCH_TAP) {
      if (!handleTimerDoneDialogTouch(tx, ty)) {
        if (timerCustomEntryOpen) {
          handleCustomTimerStepperTouch(tx, ty);
        } else if (timerPickerOpen) {
          handleTimerPickerTouch(tx, ty);
        } else if (tx >= SCREEN_W - 36 && ty <= TOPBAR_H) {
          toggleSleepMode();
          pageDirty = true;
        } else if (!handleHomeTouch(tx, ty) && !handleWeatherTouch(tx, ty) && !handleStatusTouch(tx, ty) && !handleBusTouch(tx, ty)) {
          handleNavTouch(tx, ty);
        }
      }
    } else if (evt.type == TOUCH_LONG_PRESS) {
      // Phase 7: STATUS header 5s hold runs touch calibration via
      // touchHoldMsInRect(), independent of this 600ms event -- a no-op
      // here for now, not a missed case. The timer picker/Custom stepper
      // have no long-press action of their own, so no guard needed here
      // the way the tap branch above needs one.
      if (!handleHomeLongPress(tx, ty)) handleBusLongPress(tx, ty);
    }
  }

  if (pageDirty || lastDrawnPage != currentPage) {
    bool anyOverlayOpen = (focusTimerState == TIMER_DONE) || timerPickerOpen || timerCustomEntryOpen;
    if (lastDrawnPage != currentPage || !anyOverlayOpen) {
      drawCurrentPageFull();
    } else if (focusTimerState == TIMER_DONE) {
      drawTimerDoneOverlay(true);
    } else if (timerCustomEntryOpen) {
      drawCustomTimerStepperOverlay(true);
    } else if (timerPickerOpen) {
      drawTimerPickerOverlay(true);
    }
    updateCurrentPageDynamic();
    pageDirty = false;
    dataDirty = false;
  }

  if (millis() - lastClockTick >= CLOCK_TICK_MS) {
    lastClockTick = millis();
    applyNightOverride();
    updateCurrentPageDynamic();
    dataDirty = false;
  }

  delay(10);
}
