// ============================================================================
// web_server.h - Веб-сервер и WebSocket
// ============================================================================
// Проект: Heat-Chamber
// Версия ESPAsyncWebServer: 3.10.1
// ============================================================================

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>

#include "config.h"

// ============================================================================
// СТРУКТУРА ДАННЫХ ДЛЯ ОТПРАВКИ
// ============================================================================
struct WebData {
  float temps[3];        // [0]-главный, [1]-второй, [2]-третий
  float target;          // целевая температура
  uint8_t state;         // 0-выкл, 1-вкл
  char timeStr[6];       // ЧЧ:ММ
};

// ============================================================================
// ФУНКЦИИ
// ============================================================================
void initWebServer();           // Запуск HTTP-сервера
void initWebSocket();           // Запуск WebSocket
void initMDNS();                // Запуск mDNS
void broadcastData(const WebData& data);  // Отправка данных
bool hasWebClients();           // Проверка клиентов
void taskWebServer(void* pvParameters);   // Задача FreeRTOS

#endif // WEB_SERVER_H