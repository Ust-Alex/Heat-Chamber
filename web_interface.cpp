// ============================================================================
// web_interface.cpp - Обработка команд с веба
// ============================================================================
// Проект: Heat-Chamber
// ============================================================================

#include "web_interface.h"
#include <ArduinoJson.h>

static WebCallbacks userCallbacks;
static bool hasClient = false;
extern WebSocketsServer webSocket;

// ============================================================================
// ОБРАБОТЧИК WEBSOCKET
// ============================================================================
void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      hasClient = false;
      break;
      
    case WStype_CONNECTED:
      hasClient = true;
      break;
      
    case WStype_TEXT:
      // Команды обработаем позже
      Serial.printf("[WEB] Команда: %s\n", payload);
      break;
      
    default:
      break;
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
void initWebInterface(const WebCallbacks& callbacks) {
  userCallbacks = callbacks;
  Serial.println(F("[WEB] Интерфейс готов"));
}

// ============================================================================
// ОТПРАВКА ДАННЫХ
// ============================================================================
void sendWebData(float t0, float t1, float t2, float target, bool state, const char* timeStr) {
  if (!hasClient) return;
  
  WebData data;
  data.temps[0] = t0;
  data.temps[1] = t1;
  data.temps[2] = t2;
  data.target = target;
  data.state = state ? 1 : 0;
  strncpy(data.timeStr, timeStr, 5);
  data.timeStr[5] = '\0';
  
  broadcastData(data);
}

// ============================================================================
// ПРОВЕРКА КЛИЕНТОВ
// ============================================================================
bool isWebClientConnected() {
  return hasClient;
}