// ============================================================================
// web_interface.cpp - Обработка команд с веба
// ============================================================================

#include "web_interface.h"
#include <ArduinoJson.h>
#include <cstdint>
#include <cstddef>
#include <WebSocketsServer.h>
#include "web_server.h"

extern unsigned long heaterSeconds;

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
        payload[length] = '\0';
        Serial.printf("[WEB] Получена команда: %s\n", (char*)payload);

        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.printf("[WEB] Ошибка парсинга JSON: %s\n", error.c_str());
          break;
        }

        const char* command = doc["command"];

        if (!command) {
          Serial.println("[WEB] Нет поля 'command' в JSON");
          break;
        }

        // Команда запроса истории
        if (strcmp(command, "getHistory") == 0) {
          Serial.println("[WEB] Запрос истории");
          sendFullHistory(num);
        }
        else if (strcmp(command, "setPower") == 0) {
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
        } else if (strcmp(command, "resetTimer") == 0) {
          heaterSeconds = 0;
          Serial.println(F("[WEB] Таймер нагрева сброшен"));
        } else if (strcmp(command, "ping") == 0) {
          // Игнорируем ping
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
  strncpy(data.timeStr, timeStr, 8);
  data.timeStr[8] = '\0';
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