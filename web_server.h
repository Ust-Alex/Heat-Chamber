// ============================================================================
// web_server.h - Заголовок веб-сервера
// ============================================================================
// Проект: Heat-Chamber
// ============================================================================

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <time.h>
#include "config.h"  // ВАЖНО: для HISTORY_SIZE и других констант

// ============================================================================
// ПРЕДВАРИТЕЛЬНОЕ ОБЪЯВЛЕНИЕ (forward declaration)
// ============================================================================
struct WebData;  // Сообщаем, что такая структура существует

// ============================================================================
// СТРУКТУРА ТОЧКИ ИСТОРИИ
// ============================================================================
struct HistoryPoint {
  float sensor0;
  float sensor1;
  float sensor2;
  float target;
  time_t timestamp;
};

// ============================================================================
// ФУНКЦИИ
// ============================================================================
void initWebServer();
void initWebSocket();
void initMDNS();
void initNTP();
void addHistoryPoint(float s0, float s1, float s2, float tgt);
void sendFullHistory(uint8_t clientNum);
void broadcastData(const WebData& data);  // ← теперь WebData известна как тип
bool hasWebClients();
void taskWebServer(void* pvParameters);
String getCurrentTimeString();

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
extern WebSocketsServer webSocket;
extern AsyncWebServer server;
extern bool clientConnected;
extern HistoryPoint historyBuffer[HISTORY_SIZE];  // ← теперь HISTORY_SIZE известен
extern int historyIndex;
extern int historyCount;
extern bool ntpSyncDone;

#endif  // WEB_SERVER_H