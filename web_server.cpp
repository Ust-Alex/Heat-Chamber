// ============================================================================
// web_server.cpp - Реализация веб-сервера (с историей и NTP)
// Используется ESPAsyncWebServer версии 3.10.1
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <time.h>
#include <cstdint>
#include <cstddef>

#include "web_server.h"
#include "web_interface.h"
#include "wifi_static.h"
#include "config.h"

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
WebSocketsServer webSocket(WEBSOCKET_PORT);
AsyncWebServer server(WEB_SERVER_PORT);
bool clientConnected = false;

extern void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
extern float currentPower;
extern uint32_t currentDuty;

// ============================================================================
// БУФЕР ИСТОРИИ
// ============================================================================
HistoryPoint historyBuffer[HISTORY_SIZE];
int historyIndex = 0;
int historyCount = 0;

// ============================================================================
// NTP ВРЕМЯ
// ============================================================================
time_t now;
struct tm timeinfo;
bool ntpSyncDone = false;
unsigned long lastNTPAttempt = 0;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ NTP
// ============================================================================
void initNTP() {
  if (!isWiFiConnected()) {
    if (DEBUG_NTP) Serial.println(F("⚠️ WiFi не подключён, NTP отложен"));
    return;
  }
  
  if (DEBUG_NTP) Serial.print(F("[NTP] Синхронизация времени... "));
  configTime(TIMEZONE_OFFSET, 0, NTP_SERVER1, NTP_SERVER2);
  if (DEBUG_NTP) Serial.println(F("запущена (фоновая)"));
  ntpSyncDone = false;
  lastNTPAttempt = millis();
}

// ============================================================================
// ПОЛУЧИТЬ ТЕКУЩЕЕ ВРЕМЯ В ВИДЕ СТРОКИ
// ============================================================================
String getCurrentTimeString() {
  static char timeStr[9] = "00:00:00";
  
  if (!ntpSyncDone && isWiFiConnected() && millis() - lastNTPAttempt > 30000) {
    if (getLocalTime(&timeinfo)) {
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
      ntpSyncDone = true;
      if (DEBUG_NTP) Serial.printf("[NTP] Время синхронизировано: %s\n", timeStr);
    }
    lastNTPAttempt = millis();
  } else if (ntpSyncDone) {
    getLocalTime(&timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  }
  
  return String(timeStr);
}

// ============================================================================
// ДОБАВЛЕНИЕ ТОЧКИ В ИСТОРИЮ
// ============================================================================
void addHistoryPoint(float s0, float s1, float s2, float tgt) {
  HistoryPoint& p = historyBuffer[historyIndex];
  p.sensor0 = s0;
  p.sensor1 = s1;
  p.sensor2 = s2;
  p.target = tgt;
  
  if (ntpSyncDone) {
    time_t t = time(nullptr);
    p.timestamp = t;
  } else {
    p.timestamp = millis() / 1000;
  }
  
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) historyCount++;
}

// ============================================================================
// ОТПРАВКА ВСЕЙ ИСТОРИИ КЛИЕНТУ
// ============================================================================
void sendFullHistory(uint8_t clientNum) {
  if (historyCount == 0) return;
  
  DynamicJsonDocument doc(historyCount * 60 + 512);
  JsonArray history = doc.createNestedArray("history");
  
  int startIdx = (historyIndex - historyCount + HISTORY_SIZE) % HISTORY_SIZE;
  
  for (int i = 0; i < historyCount; i++) {
    int idx = (startIdx + i) % HISTORY_SIZE;
    JsonObject point = history.createNestedObject();
    point["sensor0"] = historyBuffer[idx].sensor0;
    point["sensor1"] = historyBuffer[idx].sensor1;
    point["sensor2"] = historyBuffer[idx].sensor2;
    point["target"] = historyBuffer[idx].target;
    point["time"] = historyBuffer[idx].timestamp;
  }
  
  String json;
  serializeJson(doc, json);
  webSocket.sendTXT(clientNum, json);
  
  if (DEBUG_WEB) {
    Serial.printf("[HISTORY] Отправлено %d точек клиенту %u\n", historyCount, clientNum);
  }
}

