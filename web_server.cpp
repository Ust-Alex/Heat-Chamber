// ============================================================================
// web_server.cpp - Реализация веб-сервера
// ============================================================================
// Проект: Heat-Chamber
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>

#include "web_server.h"
#include "web_interface.h"

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
WebSocketsServer webSocket(WEBSOCKET_PORT);
AsyncWebServer server(WEB_SERVER_PORT);
bool clientConnected = false;

extern void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

// ============================================================================
// ФОРМИРОВАНИЕ JSON
// ============================================================================
static void buildJSON(const WebData& data, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize,
    "{"
    "\"sensor0\":%.2f,"
    "\"sensor1\":%.2f,"
    "\"sensor2\":%.2f,"
    "\"target\":%.2f,"
    "\"state\":%d,"
    "\"time\":\"%s\""
    "}",
    data.temps[0], data.temps[1], data.temps[2],
    data.target, data.state, data.timeStr
  );
  
  #if DEBUG_SERIAL
  Serial.print(F("[WEB] JSON: ")); Serial.println(buffer);
  #endif
}

// ============================================================================
// ОБРАБОТЧИК WEBSOCKET
// ============================================================================
static void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  
  webSocketEventHandler(num, type, payload, length);
  
  switch (type) {
    case WStype_DISCONNECTED:
      clientConnected = false;
      #if DEBUG_SERIAL
      Serial.printf("[WEB] Клиент %u отключился\n", num);
      #endif
      break;
      
    case WStype_CONNECTED:
      clientConnected = true;
      #if DEBUG_SERIAL
      Serial.printf("[WEB] Клиент %u подключился\n", num);
      #endif
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
  
  // Монтируем LittleFS
  Serial.print(F("📁 LittleFS ... "));
  if (!LittleFS.begin()) {
    Serial.println(F("❌ ОШИБКА"));
    Serial.println(F("   Веб-интерфейс не будет доступен!"));
    return;
  }
  Serial.println(F("✅ OK"));
  
  // Главная страница
  server.on("/", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  
  // Статические файлы
  server.serveStatic("/", LittleFS, "/");
  
  // Запуск
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
  
  char buffer[128];
  buildJSON(data, buffer, sizeof(buffer));
  webSocket.broadcastTXT(buffer);
}

bool hasWebClients() {
  return clientConnected;
}

// ============================================================================
// ЗАДАЧА FREERTOS - ИСПРАВЛЕННАЯ ВЕРСИЯ С ВЫЗОВОМ sendWebData
// ============================================================================
void taskWebServer(void* pvParameters) {
  Serial.println(F("[TASK] WebServer задача запущена"));
  
  // Доступ к глобальным переменным из .ino файла
  extern float sensorTemps[MAX_SENSORS];
  extern float targetTemp;
  extern bool systemState;
  extern unsigned long startMillis;
  
  uint32_t lastSend = 0;
  
  // Вспомогательная функция для формирования строки времени ЧЧ:ММ
  auto getTimeString = [](unsigned long start) -> const char* {
    static char timeStr[6] = "00:00";
    if (start == 0) return "00:00";
    
    unsigned long elapsed = (millis() - start) / 60000; // минуты
    unsigned long hours = elapsed / 60;
    unsigned long minutes = elapsed % 60;
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu", hours, minutes);
    return timeStr;
  };
  
  while (1) {
    webSocket.loop();
    
    uint32_t now = millis();
    // Отправляем данные раз в секунду, если есть клиенты
    if (now - lastSend >= 1000) {
      if (isWebClientConnected()) { // Используем готовую функцию проверки
        // Вызываем готовую функцию отправки с актуальными данными!
        sendWebData(
          sensorTemps[0],      // t0
          sensorTemps[1],      // t1
          sensorTemps[2],      // t2
          targetTemp,          // target
          systemState,         // state
          getTimeString(startMillis) // timeStr
        );
      }
      lastSend = now;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}