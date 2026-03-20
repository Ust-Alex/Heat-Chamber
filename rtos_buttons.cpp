// ============================================================================
// rtos_buttons.cpp - Реализация задачи обработки кнопок
// ============================================================================

#include "rtos_buttons.h"
#include "settings.h"

// Внешние переменные для сохранения настроек
extern float targetTemp;
extern bool systemState;

// ============================================================================
// ФУНКЦИЯ ЗАДАЧИ
// ============================================================================
static void buttonsTask(void* pvParameters) {
  ButtonTaskData* data = (ButtonTaskData*)pvParameters;
  
  for (;;) {
    data->btnUp->tick();
    data->btnDown->tick();
    data->btnPower->tick();

    // Кнопка питания
    if (data->btnPower->isClick()) {
      *(data->systemState) = !(*(data->systemState));
      if (*(data->systemState)) {
        *(data->startMillis) = millis();  // Включили → таймер стартует
      } else {
        *(data->startMillis) = 0;          // Выключили → таймер сбрасывается
      }
      saveSettings(*(data->targetTemp), *(data->systemState));
    }

    // Кнопка "+"
    if (data->btnUp->isClick() && *(data->targetTemp) < data->maxTemp) {
      *(data->targetTemp) += 1.0;
      if (data->onTargetChanged) {
        data->onTargetChanged(*(data->targetTemp));
      }
      saveSettings(*(data->targetTemp), *(data->systemState));
    }

    // Кнопка "-"
    if (data->btnDown->isClick() && *(data->targetTemp) > 0) {
      *(data->targetTemp) -= 1.0;
      if (data->onTargetChanged) {
        data->onTargetChanged(*(data->targetTemp));
      }
      saveSettings(*(data->targetTemp), *(data->systemState));
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
    buttonsTask,
    "buttonsTask",
    2048,
    data,
    1,
    &handle,
    0
  );
  
  return handle;
}