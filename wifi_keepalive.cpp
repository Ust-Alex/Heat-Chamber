// ============================================================================
// wifi_keepalive.cpp - Отдельная задача для поддержания WiFi
// ============================================================================
// Создано для: Heat-Chamber
// Задача: проверять WiFi каждые 30 секунд и переподключать при потере
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "wifi_static.h"
#include "wifi_keepalive.h"

// ============================================================================
// ДЕСКРИПТОР ЗАДАЧИ
// ============================================================================
TaskHandle_t wifiKeepaliveTaskHandle = NULL;

// ============================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: ПОПЫТКА ПЕРЕПОДКЛЮЧЕНИЯ
// ============================================================================
void checkWiFiAndReconnect() {
  if (WiFi.status() == WL_CONNECTED) {
    return; // Всё хорошо, ничего не делаем
  }

  Serial.println(F("\n[WiFiKeepalive] СВЯЗЬ ПОТЕРЯНА!"));
  Serial.print(F("[WiFiKeepalive] Текущий статус: "));
  Serial.println(WiFi.status());
  Serial.println(F("[WiFiKeepalive] Пытаюсь переподключиться..."));

  WiFi.disconnect();
  delay(100);
  WiFi.reconnect();

  // Ждём результата (до 10 секунд)
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(F("."));
    attempts++;
  }
  Serial.println(F(""));

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WiFiKeepalive] ✅ ПОДКЛЮЧЕНО! IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("[WiFiKeepalive] ❌ НЕ УДАЛОСЬ. Попробую позже."));
  }
}

// ============================================================================
// ЗАДАЧА: ПОДДЕРЖАНИЕ WiFi
// ============================================================================
void taskWiFiKeepalive(void* pvParameters) {
  const unsigned long CHECK_INTERVAL = 30000; // 30 секунд
  unsigned long lastCheck = 0;

  if (DEBUG_SERIAL || DEBUG_WEB) {
    Serial.println(F("[WiFiKeepalive] Задача запущена (интервал 30 сек)"));
  }

  while (1) {
    unsigned long now = millis();

    if (now - lastCheck >= CHECK_INTERVAL) {
      lastCheck = now;
      checkWiFiAndReconnect();
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Проверяем время раз в секунду
  }
}