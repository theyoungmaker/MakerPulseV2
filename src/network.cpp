#include "network.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>



// =========================================================
// API
// =========================================================
bool fetchSunriseSunset() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.sunrise-sunset.org/json?lat=") + String(LAT, 4) +
               "&lng=" + String(LNG, 4) + "&formatted=0";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body)) return false;

  const char* sunriseStr = doc["results"]["sunrise"];
  const char* sunsetStr  = doc["results"]["sunset"];
  if (!sunriseStr || !sunsetStr) return false;

  auto parseIsoToEpochUTC = [](const char* iso) -> time_t {
    int Y, M, D, h, m, s;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return (time_t)-1;

    struct tm t{};
    t.tm_year = Y - 1900;
    t.tm_mon  = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min  = m;
    t.tm_sec  = s;

    char* oldTz = getenv("TZ");
    String old = oldTz ? String(oldTz) : String("");

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(&t);

    if (old.length()) setenv("TZ", old.c_str(), 1);
    else unsetenv("TZ");
    tzset();

    return epoch;
  };

  time_t srEpoch = parseIsoToEpochUTC(sunriseStr);
  time_t ssEpoch = parseIsoToEpochUTC(sunsetStr);
  if (srEpoch < 0 || ssEpoch < 0) return false;

  dataLock();
  sunriseMin = minutesFromLocalEpoch(srEpoch);
  sunsetMin  = minutesFromLocalEpoch(ssEpoch);
  lastSunYmd = ymdFromLocal(time(nullptr));
  lastSyncTime = time(nullptr);
  dataUnlock();
  return true;
}


void ensureSunTimesForToday() {
  time_t nowT = time(nullptr);
  int ymd = ymdFromLocal(nowT);
  bool needed = (sunriseMin < 0 || sunsetMin < 0 || ymd != lastSunYmd);

  if (needed && WiFi.status() == WL_CONNECTED && fetchBackoffDue(sunBackoff, nowT)) {
    unsigned long t0 = millis();
    bool ok = fetchSunriseSunset();
    fetchBackoffRecord(sunBackoff, time(nullptr), ok, 24UL * 60 * 60);
    Serial.printf("[fetch] sun ok=%d ms=%lu heap=%u\n", ok, millis() - t0, ESP.getFreeHeap());
    if (ok) dataDirty = true;
  }
}


bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LAT, 4) +
               "&longitude=" + String(LNG, 4) +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,wind_direction_10m,uv_index,weather_code,is_day" +
               "&hourly=precipitation,precipitation_probability,temperature_2m,weather_code,is_day" +
               "&daily=temperature_2m_max,temperature_2m_min,uv_index_max,weather_code,precipitation_probability_max" +
               "&forecast_days=" + String(DAILY_COUNT) + "&timezone=auto&wind_speed_unit=ms";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(24576);
  if (deserializeJson(doc, body)) return false;

  // Parse into locals first -- the mutex is held only around the final
  // assignment to shared state, never across the (already-finished) HTTP
  // call or the JSON parse itself.
  float newTempC = doc["current"]["temperature_2m"] | NAN;
  float newApparentTempC = doc["current"]["apparent_temperature"] | NAN;
  float newHumidity = doc["current"]["relative_humidity_2m"] | NAN;
  float newWindSpeedMs = doc["current"]["wind_speed_10m"] | NAN;
  float newWindDirectionDeg = doc["current"]["wind_direction_10m"] | NAN;
  float newUvIndex = doc["current"]["uv_index"] | NAN;
  int newWeatherCode = doc["current"]["weather_code"] | -1;
  bool newWeatherIsDay = (doc["current"]["is_day"] | 1) == 1;
  float newTempMaxC = NAN, newTempMinC = NAN, newUvIndexMax = NAN;

  JsonArray maxTemps = doc["daily"]["temperature_2m_max"];
  JsonArray minTemps = doc["daily"]["temperature_2m_min"];
  JsonArray uvMaxes = doc["daily"]["uv_index_max"];
  if (maxTemps && !maxTemps.isNull() && maxTemps.size() > 0) newTempMaxC = maxTemps[0] | NAN;
  if (minTemps && !minTemps.isNull() && minTemps.size() > 0) newTempMinC = minTemps[0] | NAN;
  if (uvMaxes && !uvMaxes.isNull() && uvMaxes.size() > 0) newUvIndexMax = uvMaxes[0] | NAN;

  JsonArray dailyTimes = doc["daily"]["time"];
  JsonArray dailyCodes = doc["daily"]["weather_code"];
  JsonArray dailyPrecipProbMaxes = doc["daily"]["precipitation_probability_max"];
  int newDailyWeatherCode[DAILY_COUNT];
  float newDailyTempMaxC[DAILY_COUNT];
  int newDailyPrecipProbMax[DAILY_COUNT];
  int newDailyYmd[DAILY_COUNT];
  int newDailyFilled = 0;
  if (dailyTimes) {
    int n = dailyTimes.size();
    if (n > DAILY_COUNT) n = DAILY_COUNT;
    for (int i = 0; i < n; i++) {
      const char* t = dailyTimes[i]; // "YYYY-MM-DD"
      int ymd = -1;
      if (t && strlen(t) >= 10) {
        int y = String(t).substring(0, 4).toInt();
        int m = String(t).substring(5, 7).toInt();
        int d = String(t).substring(8, 10).toInt();
        ymd = y * 10000 + m * 100 + d;
      }
      newDailyYmd[newDailyFilled] = ymd;
      newDailyWeatherCode[newDailyFilled] = (dailyCodes && i < (int)dailyCodes.size()) ? (int)(dailyCodes[i] | -1) : -1;
      newDailyTempMaxC[newDailyFilled] = (i < (int)maxTemps.size()) ? (float)(maxTemps[i] | NAN) : NAN;
      newDailyPrecipProbMax[newDailyFilled] = (dailyPrecipProbMaxes && i < (int)dailyPrecipProbMaxes.size()) ? (int)(dailyPrecipProbMaxes[i] | 0) : 0;
      newDailyFilled++;
    }
  }

  JsonArray times = doc["hourly"]["time"];
  JsonArray precs = doc["hourly"]["precipitation"];
  JsonArray precipProbs = doc["hourly"]["precipitation_probability"];
  JsonArray hourlyTemps = doc["hourly"]["temperature_2m"];
  JsonArray hourlyCodes = doc["hourly"]["weather_code"];
  JsonArray hourlyIsDayArr = doc["hourly"]["is_day"];

  float newPrecipMm = NAN;
  float newHourlyTempC[HOURLY_COUNT];
  int newHourlyCode[HOURLY_COUNT];
  bool newHourlyIsDay[HOURLY_COUNT];
  int newHourlyHour[HOURLY_COUNT];
  int newHourlyPrecipProb[HOURLY_COUNT];
  int newHourlyFilled = 0;

  if (times && precs) {
    time_t nowT = time(nullptr);
    struct tm tmNow;
    localtime_r(&nowT, &tmNow);

    char key[20];
    strftime(key, sizeof(key), "%Y-%m-%dT%H:00", &tmNow);

    int idx = -1;
    for (int i = 0; i < (int)times.size(); i++) {
      const char* t = times[i];
      if (t && String(t).startsWith(key)) {
        idx = i;
        break;
      }
    }
    if (idx < 0) idx = 0;
    newPrecipMm = precs[idx] | NAN;

    if (hourlyTemps && hourlyCodes && hourlyIsDayArr) {
      for (int i = 0; i < HOURLY_COUNT; i++) {
        int srcIdx = idx + i;
        if (srcIdx >= (int)times.size()) break;

        const char* t = times[srcIdx];
        int hourVal = (t && strlen(t) >= 13) ? String(t).substring(11, 13).toInt() : -1;

        newHourlyTempC[newHourlyFilled] = hourlyTemps[srcIdx] | NAN;
        newHourlyCode[newHourlyFilled] = hourlyCodes[srcIdx] | -1;
        newHourlyIsDay[newHourlyFilled] = (hourlyIsDayArr[srcIdx] | 1) == 1;
        newHourlyHour[newHourlyFilled] = hourVal;
        newHourlyPrecipProb[newHourlyFilled] = (precipProbs && srcIdx < (int)precipProbs.size()) ? (int)(precipProbs[srcIdx] | 0) : 0;
        newHourlyFilled++;
      }
    }
  }

  dataLock();
  tempC = newTempC;
  apparentTempC = newApparentTempC;
  humidity = newHumidity;
  windSpeedMs = newWindSpeedMs;
  windDirectionDeg = newWindDirectionDeg;
  uvIndex = newUvIndex;
  uvIndexMax = newUvIndexMax;
  weatherCode = newWeatherCode;
  weatherIsDay = newWeatherIsDay;
  tempMaxC = newTempMaxC;
  tempMinC = newTempMinC;
  precipMm = newPrecipMm;
  hourlyFilled = newHourlyFilled;
  for (int i = 0; i < newHourlyFilled; i++) {
    hourlyTempC[i] = newHourlyTempC[i];
    hourlyCode[i] = newHourlyCode[i];
    hourlyIsDay[i] = newHourlyIsDay[i];
    hourlyHour[i] = newHourlyHour[i];
    hourlyPrecipProb[i] = newHourlyPrecipProb[i];
  }
  dailyFilled = newDailyFilled;
  for (int i = 0; i < newDailyFilled; i++) {
    dailyWeatherCode[i] = newDailyWeatherCode[i];
    dailyTempMaxC[i] = newDailyTempMaxC[i];
    dailyPrecipProbMax[i] = newDailyPrecipProbMax[i];
    dailyYmd[i] = newDailyYmd[i];
  }
  lastWeatherFetch = time(nullptr);
  lastSyncTime = lastWeatherFetch;
  dataUnlock();
  return true;
}


