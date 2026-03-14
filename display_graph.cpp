// ============================================================================
// display_graph.cpp - Реализация отрисовки графиков
// ============================================================================

#include "display_graph.h"

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ БУФЕРОВ
// ============================================================================
void initGraphBuffers(int* targetHist, float sensorHist[][GRAPH_WIDTH],
                      float targetTemp, int numSensors) {
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    targetHist[i] = (int)targetTemp;
    for (int j = 0; j < numSensors; j++) {
      sensorHist[j][i] = 0;
    }
  }
}

// ============================================================================
// СДВИГ ГРАФИКОВ ВЛЕВО
// ============================================================================
void shiftGraphsLeft(int* targetHist, float sensorHist[][GRAPH_WIDTH],
                     float targetTemp, float* sensorTemps, int numSensors) {
  
  // Сдвиг истории уставки
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
    targetHist[i] = targetHist[i + 1];
  }
  targetHist[GRAPH_WIDTH - 1] = (int)targetTemp;

  // Сдвиг истории датчиков
  for (int j = 0; j < numSensors; j++) {
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
      sensorHist[j][i] = sensorHist[j][i + 1];
    }
    sensorHist[j][GRAPH_WIDTH - 1] = sensorTemps[j];
  }
}

// ============================================================================
// ОТРИСОВКА ВСЕХ ГРАФИКОВ
// ============================================================================
void drawAllGraphs(U8G2* u8g2, int* targetHist, float sensorHist[][GRAPH_WIDTH],
                   int numSensors) {
  
  for (int x = 0; x < GRAPH_WIDTH; x++) {
    int displayX = x + GRAPH_START_X;

    // Уставка (рисуем всегда)
    int y = map(targetHist[x], GRAPH_TEMP_MIN, GRAPH_TEMP_MAX, 63, 0);
    y = constrain(y, 0, 63) + 10;
    u8g2->drawPixel(displayX, y);

    // Датчики
    for (int j = 0; j < numSensors; j++) {
      float temp = sensorHist[j][x];
      if (!isnan(temp)) {
        y = map((int)temp, GRAPH_TEMP_MIN, GRAPH_TEMP_MAX, 63, 0);
        y = constrain(y, 0, 63) + 10;
        u8g2->drawPixel(displayX, y);
      }
    }
  }
}