#include "settings_server.h"
#include "display.h"



void appendTimezoneOptions(String& page, const String& selectedKey) {
  struct TimezoneGroup {
    const char* label;
    const char* keys[8];
    int count;
  };

  const TimezoneGroup groups[] = {
    {"Global", {"utc"}, 1},
    {"Europe", {"atlantic_azores", "europe_west", "uk", "europe_central", "europe_east"}, 5},
    {"Africa & Middle East", {"africa_south", "middle_east_gulf"}, 2},
    {"Asia", {"singapore", "india", "thailand", "china", "asia_tokyo", "korea"}, 6},
    {"North America", {"us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific", "alaska", "hawaii", "canada_atlantic"}, 8},
    {"South America", {"brazil_east", "argentina"}, 2},
    {"Australia & Oceania", {"australia_perth", "australia_darwin", "australia_sydney", "new_zealand"}, 4}
  };

  for (const TimezoneGroup& group : groups) {
    page += "<optgroup label='";
    page += group.label;
    page += "'>";
    for (int i = 0; i < group.count; i++) {
      const char* key = group.keys[i];
      page += "<option value='";
      page += key;
      page += "'";
      if (selectedKey == key) page += " selected";
      page += ">";
      page += timezoneLabelByKey(key);
      page += "</option>";
    }
    page += "</optgroup>";
  }
}


void appendHomeWidgetOptions(String& page, const String& selectedKey) {
  const HomeWidgetType types[] = {
    HOME_WIDGET_WEEK,
    HOME_WIDGET_TIMER,
    HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR,
    HOME_WIDGET_KP,
    HOME_WIDGET_UV,
    HOME_WIDGET_WIND,
    HOME_WIDGET_SUN,
    HOME_WIDGET_PSI,
    HOME_WIDGET_BUS
  };

  for (HomeWidgetType type : types) {
    const char* key = homeWidgetKey(type);
    page += "<option value='";
    page += key;
    page += "'";
    if (selectedKey == key) page += " selected";
    page += ">";
    page += homeWidgetLabel(type);
    page += "</option>";
  }
}


static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}


static String cssColorFrom565(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}


// Swatch color shown on the settings page is the raw case color (what the
// physical shell actually looks like), not the derived screen background.
static String shellColourPreviewCss(const String& key) {
  const char* k; const char* label; uint16_t swatch;
  for (int i = 0; i < shellColourCount(); i++) {
    shellColourAt(i, &k, &label, &swatch);
    if (key == k) return cssColorFrom565(swatch);
  }
  shellColourAt(0, &k, &label, &swatch);
  return cssColorFrom565(swatch);
}


