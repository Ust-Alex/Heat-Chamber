// ============================================================================
// rtos_buttons.h - Задача FreeRTOS для обработки кнопок
// ============================================================================

#ifndef RTOS_BUTTONS_H
#define RTOS_BUTTONS_H

#include <Arduino.h>
#include <GyverButton.h>

// Структура для передачи данных в задачу
struct ButtonTaskData {
  GButton* btnUp;
  GButton* btnDown;
  GButton* btnPower;
  float* targetTemp;        // указатель на целевую температуру
  bool* systemState;         // указатель на состояние системы
  unsigned long* startMillis; // указатель на время старта
  float maxTemp;             // максимальная температура (из config.h)
  void (*onTargetChanged)(float); // колбэк при изменении targetTemp
};

// Создание задачи обработки кнопок
// Возвращает дескриптор задачи
TaskHandle_t createButtonsTask(
  GButton* btnUp,
  GButton* btnDown,
  GButton* btnPower,
  float* targetTemp,
  bool* systemState,
  unsigned long* startMillis,
  float maxTemp,
  void (*onTargetChanged)(float)
);

#endif