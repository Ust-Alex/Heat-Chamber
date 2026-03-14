// ============================================================================
// rtos_buttons.cpp - Реализация задачи обработки кнопок
// ============================================================================

#include "rtos_buttons.h"

// ============================================================================
// ФУНКЦИЯ ЗАДАЧИ (статическая, видна только в этом файле)
// ============================================================================
static void buttonsTask(void* pvParameters) {
  ButtonTaskData* data = (ButtonTaskData*)pvParameters;
  
  for (;;) {
    // Обработка состояний кнопок
    data->btnUp->tick();
    data->btnDown->tick();
    data->btnPower->tick();

    // Кнопка питания
    if (data->btnPower->isClick()) {
      *(data->systemState) = !(*(data->systemState));
      if (*(data->systemState)) {
        *(data->startMillis) = millis();  // сброс таймера при включении
      }
    }

    // Кнопка "+"
    if (data->btnUp->isClick() && *(data->targetTemp) < data->maxTemp) {
      *(data->targetTemp) += 1.0;
      if (data->onTargetChanged) {
        data->onTargetChanged(*(data->targetTemp));
      }
    }

    // Кнопка "-"
    if (data->btnDown->isClick() && *(data->targetTemp) > 0) {
      *(data->targetTemp) -= 1.0;
      if (data->onTargetChanged) {
        data->onTargetChanged(*(data->targetTemp));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ============================================================================
// СОЗДАНИЕ ЗАДАЧИ
// ============================================================================
TaskHandle_t createButtonsTask(
  GButton* btnUp,
  GButton* btnDown,
  GButton* btnPower,
  float* targetTemp,
  bool* systemState,
  unsigned long* startMillis,
  float maxTemp,
  void (*onTargetChanged)(float)
) {
  // Выделяем память под структуру с данными
  ButtonTaskData* data = new ButtonTaskData;
  
  data->btnUp = btnUp;
  data->btnDown = btnDown;
  data->btnPower = btnPower;
  data->targetTemp = targetTemp;
  data->systemState = systemState;
  data->startMillis = startMillis;
  data->maxTemp = maxTemp;
  data->onTargetChanged = onTargetChanged;

  TaskHandle_t handle;
  
  xTaskCreatePinnedToCore(
    buttonsTask,      // функция задачи
    "buttonsTask",    // имя
    2048,             // размер стека
    data,             // параметры
    1,                // приоритет
    &handle,          // дескриптор
    0                 // ядро 0
  );
  
  return handle;
}