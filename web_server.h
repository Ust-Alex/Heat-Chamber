// ============================================================================
// web_server.h - Веб-сервер и WebSocket
// Используется ESPAsyncWebServer версии 3.10.1
// ============================================================================

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <time.h>
#include "config.h"

#define WEBSOCKET_PORT 8080
#define WEB_SERVER_PORT 80

// Структура для передачи данных
struct WebData {
  float temps[3];
  float target;
  int state;
  char timeStr[6];
  float power;
  uint32_t duty;
};

// Структура для хранения точки истории
struct HistoryPoint {
  float sensor0;
  float sensor1;
  float sensor2;
  float target;
  time_t timestamp;
};

void initWebServer();
void initWebSocket();
void initMDNS();
void broadcastData(const WebData& data);
bool hasWebClients();
void taskWebServer(void* pvParameters);

// Функции для истории и NTP
void addHistoryPoint(float s0, float s1, float s2, float tgt);
void sendFullHistory(uint8_t clientNum);
void initNTP();
String getCurrentTimeString();

#endif