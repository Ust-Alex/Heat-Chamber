// ============================================================================
// wifi_webserver.cpp - Реализация WiFi, WebSocket и веб-сервера
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h> //V-3.10.0
#include <LittleFS.h>
#include <ESPmDNS.h>

#include "wifi_webserver.h"
#include "web_interface.h"

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
WebSocketsServer webSocket(WEBSOCKET_PORT);
AsyncWebServer server(80);
bool clientConnected = false;

// Внешняя функция из web_interface
extern void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// ============================================================================
// ФОРМИРОВАНИЕ JSON
// ============================================================================
static void buildJSON(const WebData& data, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize,
    "{"
    "\"guild\":%.2f,"
    "\"wall100\":%.2f,"
    "\"wall75\":%.2f,"
    "\"wall50\":%.2f,"
    "\"mode\":%d,"
    "\"color\":%d,"
    "\"time\":\"%s\","
    "\"baseTemp\":%.2f"
    "}",
    data.temps[0], data.temps[1], data.temps[2], data.temps[3],
    data.mode, data.color, data.timeStr, data.baseTemp
  );
}

// ============================================================================
// ОБРАБОТЧИК WEBSOCKET
// ============================================================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  // Вызываем функцию из web_interface
  webSocketEventHandler(num, type, payload, length);
  
  switch (type) {
    case WStype_DISCONNECTED:
      clientConnected = false;
      break;
    case WStype_CONNECTED:
      clientConnected = true;
      break;
    default:
      break;
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ВЕБ-СЕРВЕРА
// ============================================================================
void initWebServer() {
  if (!LittleFS.begin()) {
    Serial.println("[WEB] ❌ Ошибка монтирования LittleFS!");
    return;
  }
  Serial.println("[WEB] ✅ LittleFS смонтирована");

  // ИСПРАВЛЕНО: используем WebRequestMethod::HTTP_GET для новой библиотеки
  server.on("/", WebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.serveStatic("/", LittleFS, "/");
  server.begin();
  Serial.println("[WEB] ✅ Сервер запущен на порту 80");
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ WEBSOCKET
// ============================================================================
void initWebSocket() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.printf("[WiFi] WebSocket сервер запущен на порту %d\n", WEBSOCKET_PORT);
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ MDNS
// ============================================================================
void initMDNS() {
  if (MDNS.begin("heatchamber")) {
    Serial.println("[mDNS] ✅ http://heatchamber.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[mDNS] ❌ Ошибка");
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ WIFIMANAGER
// ============================================================================
void initWiFiManager() {
  WiFiManager wm;
  
  wm.setConnectTimeout(30);
  wm.setConfigPortalTimeout(180);
  
  Serial.println("[WiFi] Попытка подключения к сохранённой сети...");
  
  if (!wm.autoConnect("HeatChamber")) {
    Serial.println("[WiFi] ❌ Ошибка подключения. Перезагрузка...");
    ESP.restart();
  }

  Serial.println("[WiFi] ✅ Подключено!");
  Serial.printf("[WiFi] SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("[WiFi] IP адрес: %s\n", WiFi.localIP().toString().c_str());
}

// ============================================================================
// ОТПРАВКА ДАННЫХ
// ============================================================================
void broadcastData(const WebData& data) {
  if (!clientConnected) return;
  
  char buffer[200];
  buildJSON(data, buffer, sizeof(buffer));
  webSocket.broadcastTXT(buffer);
}

bool hasWebClients() {
  return clientConnected;
}

// ============================================================================
// ЗАДАЧА FREERTOS
// ============================================================================
void taskWiFi(void* pvParameters) {
  Serial.println("[WiFi] Задача запущена");

  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  initWiFiManager();
  initMDNS();
  initWebServer();
  initWebSocket();

  TickType_t lastWake = xTaskGetTickCount();
  uint32_t lastButtonCheck = 0;

  while (1) {
    webSocket.loop();

    uint32_t now = millis();
    if (now - lastButtonCheck > 100) {
      lastButtonCheck = now;
      
      if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
          Serial.println("[WiFi] Сброс настроек по кнопке");
          WiFiManager wm;
          wm.resetSettings();
          ESP.restart();
        }
      }
    }

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(50));
  }
}