// =========================================================
// WEB SERVER
// =========================================================
void handleRoot() {
  String shellColour = prefs.getString("shellColour", "muted-white");
  if (!isValidShellColourKey(shellColour)) shellColour = "muted-white";
  String units  = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String tz     = sanitizeTimezoneKey(prefs.getString("tz", "singapore"));
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);
  String orientation = prefs.getString("orientation", "top");
  if (orientation != "bottom") orientation = "top";
  String homeSlotKeys[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeSlotKeys[i] = prefs.getString((String("homeSlot") + String(i)).c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }

  String page;
  page.reserve(21000);

  page += "<!doctype html><html><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>MakerPulse</title>";
  page += "<style>";
  page += ":root{color-scheme:light;}";
  page += "body{margin:0;background:linear-gradient(180deg,#fff4cd 0%,#fffaf0 36%,#ffffff 100%);color:#3a3a3a;font-family:system-ui,sans-serif;}";
  page += "a{color:#b9830a;}";
  page += ".wrap{max-width:980px;margin:0 auto;padding:28px 16px 36px;}";
  page += ".hero{margin-bottom:18px;padding:18px 20px;border:1px solid #ffe3a3;border-radius:20px;background:linear-gradient(135deg,#fff8e6 0%,#ffffff 100%);box-shadow:0 10px 30px rgba(255,182,7,.15);}";
  page += ".hero-brand{display:flex;align-items:center;gap:14px;margin-bottom:10px;}";
  page += ".hero-brand svg{width:40px;height:52px;flex-shrink:0;}";
  page += ".hero-titles{display:flex;flex-direction:column;}";
  page += ".hero-by{font-size:12px;font-weight:700;color:#c79a2e;letter-spacing:.04em;text-transform:uppercase;}";
  page += ".hero h1{font-size:30px;margin:0;color:#262220;}";
  page += ".hero p{margin:0;color:#7a7a7a;font-size:14px;}";
  page += ".ip{display:inline-block;margin-top:14px;padding:8px 12px;border-radius:999px;background:#fff7e0;border:1px solid #ffdf94;color:#8a6d1f;font-size:13px;}";
  page += ".layout{display:grid;grid-template-columns:1.15fr .85fr;gap:16px;align-items:start;}";
  page += ".stack{display:grid;gap:16px;}";
  page += ".panel{background:#ffffff;border:1px solid #f0e6cf;border-radius:18px;padding:18px;margin:0;box-shadow:0 1px 2px rgba(0,0,0,.04);}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;background:none;border:none;color:#262220;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#000000;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#c79a2e;transition:transform .18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px 0;color:#7a7a7a;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px 0;color:#54595f;font-weight:600;}";
  page += "input,select{width:100%;border-radius:12px;border:1px solid #e7decb;background:#fffdf8;color:#262220;padding:12px;box-sizing:border-box;font:inherit;}";
  page += "button{margin-top:18px;background:#ffb607;border:none;color:#241b00;padding:13px 18px;border-radius:12px;font-weight:800;cursor:pointer;font:inherit;}";
  page += "button:hover{background:#e6a400;}";
  page += ".muted{font-size:13px;color:#7a7a7a;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#9a9488;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px solid #f0e6cf;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:none;}";
  page += ".settings-title{display:block;margin:0 0 6px 0;font-size:14px;font-weight:700;color:#262220;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px 0;font-size:12px;color:#7a7a7a;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px 1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#262220;}";
  page += ".color-value{font-size:12px;color:#7a7a7a;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid rgba(0,0,0,.15);cursor:pointer;position:relative;box-sizing:border-box;}";
  page += ".swatch input{display:none;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #61ce70, 0 0 0 5px rgba(97,206,112,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid #e7decb;border-radius:12px;background:#fffdf8;padding:10px 10px 12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#7a7a7a;margin-bottom:8px;font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page += ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#7a7a7a;white-space:nowrap;}";
  page += ".ym-footer{margin-top:22px;text-align:center;font-size:12px;color:#9a9488;}";
  page += ".ym-footer a{color:#c79a2e;font-weight:700;text-decoration:none;}";
  page += ".ym-footer a:hover{text-decoration:underline;}";
  page += "@media(max-width:820px){.layout{grid-template-columns:1fr;}.grid,.grid-3,.timer-slot-grid{grid-template-columns:1fr;}.color-row{grid-template-columns:1fr;}}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<div class='hero-brand'>" YM_LOGO_SVG "<div class='hero-titles'><h1>MakerPulse</h1><span class='hero-by'>by The Young Maker</span></div></div>";
  page += "<p>Shape MakerPulse into your own desk companion with widgets, notes, colors, and smart daily tools.</p>";
  page += "<div class='ip'>ESP IP: ";
  page += WiFi.localIP().toString();
  page += "</div></div>";

  page += "<form method='POST' action='/save'>";
  page += "<div class='layout'><div class='stack'>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Shell colour</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Pick the preset that matches your MakerPulseV2's physical case. The screen's colors are fixed to this choice -- there's no separate accent, text, or custom color picker.</p>";
  page += "<div class='grid'>";

  page += "<div style='grid-column:1 / -1;' class='color-stack'>";
  page += "<div class='color-row'><div class='color-meta'><label class='label'>Shell colour</label><span class='color-value' id='shellColour-value'>";
  page += shellColourLabel(shellColour);
  page += "</span></div><div class='swatch-row'>";
  for (int i = 0; i < shellColourCount(); i++) {
    const char* key; const char* label; uint16_t swatch565;
    shellColourAt(i, &key, &label, &swatch565);
    bool active = (shellColour == key);
    page += "<label class='swatch" + String(active ? " active" : "") + "' data-label='" + label + "' style='background:" + cssColorFrom565(swatch565) + ";'><input type='radio' name='shellColour' value='" + key + "'" + String(active ? " checked" : "") + "></label>";
  }
  page += "</div></div>";
  page += "</div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Settings</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Core behavior and timer setup.</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>General</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>MakerPulse nickname</label><input name='nickname' maxlength='24' value='" + htmlEscape(nickname) + "'></div>";
  page += "<div><label class='label'>Auto sleep interval</label><select name='sleepMin'>";
  page += "<option value='0'"  + String(sleepIntervalMin==0?" selected":"")  + ">Never</option>";
  page += "<option value='1'"  + String(sleepIntervalMin==1?" selected":"")  + ">1 minute</option>";
  page += "<option value='5'"  + String(sleepIntervalMin==5?" selected":"")  + ">5 minutes</option>";
  page += "<option value='10'" + String(sleepIntervalMin==10?" selected":"") + ">10 minutes</option>";
  page += "<option value='30'" + String(sleepIntervalMin==30?" selected":"") + ">30 minutes</option>";
  page += "<option value='60'" + String(sleepIntervalMin==60?" selected":"") + ">1 hour</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>Sleep dims the screen first, then turns it fully off after 60 seconds.</div></div>";
  page += "<div><label class='label'>Measurement system</label><select name='units'>";
  page += "<option value='metric'"   + String(units=="metric"?" selected":"")   + ">Celsius / mm</option>";
  page += "<option value='imperial'" + String(units=="imperial"?" selected":"") + ">Fahrenheit / inches</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Date format</label><select name='region'>";
  page += "<option value='europe'" + String(region=="europe"?" selected":"") + ">European: dd.mm.yyyy</option>";
  page += "<option value='us'" + String(region=="us"?" selected":"") + ">US: mm/dd/yyyy</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Time zone</label><select name='tz'>";
  appendTimezoneOptions(page, tz);
  page += "</select></div>";
  page += "<div><label class='label'>Screen orientation</label><select name='orientation'>";
  page += "<option value='top'"    + String(orientation=="top"?" selected":"")    + ">USB on top</option>";
  page += "<option value='bottom'" + String(orientation=="bottom"?" selected":"") + ">USB on bottom</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>Match this to which edge of your MakerPulse the USB cable comes out of.</div></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>Timer</span><div class='settings-desc'>Choose the six quick timers shown in the popup menu.</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>Slot " + String(i + 1) + "</div><div class='timer-slot-input'><input type='number' min='1' max='180' name='timer" + String(i) + "' value='" + String(timerPresetMin[i]) + "'><span class='timer-unit'>min</span></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:14px;'><span class='settings-title'>Alert behavior</span><label style='display:flex;align-items:center;gap:10px;color:#262220;'><input type='checkbox' name='flashMode' value='1'" + String(flashMode ? " checked" : "") + " style='width:auto;'>Flash screen when timer ends</label></div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Location</span><div class='settings-desc'>Used for weather data and sun times.</div><div class='grid-3'>";
  page += "<div><label class='label'>Location name</label><input name='locname' value='" + htmlEscape(locationName) + "'></div>";
  page += "<div><label class='label'>Latitude</label><input name='lat' value='" + String(LAT, 6) + "'></div>";
  page += "<div><label class='label'>Longitude</label><input name='lng' value='" + String(LNG, 6) + "'></div>";
  page += "</div><div class='footer-note'>Example Singapore: latitude 1.3521, longitude 103.8198.</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Air quality (PSI)</span><div class='settings-desc'>NEA haze reading shown on the Weather page and as a Home widget. No API key needed.</div><div class='grid'>";
  page += "<div><label class='label'>PSI region</label><select name='psiRegion'>";
  page += "<option value='central'" + String(psiRegion=="central"?" selected":"") + ">Central</option>";
  page += "<option value='north'"   + String(psiRegion=="north"?" selected":"")   + ">North</option>";
  page += "<option value='south'"   + String(psiRegion=="south"?" selected":"")   + ">South</option>";
  page += "<option value='east'"    + String(psiRegion=="east"?" selected":"")    + ">East</option>";
  page += "<option value='west'"    + String(psiRegion=="west"?" selected":"")    + ">West</option>";
  page += "</select></div>";
  page += "</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Bus arrival</span><div class='settings-desc'>Shows next bus times for up to " + String(BUS_MAX_STOPS) + " stops, via LTA DataMall. MakerPulse works out of the box using a shared default key, but shared keys can hit rate limits &mdash; for reliable results, request your own free AccountKey at <a href='https://datamall.lta.gov.sg/content/datamall/en/request-for-api.html' target='_blank'>datamall.lta.gov.sg</a> and enter it below. Find each 5-digit bus stop code on the physical bus stop sign (you can also enter a code directly on the device from the Bus tab). The stop name is looked up automatically once a code is saved; use the override field if you'd rather set your own. Leave service number blank to show the soonest bus of any route.</div>";
  page += "<div><label class='label'>LTA AccountKey</label><input name='ltaKey' value='" + htmlEscape(ltaApiKey) + "'></div>";
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    String autoName = busStopDisplayName(i);
    page += "<div class='timer-slot' style='margin-top:12px;'>";
    page += "<div class='timer-slot-head'>Stop " + String(i + 1) + (autoName.length() > 0 ? (" - " + htmlEscape(autoName)) : "") + "</div>";
    page += "<div class='grid-3'>";
    page += "<div><label class='label'>Bus stop code</label><input name='busCode" + String(i) + "' maxlength='10' value='" + htmlEscape(busStops[i].code) + "'></div>";
    page += "<div><label class='label'>Name override</label><input name='busLabel" + String(i) + "' maxlength='30' value='" + htmlEscape(busStops[i].label) + "'></div>";
    page += "<div><label class='label'>Service number</label><input name='busSvc" + String(i) + "' maxlength='10' value='" + htmlEscape(busStops[i].svcFilter) + "'></div>";
    page += "</div></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='widgets'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Widget Customization</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Choose which widgets appear in the four Home slots below the clock card.</p>";
  page += "<div class='grid'>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    page += "<div><label class='label'>";
    page += homeSlotLabel(i);
    page += "</label><select name='homeSlot";
    page += String(i);
    page += "'>";
    appendHomeWidgetOptions(page, homeSlotKeys[i]);
    page += "</select></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "</div><div class='stack'>";

  page += "<button type='submit'>Save to MakerPulse</button>";
  page += "</div></div></form>";
  page += "<div class='ym-footer'>Built by <a href='https://theyoungmaker.com' target='_blank' rel='noopener'>The Young Maker</a> &middot; <a href='https://theyoungmaker.com/3dlab/' target='_blank' rel='noopener'>3D Lab</a></div>";
  page += "<div class='ym-footer'>MakerPulse firmware v" FIRMWARE_VERSION "</div>";
  page += "<script>";
  page += "var panelStorageKey='makerpulse-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "var label=input.closest('.swatch').getAttribute('data-label');";
  page += "if(valueEl&&label){valueEl.textContent=label;}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){return {};}}";
  page += "function writePanelState(state){localStorage.setItem(panelStorageKey,JSON.stringify(state));}";
  page += "function applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',collapsed);var btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function(panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){applyPanelState(panel,!!savedPanelState[panelId]);}";
  page += "});";
  page += "document.querySelectorAll('.panel-toggle').forEach(function(btn){";
  page += "btn.addEventListener('click',function(){";
  page += "var panel=btn.closest('.panel');";
  page += "var collapsed=!panel.classList.contains('collapsed');";
  page += "applyPanelState(panel,collapsed);";
  page += "var state=readPanelState();";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(panelId){state[panelId]=collapsed;writePanelState(state);}";
  page += "});";
  page += "});";
  page += "</script>";
  page += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}