void ensureWeather() {
  time_t nowT = time(nullptr);
  bool needed = isnan(tempC) || isnan(tempMinC) || isnan(tempMaxC) || isnan(precipMm) ||
                isnan(windSpeedMs) || isnan(windDirectionDeg) || isnan(uvIndex) || isnan(uvIndexMax) || isnan(humidity) ||
                (nowT - lastWeatherFetch) > (time_t)WEATHER_INTERVAL_SEC;

  if (needed && WiFi.status() == WL_CONNECTED && fetchBackoffDue(weatherBackoff, nowT)) {
    unsigned long t0 = millis();
    bool ok = fetchWeather();
    fetchBackoffRecord(weatherBackoff, time(nullptr), ok, WEATHER_INTERVAL_SEC);
    Serial.printf("[fetch] weather ok=%d ms=%lu heap=%u\n", ok, millis() - t0, ESP.getFreeHeap());
    if (ok) dataDirty = true;
  }
}


bool fetchPsi() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (datagovApiKey.length() == 0) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://api-open.data.gov.sg/v2/real-time/api/psi")) return false;
  http.addHeader("x-api-key", datagovApiKey);

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  StaticJsonDocument<3072> doc;
  if (deserializeJson(doc, body)) return false;

  JsonArray items = doc["data"]["items"];
  if (!items || items.size() == 0) return false;

  float newPsiValue = items[0]["readings"]["psi_twenty_four_hourly"]["national"] | NAN;
  if (isnan(newPsiValue)) return false;

  dataLock();
  psiValue = newPsiValue;
  lastPsiFetch = time(nullptr);
  lastSyncTime = lastPsiFetch;
  dataUnlock();
  return true;
}


void ensurePsi() {
  time_t nowT = time(nullptr);
  bool needed = isnan(psiValue) || (nowT - lastPsiFetch) > (time_t)PSI_INTERVAL_SEC;

  if (needed && WiFi.status() == WL_CONNECTED && fetchBackoffDue(psiBackoff, nowT)) {
    unsigned long t0 = millis();
    bool ok = fetchPsi();
    fetchBackoffRecord(psiBackoff, time(nullptr), ok, PSI_INTERVAL_SEC);
    Serial.printf("[fetch] psi ok=%d ms=%lu heap=%u\n", ok, millis() - t0, ESP.getFreeHeap());
    if (ok) dataDirty = true;
  }
}


// Spec "Bus" > "Data rules": "parse them to epoch seconds once on fetch
// and store integers. All countdown text is recomputed from those
// integers every 15s, so minutes tick down smoothly between fetches" --
// returns the raw arrival epoch, not a pre-converted "minutes from now"
// (which would go stale the moment real time moves on).
static time_t parseBusArrivalEpoch(const char* arrivalStr, bool& ok) {
  ok = false;
  if (!arrivalStr || strlen(arrivalStr) < 19) return 0;
  struct tm tmArrival = {};
  if (strptime(arrivalStr, "%Y-%m-%dT%H:%M:%S", &tmArrival) == nullptr) return 0;
  ok = true;
  return mktime(&tmArrival);
}


