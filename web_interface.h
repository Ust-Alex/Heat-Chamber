// ============================================================================
// web_interface.h - Интерфейс для команд с веба
// ============================================================================
// Проект: Heat-Chamber
// ============================================================================

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <functional>
#include <WebSocketsServer.h>
#include "web_server.h"

// ============================================================================
// СТРУКТУРА КОЛБЭКОВ
// ============================================================================
struct WebCallbacks {
  std::function<void(float)> onSetTarget;             // изменение уставки
  std::function<void(bool)> onSetPower;               // вкл/выкл
  std::function<void(float, float, float)> onSetPID;  // коэффициенты ПИД
  std::function<void()> onReset;                      // перезагрузка
};

// ============================================================================
// ФУНКЦИИ
// ============================================================================
void initWebInterface(const WebCallbacks& callbacks);
void sendWebData(float t0, float t1, float t2, float target, bool state,
                 const char* timeStr, float power, uint32_t duty);
void webSocketEventHandler(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
bool isWebClientConnected();

#endif  // WEB_INTERFACE_H