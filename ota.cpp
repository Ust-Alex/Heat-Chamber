/**
 * @file ota.cpp
 * @brief Реализация модуля OTA (Over-The-Air) обновлений
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "ota.h"
#include "config.h"

// ============================================================================
// ПАРАМЕТРЫ ЗАДАЧИ FREERTOS
// ============================================================================
static const uint32_t OTA_TASK_STACK_SIZE = 4096;
static const UBaseType_t OTA_TASK_PRIORITY = 1;
static const BaseType_t OTA_TASK_CORE = 0;

// ============================================================================
// ХЭНДЛ ЗАДАЧИ
// ============================================================================
static TaskHandle_t otaTaskHandle = nullptr;

// ============================================================================
// ТЕЛО ЗАДАЧИ FREERTOS
// ============================================================================
static void taskOTA(void* pvParameters) {
  (void)pvParameters;
  while (1) {
    ArduinoOTA.handle();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ OTA
// ============================================================================
void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "прошивка (sketch)";
    } else {
      type = "файловая система (spiffs)";
    }
    Serial.println("\n🔵 Начинается OTA обновление: " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\n✅ OTA обновление успешно завершено!");
    Serial.println("   ESP32 перезагрузится...");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned int lastPercent = 0;
    unsigned int percent = (progress / (total / 100));
    if (percent != lastPercent) {
      Serial.printf("Прогресс: %u%%\n", percent);
      lastPercent = percent;
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    switch (error) {
      case OTA_AUTH_ERROR:
        Serial.println("\n❌ Ошибка OTA: неверный пароль");
        break;
      case OTA_BEGIN_ERROR:
        Serial.println("\n❌ Ошибка OTA: ошибка начала обновления");
        break;
      case OTA_CONNECT_ERROR:
        Serial.println("\n❌ Ошибка OTA: ошибка подключения");
        break;
      case OTA_RECEIVE_ERROR:
        Serial.println("\n❌ Ошибка OTA: ошибка приема данных");
        break;
      case OTA_END_ERROR:
        Serial.println("\n❌ Ошибка OTA: ошибка завершения обновления");
        break;
      default:
        Serial.println("\n❌ Ошибка OTA: неизвестная ошибка");
        break;
    }
  });
  
  ArduinoOTA.begin();
  
  Serial.println("✅ OTA готов к работе!");
  Serial.println("   Для обновления: Инструменты -> Порт -> выбрать IP-адрес");
}

// ============================================================================
// ОБРАБОТКА OTA (ДЛЯ ВНЕШНЕГО ВЫЗОВА)
// ============================================================================
void handleOTA() {
  ArduinoOTA.handle();
}

// ============================================================================
// СОЗДАНИЕ ЗАДАЧИ FREERTOS
// ============================================================================
void createOTATask() {
  BaseType_t result = xTaskCreatePinnedToCore(
    taskOTA,
    "otaTask",
    OTA_TASK_STACK_SIZE,
    nullptr,
    OTA_TASK_PRIORITY,
    &otaTaskHandle,
    OTA_TASK_CORE
  );
  
  if (result == pdPASS) {
    Serial.println("[OTA] Задача FreeRTOS создана");
  } else {
    Serial.println("[OTA] ОШИБКА: не удалось создать задачу");
  }
}