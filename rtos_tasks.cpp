// ============================================================================
// rtos_tasks.cpp - ДИСПЕТЧЕР ЗАДАЧ FreeRTOS
// ============================================================================

#include "rtos_tasks.h"
#include "web_server.h"
#include "config.h"
#include "settings.h"
#include "task_display.h"
#include "task_sensors.h"
#include "task_heater.h"
#include "task_serial.h"
#include "wifi_keepalive.h"

// ============================================================================
// ВНЕШНИЕ ПЕРЕМЕННЫЕ
// ============================================================================
extern U8G2_ST7920_128X64_F_HW_SPI u8g2;
extern OneWire oneWire;
extern DallasTemperature sensors;
extern DeviceAddress sensorAddresses[MAX_SENSORS];
extern uint8_t sensorCount;
extern float sensorTemps[MAX_SENSORS];
extern float targetTemp;
extern bool systemState;
extern unsigned long startMillis;
extern int targetHistory[GRAPH_WIDTH];
extern float sensorHistory[MAX_SENSORS][GRAPH_WIDTH];
extern uPID PIDregulator;
extern unsigned long lastGraphShift;
extern unsigned long lastGraphDraw;
extern unsigned long lastTempUpdate;
extern unsigned long lastPIDUpdate;
extern float currentPower;

// ============================================================================
// ДЕСКРИПТОРЫ ЗАДАЧ
// ============================================================================
extern TaskHandle_t buttonsTaskHandle;
extern TaskHandle_t webTaskHandle;

// ============================================================================
// ФУНКЦИЯ СОЗДАНИЯ ВСЕХ ЗАДАЧ
// ============================================================================
void createAllTasks(
  GButton* btnUp,
  GButton* btnDown,
  GButton* btnPower,
  float* targetTempPtr,
  bool* systemStatePtr,
  unsigned long* startMillisPtr) {

  Serial.println(F("\n[RTOS] Creating all tasks..."));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 1: КНОПКИ
  // --------------------------------------------------------------------------
  Serial.print(F("   Button task ... "));
  buttonsTaskHandle = createButtonsTask(
    btnUp, btnDown, btnPower,
    targetTempPtr, systemStatePtr, startMillisPtr,
    MAX_TEMP,
    [](float newTarget) { PIDregulator.setpoint = newTarget; });
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 2: ДИСПЛЕЙ
  // --------------------------------------------------------------------------
  Serial.print(F("   Display task ... "));
  xTaskCreatePinnedToCore(taskDisplay, "displayTask", 4096, NULL, 2, NULL, 0);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 3: WiFi/ВЕБ
  // --------------------------------------------------------------------------
  Serial.print(F("   WiFi task ... "));
  xTaskCreatePinnedToCore(taskWebServer, "webTask", 8192, NULL, 3, &webTaskHandle, 1);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 4: ДАТЧИКИ
  // --------------------------------------------------------------------------
  Serial.print(F("   Sensor task ... "));
  xTaskCreatePinnedToCore(taskSensors, "sensorTask", 4096, NULL, 3, NULL, 1);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 5: НАГРЕВ
  // --------------------------------------------------------------------------
  Serial.print(F("   Heater task ... "));
  xTaskCreatePinnedToCore(taskHeaterControl, "heaterTask", 4096, NULL, 3, NULL, 1);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 6: SERIAL
  // --------------------------------------------------------------------------
  Serial.print(F("   Serial task ... "));
  xTaskCreatePinnedToCore(taskSerialCommands, "serialTask", 4096, NULL, 1, NULL, 0);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 7: WiFi KEEPALIVE (если файл добавлен)
  // --------------------------------------------------------------------------
  #ifdef WIFI_KEEPALIVE_H
  extern TaskHandle_t wifiKeepaliveTaskHandle;
  extern void taskWiFiKeepalive(void*);
  Serial.print(F("   WiFi keepalive task ... "));
  xTaskCreatePinnedToCore(taskWiFiKeepalive, "wifiKeepalive", 2048, NULL, 1, &wifiKeepaliveTaskHandle, 0);
  Serial.println(F("OK"));
  #endif

  Serial.println(F("[RTOS] All tasks created"));
}