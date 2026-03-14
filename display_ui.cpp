// ============================================================================
// display_ui.cpp - Реализация интерфейса пользователя
// ============================================================================

#include "display_ui.h"
#include "display_graph.h"  // для drawAllGraphs

// ============================================================================
// ОТРИСОВКА ИНТЕРФЕЙСА
// ============================================================================
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
) {
  u8g2->clearBuffer();

  // --------------------------------------------------------------------------
  // 1. Время работы системы
  // --------------------------------------------------------------------------
  u8g2->setFont(u8g2_font_6x10_tf);
  unsigned long elapsed = millis() - startMillis;
  uint8_t hours = (elapsed / 3600000) % 100;
  uint8_t minutes = (elapsed / 60000) % 60;
  uint8_t seconds = (elapsed / 1000) % 60;
  
  char timeStr[9];
  sprintf(timeStr, "%02u:%02u:%02u", hours, minutes, seconds);
  u8g2->drawStr(80, 8, timeStr);

  // --------------------------------------------------------------------------
  // 2. Целевая температура и статус
  // --------------------------------------------------------------------------
  u8g2->setFont(u8g2_font_gb24st_t_3);
  char tempStr[10];
  dtostrf(targetTemp, 2, 0, tempStr);
  u8g2->drawStr(0, 15, tempStr);

  u8g2->setFont(u8g2_font_6x10_tf);
  u8g2->drawStr(25, 8, systemState ? "===ON===" : "   OFF");

  // --------------------------------------------------------------------------
  // 3. Температуры с датчиков
  // --------------------------------------------------------------------------
  u8g2->setFont(u8g2_font_gb24st_t_3);
  for (uint8_t i = 0; i < min(sensorCount, numSensors); i++) {
    if (!isnan(sensorTemps[i])) {
      dtostrf(sensorTemps[i], 4, 1, tempStr);
      u8g2->drawStr(0, 15 * (i + 2), tempStr);
    } else {
      u8g2->drawStr(0, 15 * (i + 2), "Err");
    }
  }

  // --------------------------------------------------------------------------
  // 4. Графики (из модуля display_graph)
  // --------------------------------------------------------------------------
  drawAllGraphs(u8g2, targetHistory, sensorHistory, numSensors);

  u8g2->sendBuffer();
}