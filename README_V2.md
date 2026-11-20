# MakerPulse V2.1.0 — Workshop Teacher's Guide

MakerPulse is a desk companion built on the ESP32-2432S028 "Cheap Yellow
Display" board. This guide covers flashing a device, setting it up, picking
a colourway to match the printed case, and calibrating touch — everything
a workshop teacher needs, no code required.

If you're looking for the design spec this firmware implements, see
`docs/MAKERPULSE_V2_SPEC.md` and the reference mockups in `docs/screens/`.
If you're changing the firmware itself, see `CLAUDE.md` and `PROJECT_LOG.md`.

## 1. Flashing a device

Unlike some ESP32 workshop kits, **nothing needs to be edited in code before
flashing.** Every per-device setting (Wi-Fi, location, bus stops, API keys,
colourway, owner name) is entered later through the device's own setup
website — flash the same firmware image onto every board.

1. Plug the board in by USB.
2. Open `https://theyoungmaker.github.io/MakerPulseV2` in Chrome or Edge
   (the flashing page needs WebSerial, which only those two support).
3. Click **Connect**, choose the board's serial port, and click **Install**.
4. Wait for it to finish — the screen will show "Set up your device at:
   theyoungmaker.github.io/MakerPulseV2" once it reboots.

## 2. First-time setup (Wi-Fi + settings)

1. With the device still plugged into USB, reload the same setup page.
2. Click **Connect** again, choose the serial port, and follow the prompts
   to enter your Wi-Fi network name and password. The device saves these
   and reboots onto your network — USB is no longer needed after this step.
3. Find the device's IP address — either watch the setup page (it prints
   the address once Wi-Fi connects) or check your router's device list for
   "MakerPulse".
4. Open `http://<that-ip-address>/` in a browser on the same network. This
   is the device's own settings page, and it's where everything below gets
   configured.

## 3. Getting your API keys

Two of the device's data feeds need a free API key. MakerPulse works out of
the box without either (bus arrivals use a shared fallback key; PSI simply
shows "--" until a key is entered), but a device's own key is more
reliable, especially with a whole classroom of boards sharing a network.

- **LTA DataMall (bus arrivals)** — register at
  [datamall.lta.gov.sg](https://datamall.lta.gov.sg/content/datamall/en/request-for-api.html),
  request an AccountKey, and paste it into the settings page's **LTA
  AccountKey** field under "Bus arrival".
- **data.gov.sg (PSI / air quality)** — register at
  [api-open.data.gov.sg](https://api-open.data.gov.sg/), and paste the key
  into the settings page's **data.gov.sg API key** field under "Air
  quality (PSI)".

Bus stop codes are the 5-digit numbers printed on the physical bus stop
sign; enter up to three per device. The stop name is looked up
automatically once a code is saved.

## 4. Picking a colourway

MakerPulse ships in six 3D-printed case colours — Sage, Ivory, Sky, Peach,
Lilac and Charcoal — and the on-screen colours are tuned to match each one.
Pick whichever one matches the physical case in front of you:

- **On the device**: go to the **Status** tab and tap one of the six
  swatches below "COLOURWAY". It applies instantly.
- **From the settings page**: the same six swatches appear under
  "Colourway" if you'd rather set it before handing the device to a
  student.

There's no custom colour picker by design — every screen's text contrast
against its background has already been checked against WCAG AA for all
six presets, so picking one of the six is all that's needed.

Charcoal doubles as the automatic **night mode** palette: turn on "Night
mode: Auto" on the Status tab and the device switches to Charcoal and a
dimmer backlight every evening between sunset and sunrise, then returns to
whichever colourway you picked once the sun's back up.

## 5. Calibrating touch

Most boards ship correctly calibrated. If taps are landing noticeably off
from where a student is actually touching the screen:

1. Go to the **Status** tab.
2. Press and hold the "STATUS" header (top-left) for about 5 seconds.
3. The screen clears to a single crosshair with "Tap the crosshair"
   underneath — tap exactly on it.
4. A second crosshair appears in the opposite corner — tap that one too.
5. The screen returns to Status automatically; the new calibration is
   saved and survives a power cycle.

If it's still off afterwards, the two crosshairs were probably tapped
imprecisely — just hold the header again and repeat.

## 6. Orientation (USB cable on top or bottom)

If the case is printed with the USB cutout on a different edge than the
ESP32 board's default, set **Orientation** on the settings page to match
("USB on top" or "USB on bottom") — this flips the display and touch
mapping together so taps still land correctly either way.

## Known limits

- Air quality (PSI) is Singapore-specific (data.gov.sg's national 24-hour
  reading) — outside Singapore it will just show "--".
- Bus arrivals are Singapore-specific (LTA DataMall) for the same reason.
- Weather (temperature, rain, UV) works anywhere via Open-Meteo, as long
  as **Location name / Latitude / Longitude** on the settings page are set
  correctly.
- The board has no real-time clock battery backup; after being fully
  unplugged, it re-syncs the time from the internet on its next boot with
  Wi-Fi available, and shows "--:--" until then.