void handleSave() {
  String newShellColour = server.hasArg("shellColour") ? server.arg("shellColour") : shellColourKey;
  if (!isValidShellColourKey(newShellColour)) newShellColour = shellColourKey;
  String newUnits  = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newTz     = server.hasArg("tz") ? server.arg("tz") : timezoneKey;
  String newLoc    = server.hasArg("locname") ? server.arg("locname") : locationName;
  String newNickname = server.hasArg("nickname") ? server.arg("nickname") : deviceNickname;
  HomeWidgetType newHomeSlots[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    String currentKey = homeWidgetKey(homeWidgetSlots[i]);
    newHomeSlots[i] = homeWidgetFromKey(server.hasArg(key) ? server.arg(key) : currentKey);
  }

  float newLat = server.hasArg("lat") ? server.arg("lat").toFloat() : LAT;
  float newLng = server.hasArg("lng") ? server.arg("lng").toFloat() : LNG;

  String newPsiRegion = server.hasArg("psiRegion") ? server.arg("psiRegion") : psiRegion;
  String newLtaKey    = server.hasArg("ltaKey") ? server.arg("ltaKey") : ltaApiKey;
  String newOrientation = server.hasArg("orientation") ? server.arg("orientation") : orientationKey;
  if (newOrientation != "bottom") newOrientation = "top";

  BusStopConfig newBusStops[BUS_MAX_STOPS];
  bool busStopCodeChanged[BUS_MAX_STOPS];
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    String codeKey  = String("busCode") + String(i);
    String labelKey = String("busLabel") + String(i);
    String svcKey   = String("busSvc") + String(i);

    newBusStops[i].code      = server.hasArg(codeKey) ? server.arg(codeKey) : busStops[i].code;
    newBusStops[i].label     = server.hasArg(labelKey) ? server.arg(labelKey) : busStops[i].label;
    newBusStops[i].svcFilter = server.hasArg(svcKey) ? server.arg(svcKey) : busStops[i].svcFilter;

    newBusStops[i].code.trim();
    newBusStops[i].label.trim();
    newBusStops[i].svcFilter.trim();

    if (newBusStops[i].code.length() > 10) newBusStops[i].code = newBusStops[i].code.substring(0, 10);
    if (newBusStops[i].label.length() > 30) newBusStops[i].label = newBusStops[i].label.substring(0, 30);
    if (newBusStops[i].svcFilter.length() > 10) newBusStops[i].svcFilter = newBusStops[i].svcFilter.substring(0, 10);

    busStopCodeChanged[i] = (newBusStops[i].code != busStops[i].code);
  }

  newLoc.trim();
  newNickname.trim();
  newPsiRegion.trim();
  newLtaKey.trim();

  if (newLoc.length() == 0) newLoc = "Unknown";
  if (newNickname.length() > 24) newNickname = newNickname.substring(0, 24);
  if (newUnits != "metric" && newUnits != "imperial") newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us") newRegion = "europe";
  newTz = sanitizeTimezoneKey(newTz);
  newPsiRegion = sanitizePsiRegion(newPsiRegion);

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt() : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged =
    (fabsf(newLat - LAT) > 0.0001f) ||
    (fabsf(newLng - LNG) > 0.0001f) ||
    (newLoc != locationName);
  bool psiRegionChanged = (newPsiRegion != psiRegion);
  bool busSettingsChanged = (newLtaKey != ltaApiKey);
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    if (busStopCodeChanged[i] ||
        newBusStops[i].label != busStops[i].label ||
        newBusStops[i].svcFilter != busStops[i].svcFilter) {
      busSettingsChanged = true;
    }
  }

  deviceNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  timezoneKey = newTz;
  flashModeEnabled = newFlashMode;
  psiRegion = newPsiRegion;
  ltaApiKey = newLtaKey;
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    busStops[i] = newBusStops[i];
    if (busStopCodeChanged[i]) {
      busStopNames[i] = BusStopNameInfo();
      busDataValidByStop[i] = false;
      busServiceCountByStop[i] = 0;
      lastBusFetchByStop[i] = 0;
      busServicePage[i] = 0;
    }
  }
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeWidgetSlots[i] = newHomeSlots[i];
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  prefs.putString("shellColour", newShellColour);
  prefs.putString("units", unitKey);
  prefs.putString("region", regionFormatKey);
  prefs.putString("tz", timezoneKey);
  prefs.putString("nickname", deviceNickname);
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  prefs.putInt("sleepMin", sleepIntervalMin);
  prefs.putBool("flashMode", flashModeEnabled);
  prefs.putString("psiRegion", psiRegion);
  prefs.putString("ltaKey", ltaApiKey);
  prefs.putString("orientation", newOrientation);
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    prefs.putString((String("busCode") + String(i)).c_str(), busStops[i].code);
    prefs.putString((String("busLabel") + String(i)).c_str(), busStops[i].label);
    prefs.putString((String("busSvc") + String(i)).c_str(), busStops[i].svcFilter);
    if (busStopCodeChanged[i]) {
      prefs.putString((String("busName") + String(i)).c_str(), "");
      prefs.putString((String("busRoad") + String(i)).c_str(), "");
    }
  }
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    prefs.putString(key.c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyShellColourByKey(newShellColour);
  applyDeviceTimezoneByKey(timezoneKey);
  applyOrientationByKey(newOrientation); // hot-applies rotation/touch, no reboot needed
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);

  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  lastNowText = "";
  lastHourlyText = "";
  lastPsiText = "";
  lastPsiLevelText = "";
  lastUptimeText = "";

  if (locationChanged) resetDataCaches();
  if (psiRegionChanged) {
    psiValue = NAN;
    lastPsiFetch = 0;
  }
  if (busSettingsChanged) {
    lastBusPageText = "";
  }

  server.sendHeader("Location", "/");
  server.send(303);
}


void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}
