#pragma once

// Touch input and everything drawn to the TFT: page chrome, sprites/icons,
// the four main pages and their overlays, and the readout-formatting
// helpers used only for what appears on screen.

#include "state.h"

bool readTouchXY(int& sx, int& sy);
bool touchNewPress(int& tx, int& ty);

// Spec "Touch and gestures": tap (<=400ms, <10px move) vs long-press
// (600ms, fires once at the threshold without waiting for release).
enum TouchEvtType { TOUCH_NONE, TOUCH_TAP, TOUCH_LONG_PRESS };
struct TouchEvt {
  TouchEvtType type;
  int x, y;
};
// Returns true if evt.type != TOUCH_NONE this call. justPressed is set true
// on the exact frame a new press begins (before tap/long-press resolve) --
// use it for "wake swallows the first touch", independent of what the
// press eventually resolves to.
bool pollTouchEvent(TouchEvt& evt, bool& justPressed);
// Continuous-hold duration (ms) of the touch currently in progress at
// (x,y) within the given rect, or 0 if nothing is being held there. For
// the Status-page 5s calibration hold, which is its own longer threshold
// on top of (not instead of) the normal long-press.
unsigned long touchHoldMsInRect(int rx, int ry, int rw, int rh);

// Spec "Status" > "Interactions": "STATUS header: Hold 5s -> touch
// calibration." A simple two-point crosshair routine; reads raw touch
// points directly (bypassing the calibration mapping it's setting).
// While active it owns all touch input -- main.cpp's loop() checks
// touchCalibrationActive() before its normal tap/long-press dispatch.
bool touchCalibrationActive();
void startTouchCalibration();
void updateTouchCalibration();

void setBacklight(int value);
void wakeDisplay(bool clearManualMode = true);
void enterSleepDim();
void enterSleepOff();
void toggleSleepMode();
void handleAutoSleep();

void drawCurrentPageFull();
void updateCurrentPageDynamic();
void drawColourTestScreen();
void drawTimerDoneOverlay(bool force = false);

bool handleHomeTouch(int x, int y);
bool handleHomeLongPress(int x, int y);
bool handleWeatherTouch(int x, int y);
bool handleStatusTouch(int x, int y);
bool handleBusTouch(int x, int y);
bool handleBusLongPress(int x, int y);
bool handleTimerDoneDialogTouch(int x, int y);
void handleNavTouch(int x, int y);