// ============================================================================
// ФОРМИРОВАНИЕ JSON (С ЗАЩИТОЙ ОТ NaN)
// ============================================================================
static void buildJSON(const WebData& data, char* buffer, size_t bufferSize) {
  String ntpTime = getCurrentTimeString();
  
  // Преобразуем каждый датчик: если NaN — пишем null, иначе число с двумя знаками
  String sensor0Str = isnan(data.temps[0]) ? "null" : String(data.temps[0], 2);
  String sensor1Str = isnan(data.temps[1]) ? "null" : String(data.temps[1], 2);
  String sensor2Str = isnan(data.temps[2]) ? "null" : String(data.temps[2], 2);
  
  snprintf(buffer, bufferSize,
           "{"
           "\"sensor0\":%s,"
           "\"sensor1\":%s,"
           "\"sensor2\":%s,"
           "\"target\":%.2f,"
           "\"state\":%d,"
           "\"time\":\"%s\","
           "\"ntpTime\":\"%s\","
           "\"power\":%.1f,"
           "\"duty\":%u"
           "}",
           sensor0Str.c_str(), 
           sensor1Str.c_str(), 
           sensor2Str.c_str(),
           data.target, data.state, data.timeStr,
           ntpTime.c_str(),
           data.power, data.duty);

  if (DEBUG_WEB) {
    static uint32_t lastDebugTime = 0;
    uint32_t now = millis();
    if (now - lastDebugTime >= 10000) {
      Serial.print(F("[WEB] JSON: "));
      Serial.println(buffer);
      lastDebugTime = now;
    }
  }
}

// ============================================================================
// ОБРАБОТЧИК WEBSOCKET (ИСПРАВЛЕННАЯ ВЕРСИЯ)
// ============================================================================
static void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {

  webSocketEventHandler(num, type, payload, length);

  switch (type) {
    case WStype_DISCONNECTED:
      clientConnected = false;
      if (DEBUG_WEB) Serial.printf("[WEB] Клиент %u отключился\n", num);
      break;

    case WStype_CONNECTED:
      // Защита от множественных подключений
      if (clientConnected && num != 0) {
        if (DEBUG_WEB) Serial.printf("[WEB] Отключаем старого клиента (был клиент 0, новый %u)\n", num);
        webSocket.disconnect(0);
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      
      clientConnected = true;
      if (DEBUG_WEB) Serial.printf("[WEB] Клиент %u подключился\n", num);
      
      vTaskDelay(pdMS_TO_TICKS(50));
      sendFullHistory(num);
      break;

    default:
      break;
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ HTTP-СЕРВЕРА
// ============================================================================
void initWebServer() {
  Serial.println(F("\n========================================"));
  Serial.println(F("🌐 ЗАПУСК ВЕБ-СЕРВЕРА"));
  Serial.println(F("========================================"));

  if (!LittleFS.begin()) {
    Serial.println(F("❌ ОШИБКА LittleFS"));
    return;
  }
  Serial.println(F("✅ LittleFS OK"));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");
  server.begin();
  Serial.println(F("✅ Сервер запущен"));
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ WEBSOCKET
// ============================================================================
void initWebSocket() {
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.printf("✅ WebSocket порт %d\n", WEBSOCKET_PORT);
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ MDNS
// ============================================================================
void initMDNS() {
  if (MDNS.begin("heatchamber")) {
    Serial.println(F("✅ mDNS: http://heatchamber.local"));
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
  } else {
    Serial.println(F("❌ mDNS ошибка"));
  }
}

// ============================================================================
// ОТПРАВКА ДАННЫХ
// ============================================================================
void broadcastData(const WebData& data) {
  if (!clientConnected) return;

  char buffer[192];
  buildJSON(data, buffer, sizeof(buffer));
  webSocket.broadcastTXT(buffer);
}

bool hasWebClients() {
  return clientConnected;
}

// ============================================================================
// ЗАДАЧА FREERTOS
// ============================================================================
void taskWebServer(void* pvParameters) {
  if (DEBUG_SERIAL) Serial.println(F("[TASK] WebServer задача запущена"));

  extern float sensorTemps[MAX_SENSORS];
  extern float targetTemp;
  extern bool systemState;
  extern unsigned long startMillis;
  extern float currentPower;
  extern uint32_t currentDuty;

  uint32_t lastSend = 0;

  auto getTimeString = [](unsigned long start) -> const char* {
    static char timeStr[6] = "00:00";
    if (start == 0) return "00:00";
    unsigned long elapsed = (millis() - start) / 60000;
    unsigned long hours = elapsed / 60;
    unsigned long minutes = elapsed % 60;
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu", hours, minutes);
    return timeStr;
  };

  while (1) {
    webSocket.loop();

    uint32_t now = millis();
    if (now - lastSend >= 1000) {
      if (isWebClientConnected()) {
        sendWebData(
          sensorTemps[0],
          sensorTemps[1],
          sensorTemps[2],
          targetTemp,
          systemState,
          getTimeString(startMillis),
          currentPower,
          currentDuty
        );
      }
      lastSend = now;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}