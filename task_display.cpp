#include "task_display.h"
#include "config.h"
#include "display_graph.h"
#include "display_ui.h"

// Внешние переменные из твоего проекта
extern U8G2_ST7920_128X64_F_HW_SPI u8g2;
extern unsigned long startMillis;
extern float targetTemp;
extern bool systemState;
extern float sensorTemps[MAX_SENSORS];
extern uint8_t sensorCount;
extern int targetHistory[GRAPH_WIDTH];
extern float sensorHistory[MAX_SENSORS][GRAPH_WIDTH];
extern unsigned long lastGraphShift;
extern unsigned long lastGraphDraw;

// Для отладки (если нужна)
extern float currentPower;

void taskDisplay(void* pvParameters) {
#if DEBUG_SERIAL
  Serial.println(F("[DISPLAY] Task started"));
#endif

  // Не инициализируем дисплей здесь — он уже в setup()

  while(1) {
    unsigned long now = millis();

    // Сдвиг графиков
    if (now - lastGraphShift >= GRAPH_SHIFT_INTERVAL_MS) {
      lastGraphShift = now;
      shiftGraphsLeft(targetHistory, sensorHistory, targetTemp, sensorTemps, MAX_SENSORS);
    }

    // Отрисовка интерфейса
    if (now - lastGraphDraw >= GRAPH_DRAW_INTERVAL_MS) {
      lastGraphDraw = now;
      drawInterface(
        &u8g2, 
        startMillis, 
        targetTemp, 
        systemState,
        sensorTemps, 
        sensorCount,
        targetHistory, 
        sensorHistory, 
        MAX_SENSORS
      );
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}