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
// НЕ подключаем web_server.h здесь, чтобы избежать циклических зависимостей

// ============================================================================
// СТРУКТУРА ДАННЫХ ДЛЯ ВЕБА (ПОЛНОЕ ОПРЕДЕЛЕНИЕ)
// ============================================================================
struct WebData {
  float temps[3];
  float target;
  uint8_t state;
  char timeStr[9];        // для "ЧЧ:ММ:СС"
  float power;
  uint32_t duty;
};

// ============================================================================
// СТРУКТУРА КОЛБЭКОВ
// ============================================================================
struct WebCallbacks {
  std::function<void(float)> onSetTarget;
  std::function<void(bool)> onSetPower;
  std::function<void(float, float, float)> onSetPID;
  std::function<void()> onReset;
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