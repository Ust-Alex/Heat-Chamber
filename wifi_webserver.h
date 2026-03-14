// ============================================================================
// wifi_webserver.h - WiFiManager, WebSocket, mDNS и веб-сервер
// ============================================================================

#ifndef WIFI_WEBSERVER_H
#define WIFI_WEBSERVER_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <functional>

// Константы
#define WEBSOCKET_PORT 8080
#define CONFIG_BUTTON_PIN 0   // GPIO0 (кнопка BOOT)

// Структура с данными для отправки клиентам
struct WebData {
  float temps[4];        // гильза, 100см, 75см, 50см
  int mode;              // режим работы
  int color;             // цветовой статус
  char timeStr[6];       // время в формате ЧЧ:ММ
  float baseTemp;        // базовая температура для режима 2
};

// Функции инициализации
void initWiFiManager();
void initWebServer();
void initWebSocket();
void initMDNS();

// Отправка данных всем подключённым клиентам
void broadcastData(const WebData& data);

// Проверка, есть ли подключённые клиенты
bool hasWebClients();

// Задача FreeRTOS для WiFi
void taskWiFi(void* pvParameters);

#endif