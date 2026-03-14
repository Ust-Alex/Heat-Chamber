// ============================================================================
// web_interface.cpp - Реализация веб-интерфейса
// ============================================================================

#include "web_interface.h"
#include <ArduinoJson.h>

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ
// ============================================================================
static WebCallbacks userCallbacks;
static bool hasClient = false;

// Внешняя ссылка на WebSocket из wifi_webserver
extern WebSocketsServer webSocket;

// ============================================================================
// ПАРСИНГ ВХОДЯЩИХ КОМАНД (JSON)
// ============================================================================
static void processCommand(uint8_t clientNum, const char* json) {
  // ... (код парсинга тот же, что и раньше) ...
}

// ============================================================================
// ОБРАБОТЧИК СОБЫТИЙ WEBSOCKET (ТЕПЕРЬ ПУБЛИЧНЫЙ)
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
      processCommand(num, (char*)payload);
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
}

// ============================================================================
// ОТПРАВКА ДАННЫХ
// ============================================================================
void sendWebData(
  float guildTemp,
  float wall100,
  float wall75,
  float wall50,
  int mode,
  int color,
  const char* timeStr,
  float baseTemp
) {
  if (!hasClient) return;

  WebData data;
  data.temps[0] = guildTemp;
  data.temps[1] = wall100;
  data.temps[2] = wall75;
  data.temps[3] = wall50;
  data.mode = mode;
  data.color = color;
  data.baseTemp = baseTemp;
  strncpy(data.timeStr, timeStr, sizeof(data.timeStr) - 1);
  data.timeStr[5] = '\0';

  broadcastData(data);
}

void sendWebAck(uint8_t clientNum, const char* command, bool success) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer),
    "{\"ack\":\"%s\",\"success\":%s}",
    command, success ? "true" : "false"
  );
  
  webSocket.sendTXT(clientNum, buffer);
}

bool isWebClientConnected() {
  return hasClient;
}