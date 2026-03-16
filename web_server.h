// ============================================================================
// web_server.h - Веб-сервер и WebSocket
// ============================================================================

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

#define WEBSOCKET_PORT 8080
#define WEB_SERVER_PORT 80

// Структура для передачи данных
struct WebData {
  float temps[3];      // температуры датчиков
  float target;         // уставка
  int state;            // состояние (0/1)
  char timeStr[6];      // время ЧЧ:ММ
  float power;          // мощность в процентах (ДОБАВИТЬ)
  uint32_t duty;        // значение ШИМ (ДОБАВИТЬ)
};

void initWebServer();
void initWebSocket();
void initMDNS();
void broadcastData(const WebData& data);
bool hasWebClients();
void taskWebServer(void* pvParameters);

#endif