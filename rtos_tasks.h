// ============================================================================
// rtos_tasks.h - Все задачи FreeRTOS
// ============================================================================
// Приоритеты: 0-низкий, 4-высокий
// Ядро 0: кнопки (prio1), дисплей (prio2)
// Ядро 1: WiFi (prio3), датчики (prio3), нагрев (prio3), ШИМ (prio4)
// ============================================================================

#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H

#include <Arduino.h>
#include "config.h"
#include "pwm.h"
#include "sensors_core.h"
#include "display_ui.h"
#include "display_graph.h"
#include "rtos_buttons.h"
#include "web_server.h"

// ============================================================================
// ВНЕШНИЕ ПЕРЕМЕННЫЕ (из main ino)
// ============================================================================
extern U8G2_ST7920_128X64_F_HW_SPI u8g2;
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

// ============================================================================
// ДЕСКРИПТОРЫ ЗАДАЧ
// ============================================================================
extern TaskHandle_t buttonsTaskHandle;
extern TaskHandle_t webTaskHandle;

// ============================================================================
// ПРОТОТИПЫ ЗАДАЧ
// ============================================================================
void taskDisplay(void* pvParameters);
void taskSensors(void* pvParameters);
void taskHeaterControl(void* pvParameters);
void taskPWM_init(void* pvParameters);

// ============================================================================
// ФУНКЦИЯ СОЗДАНИЯ ВСЕХ ЗАДАЧ
// ============================================================================
void createAllTasks(
  GButton* btnUp, 
  GButton* btnDown, 
  GButton* btnPower,
  float* targetTempPtr,
  bool* systemStatePtr,
  unsigned long* startMillisPtr
);

#endif // RTOS_TASKS_H