// ============================================================================
// display_ui.h - Интерфейс пользователя на дисплее
// ============================================================================

#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

// Отрисовка всего интерфейса: время, статус, температуры, графики
void drawInterface(
  U8G2* u8g2,
  unsigned long startMillis,
  float targetTemp,
  bool systemState,
  float* sensorTemps,
  int sensorCount,
  int* targetHistory,
  float sensorHistory[][GRAPH_WIDTH],
  int numSensors
);

#endif