bool fetchBusArrival(int idx) {
  if (idx < 0 || idx >= BUS_MAX_STOPS) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (busStops[idx].code.length() == 0) return false;

  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://datamall2.mytransport.sg/ltaodataservice/v3/BusArrival?BusStopCode=" + busStops[idx].code;
  if (busStops[idx].svcFilter.length() > 0) url += "&ServiceNo=" + busStops[idx].svcFilter;

  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.addHeader("AccountKey", effectiveLtaApiKey());
  http.addHeader("accept", "application/json");

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, body)) return false;

  JsonArray services = doc["Services"];

  // Parse into a local scratch buffer first; only the final copy into the
  // shared busServicesByStop[idx] slots is locked.
  BusServiceInfo newSlots[BUS_MAX_SERVICES];
  int newCount = 0;

  if (services) {
    for (JsonObject svc : services) {
      const char* a1 = svc["NextBus"]["EstimatedArrival"] | "";
      bool ok1;
      time_t e1 = parseBusArrivalEpoch(a1, ok1);
      if (!ok1) continue; // spec: empty NextBus -> row hidden entirely

      bool ok2, ok3;
      const char* a2 = svc["NextBus2"]["EstimatedArrival"] | "";
      const char* a3 = svc["NextBus3"]["EstimatedArrival"] | "";
      time_t e2 = parseBusArrivalEpoch(a2, ok2);
      time_t e3 = parseBusArrivalEpoch(a3, ok3);

      const char* loadStr = svc["NextBus"]["Load"] | "";
      int loadPips = 0;
      if (strcmp(loadStr, "SEA") == 0) loadPips = 1;
      else if (strcmp(loadStr, "SDA") == 0) loadPips = 2;
      else if (strcmp(loadStr, "LSD") == 0) loadPips = 3;

      BusServiceInfo info;
      info.serviceNo = svc["ServiceNo"] | "";
      info.arrival1 = e1;
      info.arrival2 = ok2 ? e2 : 0;
      info.arrival3 = ok3 ? e3 : 0;
      info.monitored = (svc["NextBus"]["Monitored"] | 1) != 0;
      info.loadPips = loadPips;
      info.vehicleType = svc["NextBus"]["Type"] | "";

      int insertAt = (newCount < BUS_MAX_SERVICES) ? newCount : -1;
      for (int j = 0; j < newCount; j++) {
        if (info.arrival1 < newSlots[j].arrival1) { insertAt = j; break; }
      }
      if (insertAt >= 0) {
        int shiftEnd = (newCount < BUS_MAX_SERVICES) ? newCount : BUS_MAX_SERVICES - 1;
        for (int j = shiftEnd; j > insertAt; j--) newSlots[j] = newSlots[j - 1];
        newSlots[insertAt] = info;
        if (newCount < BUS_MAX_SERVICES) newCount++;
      }
    }
  }

  dataLock();
  BusServiceInfo* slots = busServicesByStop[idx];
  for (int i = 0; i < BUS_MAX_SERVICES; i++) {
    slots[i] = (i < newCount) ? newSlots[i] : BusServiceInfo();
  }
  busServiceCountByStop[idx] = newCount;
  busDataValidByStop[idx] = true;
  lastBusFetchByStop[idx] = time(nullptr);
  lastSyncTime = lastBusFetchByStop[idx];
  dataUnlock();
  return true;
}


bool fetchBusStopName(int idx) {
  if (idx < 0 || idx >= BUS_MAX_STOPS) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (busStops[idx].code.length() == 0) return false;

  busStopNames[idx].lastAttempt = time(nullptr);

  WiFiClientSecure client;
  client.setInsecure();

  String url = "https://datamall2.mytransport.sg/ltaodataservice/BusStops?BusStopCode=" + busStops[idx].code;

  HTTPClient http;
  if (!http.begin(client, url)) return false;
  http.addHeader("AccountKey", effectiveLtaApiKey());
  http.addHeader("accept", "application/json");

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, body)) return false;

  JsonArray values = doc["value"];
  if (!values) return false;

  for (JsonObject stop : values) {
    const char* stopCode = stop["BusStopCode"] | "";
    if (busStops[idx].code != stopCode) continue;

    String newRoadName = stop["RoadName"] | "";
    String newDescription = stop["Description"] | "";

    dataLock();
    busStopNames[idx].roadName = newRoadName;
    busStopNames[idx].description = newDescription;
    busStopNames[idx].valid = true;
    dataUnlock();

    prefs.putString((String("busRoad") + String(idx)).c_str(), newRoadName);
    prefs.putString((String("busName") + String(idx)).c_str(), newDescription);
    return true;
  }

  return false;
}


