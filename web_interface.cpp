// ============================================================================
// web_interface.cpp - Обработка команд с веба
// ============================================================================
// Проект: Heat-Chamber
// ============================================================================

#include "web_interface.h"
#include <ArduinoJson.h>
#include <cstdint>
#include <cstddef>
#include <WebSocketsServer.h>

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
      Serial.printf("[WEB] Клиент %u отключился\n", num);
      break;

    case WStype_CONNECTED:
      hasClient = true;
      Serial.printf("[WEB] Клиент %u подключился\n", num);
      break;

    case WStype_TEXT:
      {
        // Убеждаемся, что строка завершается нулём
        payload[length] = '\0';
        Serial.printf("[WEB] Получена команда: %s\n", (char*)payload);

        // Парсим JSON
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.printf("[WEB] Ошибка парсинга JSON: %s\n", error.c_str());
          break;
        }

        // Извлекаем команду
        const char* command = doc["command"];

        if (!command) {
          Serial.println("[WEB] Нет поля 'command' в JSON");
          break;
        }

        // Обрабатываем команды
        if (strcmp(command, "setPower") == 0) {
          bool value = doc["value"];
          Serial.printf("[WEB] Команда setPower: %s\n", value ? "ON" : "OFF");

          if (userCallbacks.onSetPower) {
            userCallbacks.onSetPower(value);
          } else {
            Serial.println("[WEB] Ошибка: onSetPower не установлен");
          }
        } else if (strcmp(command, "setTarget") == 0) {
          float value = doc["value"];
          Serial.printf("[WEB] Команда setTarget: %.1f\n", value);

          if (userCallbacks.onSetTarget) {
            userCallbacks.onSetTarget(value);
          } else {
            Serial.println("[WEB] Ошибка: onSetTarget не установлен");
          }
        } else {
          Serial.printf("[WEB] Неизвестная команда: %s\n", command);
        }
        break;
      }

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
void sendWebData(float t0, float t1, float t2, float target, bool state,
                 const char* timeStr, float power, uint32_t duty) {
  if (!hasClient) return;

  WebData data;
  data.temps[0] = t0;
  data.temps[1] = t1;
  data.temps[2] = t2;
  data.target = target;
  data.state = state ? 1 : 0;
  strncpy(data.timeStr, timeStr, 5);
  data.timeStr[5] = '\0';
  data.power = power;
  data.duty = duty;

  broadcastData(data);
}

// ============================================================================
// ПРОВЕРКА КЛИЕНТОВ
// ============================================================================
bool isWebClientConnected() {
  return hasClient;
}