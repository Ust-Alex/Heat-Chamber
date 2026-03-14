// ============================================================================
// web_interface.h - Интерфейс между веб-сервером и основной логикой
// ============================================================================

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <functional>
#include "wifi_webserver.h"

// Структура для обратных вызовов (команды от веб-клиентов)
struct WebCallbacks {
  std::function<void(float)> onSetTarget;        // изменение уставки
  std::function<void(bool)> onSetPower;          // вкл/выкл
  std::function<void(float, float, float)> onSetPID; // установка Kp, Ki, Kd
  std::function<void()> onReset;                  // перезагрузка системы
};

// Инициализация веб-интерфейса с колбэками
void initWebInterface(const WebCallbacks& callbacks);

// Отправка текущих данных всем клиентам
void sendWebData(
  float guildTemp,
  float wall100,
  float wall75,
  float wall50,
  int mode,
  int color,
  const char* timeStr,
  float baseTemp
);

// Отправка подтверждения клиенту
void sendWebAck(uint8_t clientNum, const char* command, bool success);

// Проверка, есть ли подключённые клиенты
bool isWebClientConnected();

#endif