void ensureBusArrival() {
  if (WiFi.status() != WL_CONNECTED) return;

  time_t nowT = time(nullptr);

  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    if (busStops[i].code.length() == 0) continue;
    if (busStopNames[i].valid) continue;
    if ((nowT - busStopNames[i].lastAttempt) < (time_t)BUS_NAME_RETRY_SEC) continue;

    unsigned long t0 = millis();
    bool ok = fetchBusStopName(i);
    Serial.printf("[fetch] busName[%d] ok=%d ms=%lu heap=%u\n", i, ok, millis() - t0, ESP.getFreeHeap());
    if (ok) dataDirty = true;
    return;
  }

  // Spec "Data sources": "30s on Home or Bus, 120s on other pages or when
  // dimmed" -- the fast path only applies to the stop Home/Bus are
  // actually showing (busActiveStopIdx), and only while the screen is on.
  bool dimmed = sleepDimmed || sleepOff;
  bool fastPageVisible = (currentPage == PAGE_HOME || currentPage == PAGE_BUS) && !dimmed;

  int staleIdx = -1;
  time_t staleFetch = 0;
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    if (busStops[i].code.length() == 0) continue;
    bool onScreen = fastPageVisible && (i == busActiveStopIdx);
    uint32_t intervalSec = onScreen ? BUS_FAST_INTERVAL_SEC : BUS_INTERVAL_SEC;
    bool due = (!busDataValidByStop[i] || (nowT - lastBusFetchByStop[i]) > (time_t)intervalSec) &&
               fetchBackoffDue(busBackoff[i], nowT);
    if (!due) continue;
    if (staleIdx < 0 || lastBusFetchByStop[i] < staleFetch) {
      staleIdx = i;
      staleFetch = lastBusFetchByStop[i];
    }
  }

  if (staleIdx >= 0) {
    bool onScreen = fastPageVisible && (staleIdx == busActiveStopIdx);
    uint32_t intervalSec = onScreen ? BUS_FAST_INTERVAL_SEC : BUS_INTERVAL_SEC;
    unsigned long t0 = millis();
    bool ok = fetchBusArrival(staleIdx);
    fetchBackoffRecord(busBackoff[staleIdx], time(nullptr), ok, intervalSec);
    Serial.printf("[fetch] bus[%d] ok=%d ms=%lu heap=%u\n", staleIdx, ok, millis() - t0, ESP.getFreeHeap());
    if (ok) dataDirty = true;
  }
}


static void netTaskFn(void* param) {
  (void)param;
  unsigned long lastReconnectAttemptMs = 0;
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      ensureSunTimesForToday();
      ensureWeather();
      ensurePsi();
      ensureBusArrival();
    } else if (wifiEnabled && storedWifiSsid.length() > 0 && !wifiConnectInProgress) {
      // Spec "Staleness and offline": "Wi-Fi drop: reconnect every 30s in
      // the network task; the UI keeps running on cached data." Before
      // this, a drop after the initial boot-time connect was never
      // retried at all -- only the first connection attempt had any
      // retry/timeout logic (updateWiFiConnectionState(), main loop).
      unsigned long now = millis();
      if (lastReconnectAttemptMs == 0 || now - lastReconnectAttemptMs >= 30000UL) {
        lastReconnectAttemptMs = now;
        beginWiFiConnect();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


void startNetTask() {
  xTaskCreatePinnedToCore(netTaskFn, "net", 8192, nullptr, 1, nullptr, 0);
}


void beginWiFiConnect() {
  if (!wifiEnabled || storedWifiSsid.length() == 0) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiConnectInProgress = false;
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(storedWifiSsid.c_str(), storedWifiPass.c_str());
  wifiConnectInProgress = true;
  wifiConnectStartedMs = millis();
}


void connectWiFi(bool waitForConnection) {
  beginWiFiConnect();
  if (!waitForConnection) return;

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
  }
  wifiConnectInProgress = false;
}


void updateWiFiConnectionState() {
  if (!wifiEnabled || !wifiConnectInProgress) return;

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    wifiConnectInProgress = false;
    // Fetching itself is the net task's job (startNetTask(), polls every
    // 1s) -- it will pick these up on its next pass now that WiFi is up.
    dataDirty = true;
    pageDirty = true;
    return;
  }

  if (millis() - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
    wifiConnectInProgress = false;
    pageDirty = true;
  }
}


